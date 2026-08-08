#include <openssl/sha.h>
#include <omp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    [[nodiscard]] std::array<int, 256> make_decode_table()
    {
        std::array<int, 256> table{};
        table.fill(-1);
        for (int index = 0; index < 26; ++index)
        {
            table[static_cast<unsigned char>('A' + index)] = index;
            table[static_cast<unsigned char>('a' + index)] = 26 + index;
        }
        for (int index = 0; index < 10; ++index)
            table[static_cast<unsigned char>('0' + index)] = 52 + index;
        table[static_cast<unsigned char>('+')] = 62;
        table[static_cast<unsigned char>('/')] = 63;
        table[static_cast<unsigned char>('=')] = -2;
        return table;
    }

    const auto decode_table = make_decode_table();

    [[nodiscard]] std::vector<unsigned char> parse_sha256(const std::string& text)
    {
        if (text.size() != 64)
            throw std::runtime_error("target SHA-256 must contain 64 hexadecimal characters");

        const auto nibble = [](const char value) -> int
        {
            if (value >= '0' && value <= '9')
                return value - '0';
            if (value >= 'a' && value <= 'f')
                return 10 + value - 'a';
            if (value >= 'A' && value <= 'F')
                return 10 + value - 'A';
            return -1;
        };

        std::vector<unsigned char> digest(32);
        for (std::size_t index = 0; index < digest.size(); ++index)
        {
            const int high = nibble(text[index * 2]);
            const int low = nibble(text[index * 2 + 1]);
            if (high < 0 || low < 0)
                throw std::runtime_error("target SHA-256 contains a non-hexadecimal character");
            digest[index] = static_cast<unsigned char>((high << 4) | low);
        }
        return digest;
    }

    [[nodiscard]] std::string read_file(const char* path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("unable to open staged payload");
        return { std::istreambuf_iterator<char>(input), {} };
    }

    void write_file(const char* path, const std::string& content)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("unable to create repaired payload");
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!output)
            throw std::runtime_error("unable to write repaired payload");
    }

    void hash_complete_quartet(SHA256_CTX& context, const unsigned char characters[4])
    {
        const int first = decode_table[characters[0]];
        const int second = decode_table[characters[1]];
        const int third = decode_table[characters[2]];
        const int fourth = decode_table[characters[3]];
        if (first < 0 || second < 0 || third < 0 || fourth < 0)
            throw std::runtime_error("staged prefix contains an invalid complete base64 quartet");

        const unsigned char bytes[3]{
            static_cast<unsigned char>((first << 2) | (second >> 4)),
            static_cast<unsigned char>((second << 4) | (third >> 2)),
            static_cast<unsigned char>((third << 6) | fourth)
        };
        SHA256_Update(&context, bytes, sizeof(bytes));
    }
}

int main(const int argument_count, char** arguments)
{
    try
    {
        if (argument_count != 4)
        {
            std::cerr << "usage: repair_single_missing_base64 <corrupt.b64> <sha256> <repaired.b64>\n";
            return 2;
        }

        std::string corrupt = read_file(arguments[1]);
        while (!corrupt.empty() && (corrupt.back() == '\n' || corrupt.back() == '\r'))
            corrupt.pop_back();

        const std::vector<unsigned char> target = parse_sha256(arguments[2]);
        const std::size_t corrupt_length = corrupt.size();
        const std::size_t repaired_length = corrupt_length + 1;
        if ((repaired_length % 4) != 0)
            throw std::runtime_error("staged payload is not missing exactly one base64 character");

        const std::size_t complete_quartets = corrupt_length / 4;
        std::vector<SHA256_CTX> prefix_contexts(complete_quartets + 1);
        SHA256_Init(&prefix_contexts.front());
        for (std::size_t quartet = 0; quartet < complete_quartets; ++quartet)
        {
            prefix_contexts[quartet + 1] = prefix_contexts[quartet];
            unsigned char characters[4]{};
            for (std::size_t index = 0; index < 4; ++index)
            {
                characters[index] = static_cast<unsigned char>(
                    corrupt[quartet * 4 + index]);
            }
            hash_complete_quartet(prefix_contexts[quartet + 1], characters);
        }

        constexpr char alphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/=";

        std::atomic_bool found{ false };
        std::atomic_size_t found_position{};
        std::atomic_int found_character{};
        const double started = omp_get_wtime();

#pragma omp parallel for schedule(dynamic, 8)
        for (long long signed_position = 0;
             signed_position <= static_cast<long long>(corrupt_length);
             ++signed_position)
        {
            if (found.load(std::memory_order_relaxed))
                continue;

            const std::size_t position = static_cast<std::size_t>(signed_position);
            const std::size_t quartet = position / 4;
            const std::size_t begin = quartet * 4;

            for (const unsigned char inserted : alphabet)
            {
                if (inserted == '\0' || found.load(std::memory_order_relaxed))
                    break;

                SHA256_CTX context = prefix_contexts[quartet];
                std::array<unsigned char, 4096> buffer{};
                std::size_t used{};
                bool valid = true;

                const auto flush = [&]()
                {
                    if (used == 0)
                        return;
                    SHA256_Update(&context, buffer.data(), used);
                    used = 0;
                };

                for (std::size_t offset = begin;
                     offset < repaired_length;
                     offset += 4)
                {
                    unsigned char characters[4]{};
                    for (std::size_t index = 0; index < 4; ++index)
                    {
                        const std::size_t candidate_index = offset + index;
                        if (candidate_index < position)
                        {
                            characters[index] = static_cast<unsigned char>(
                                corrupt[candidate_index]);
                        }
                        else if (candidate_index == position)
                        {
                            characters[index] = inserted;
                        }
                        else
                        {
                            characters[index] = static_cast<unsigned char>(
                                corrupt[candidate_index - 1]);
                        }
                    }

                    const int first = decode_table[characters[0]];
                    const int second = decode_table[characters[1]];
                    const int third = decode_table[characters[2]];
                    const int fourth = decode_table[characters[3]];
                    const bool final_quartet = offset + 4 == repaired_length;
                    if (first < 0 || second < 0)
                    {
                        valid = false;
                        break;
                    }

                    if (third == -2)
                    {
                        if (!final_quartet || fourth != -2)
                        {
                            valid = false;
                            break;
                        }
                        buffer[used++] = static_cast<unsigned char>(
                            (first << 2) | (second >> 4));
                    }
                    else
                    {
                        if (third < 0)
                        {
                            valid = false;
                            break;
                        }
                        buffer[used++] = static_cast<unsigned char>(
                            (first << 2) | (second >> 4));
                        buffer[used++] = static_cast<unsigned char>(
                            (second << 4) | (third >> 2));

                        if (fourth == -2)
                        {
                            if (!final_quartet)
                            {
                                valid = false;
                                break;
                            }
                        }
                        else
                        {
                            if (fourth < 0)
                            {
                                valid = false;
                                break;
                            }
                            buffer[used++] = static_cast<unsigned char>(
                                (third << 6) | fourth);
                        }
                    }

                    if (used > buffer.size() - 3)
                        flush();
                }

                if (!valid)
                    continue;

                flush();
                unsigned char digest[SHA256_DIGEST_LENGTH]{};
                SHA256_Final(digest, &context);
                if (std::memcmp(digest, target.data(), target.size()) != 0)
                    continue;

                bool expected = false;
                if (found.compare_exchange_strong(expected, true))
                {
                    found_position.store(position);
                    found_character.store(inserted);
                    std::cout
                        << "repaired position=" << position
                        << " character=" << inserted
                        << " elapsed=" << (omp_get_wtime() - started)
                        << "s\n";
                }
                break;
            }
        }

        if (!found.load())
            throw std::runtime_error("no one-character repair produced the expected SHA-256");

        const std::size_t position = found_position.load();
        const char character = static_cast<char>(found_character.load());
        std::string repaired;
        repaired.reserve(repaired_length);
        repaired.append(corrupt.data(), position);
        repaired.push_back(character);
        repaired.append(corrupt.data() + position, corrupt_length - position);
        write_file(arguments[3], repaired);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "repair failed: " << error.what() << '\n';
        return 1;
    }
}

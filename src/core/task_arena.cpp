#include <epochengine/particle/task_arena.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace epochengine::particle
{
    class TaskArena::Impl
    {
    public:
        explicit Impl(std::size_t worker_count)
        {
            workers_.reserve(worker_count);
            for (std::size_t index = 0; index < worker_count; ++index)
            {
                workers_.emplace_back([this]
                {
                    worker_loop();
                });
            }
        }

        ~Impl()
        {
            {
                std::lock_guard lock(mutex_);
                stopping_ = true;
                ++generation_;
            }
            start_condition_.notify_all();
            for (std::thread& worker : workers_)
            {
                if (worker.joinable())
                    worker.join();
            }
        }

        void parallel_for(
            std::size_t count,
            std::size_t grain_size,
            const std::function<void(std::size_t, std::size_t)>& function)
        {
            std::unique_lock invocation_lock(invocation_mutex_);
            if (count == 0)
                return;

            grain_size = std::max<std::size_t>(grain_size, 1U);
            if (workers_.empty() || count <= grain_size)
            {
                function(0, count);
                return;
            }

            {
                std::lock_guard lock(mutex_);
                function_ = function;
                count_ = count;
                grain_size_ = grain_size;
                next_.store(0, std::memory_order_relaxed);
                remaining_.store(workers_.size() + 1U, std::memory_order_release);
                exception_ = nullptr;
                ++generation_;
            }
            start_condition_.notify_all();

            run_chunks();
            finish_participant();

            std::exception_ptr exception;
            {
                std::unique_lock lock(mutex_);
                done_condition_.wait(lock, [this]
                {
                    return remaining_.load(std::memory_order_acquire) == 0;
                });
                exception = std::exchange(exception_, nullptr);
                function_ = {};
            }
            if (exception)
                std::rethrow_exception(exception);
        }

        [[nodiscard]] std::size_t worker_count() const noexcept
        {
            return workers_.size();
        }

    private:
        void worker_loop()
        {
            std::uint64_t observed_generation = 0;
            for (;;)
            {
                {
                    std::unique_lock lock(mutex_);
                    start_condition_.wait(lock, [this, observed_generation]
                    {
                        return stopping_ || generation_ != observed_generation;
                    });

                    if (stopping_)
                        return;

                    observed_generation = generation_;
                }

                run_chunks();
                finish_participant();
            }
        }

        [[nodiscard]] bool claim_chunk(
            std::size_t& begin,
            std::size_t& end) noexcept
        {
            begin = next_.load(std::memory_order_relaxed);
            while (begin < count_)
            {
                const std::size_t remaining = count_ - begin;
                end = grain_size_ >= remaining ? count_ : begin + grain_size_;
                if (next_.compare_exchange_weak(
                        begin,
                        end,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed))
                {
                    return true;
                }
            }
            return false;
        }

        void run_chunks() noexcept
        {
            try
            {
                std::size_t begin{};
                std::size_t end{};
                while (claim_chunk(begin, end))
                    function_(begin, end);
            }
            catch (...)
            {
                std::lock_guard lock(mutex_);
                if (!exception_)
                    exception_ = std::current_exception();
            }
        }

        void finish_participant() noexcept
        {
            if (remaining_.fetch_sub(1U, std::memory_order_acq_rel) == 1U)
            {
                std::lock_guard lock(mutex_);
                done_condition_.notify_one();
            }
        }

        std::vector<std::thread> workers_;
        std::mutex invocation_mutex_;
        mutable std::mutex mutex_;
        std::condition_variable start_condition_;
        std::condition_variable done_condition_;
        std::function<void(std::size_t, std::size_t)> function_;
        std::exception_ptr exception_;
        std::atomic<std::size_t> next_{};
        std::atomic<std::size_t> remaining_{};
        std::size_t count_{};
        std::size_t grain_size_{ 1 };
        std::uint64_t generation_{};
        bool stopping_{};
    };

    TaskArena::TaskArena(std::size_t worker_count)
        : impl_(std::make_unique<Impl>(worker_count))
    {
    }

    TaskArena::~TaskArena() = default;
    TaskArena::TaskArena(TaskArena&&) noexcept = default;
    TaskArena& TaskArena::operator=(TaskArena&&) noexcept = default;

    void TaskArena::parallel_for(
        std::size_t count,
        std::size_t grain_size,
        const std::function<void(std::size_t, std::size_t)>& function)
    {
        if (impl_)
        {
            impl_->parallel_for(count, grain_size, function);
            return;
        }
        if (count != 0)
            function(0, count);
    }

    std::size_t TaskArena::worker_count() const noexcept
    {
        return impl_ ? impl_->worker_count() : 0U;
    }

    std::size_t TaskArena::recommended_worker_count() noexcept
    {
        const unsigned hardware_threads = std::thread::hardware_concurrency();
        if (hardware_threads <= 1U)
            return 0;
        return std::min<std::size_t>(hardware_threads - 1U, 31U);
    }
}

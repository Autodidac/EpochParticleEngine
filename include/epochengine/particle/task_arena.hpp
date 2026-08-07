#pragma once

#include "export.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

namespace epochengine::particle
{
    class EPOCH_PARTICLE_API TaskArena
    {
    public:
        explicit TaskArena(std::size_t worker_count = recommended_worker_count());
        ~TaskArena();

        TaskArena(const TaskArena&) = delete;
        TaskArena& operator=(const TaskArena&) = delete;
        TaskArena(TaskArena&&) noexcept;
        TaskArena& operator=(TaskArena&&) noexcept;

        void parallel_for(
            std::size_t count,
            std::size_t grain_size,
            const std::function<void(std::size_t, std::size_t)>& function);

        template<class Function>
        void parallel_for(std::size_t count, std::size_t grain_size, Function&& function)
        {
            const std::function<void(std::size_t, std::size_t)> erased(
                std::forward<Function>(function));
            parallel_for(count, grain_size, erased);
        }

        [[nodiscard]] std::size_t worker_count() const noexcept;
        [[nodiscard]] static std::size_t recommended_worker_count() noexcept;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}


include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(EPOCH_PARTICLE_BUILD_SHARED "Build shared libraries instead of static libraries" OFF)
option(EPOCH_PARTICLE_BUILD_VULKAN "Build the Vulkan renderer and interactive lab" ON)
option(EPOCH_PARTICLE_BUILD_EXAMPLES "Build examples" ON)
option(EPOCH_PARTICLE_BUILD_TESTS "Build the deterministic test suite" ON)
option(EPOCH_PARTICLE_BUILD_BENCHMARKS "Build the CPU scene benchmark" OFF)
option(EPOCH_PARTICLE_BUILD_MODULE "Build the experimental C++23 module facade" OFF)
option(EPOCH_PARTICLE_WITH_EPOCHGUI "Use EpochGui layout primitives in the interactive lab" OFF)
option(EPOCH_PARTICLE_WITH_EPOCH_PLATFORM "Build the lab with EpochPlatformEngine's internal entrypoint" ON)
option(EPOCH_PARTICLE_FETCH_EPOCH_PLATFORM "Fetch EpochPlatformEngine when no checkout or package is available" ON)
option(EPOCH_PARTICLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(EPOCH_PARTICLE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
set(EPOCHGUI_SOURCE_DIR "" CACHE PATH "Optional path to an EpochGui source checkout")
set(EPOCH_PLATFORM_SOURCE_DIR "" CACHE PATH "Optional path to an EpochPlatformEngine source checkout")
set(EPOCH_PARTICLE_PLATFORM_GIT_TAG
    "4c5aee6cf419bd163759d71a7d3a3c1e35c66db8"
    CACHE STRING "Pinned EpochPlatformEngine commit used by FetchContent")

find_package(Threads REQUIRED)

if(EPOCH_PARTICLE_BUILD_SHARED)
    set(EPOCH_PARTICLE_LIBRARY_TYPE SHARED)
else()
    set(EPOCH_PARTICLE_LIBRARY_TYPE STATIC)
endif()

set(EPOCH_PARTICLE_PUBLIC_HEADERS
    include/epochengine/particle/epoch_particle_engine.hpp
    include/epochengine/particle/events.hpp
    include/epochengine/particle/export.hpp
    include/epochengine/particle/fixed.hpp
    include/epochengine/particle/hash.hpp
    include/epochengine/particle/material_grid.hpp
    include/epochengine/particle/particle_pool.hpp
    include/epochengine/particle/random.hpp
    include/epochengine/particle/render_frame.hpp
    include/epochengine/particle/screen_space.hpp
    include/epochengine/particle/scene.hpp
    include/epochengine/particle/scenes.hpp
    include/epochengine/particle/simulation.hpp
    include/epochengine/particle/task_arena.hpp
    include/epochengine/particle/text.hpp
    include/epochengine/particle/types.hpp
    include/epochengine/particle/uniform_grid.hpp
    include/epochengine/particle/version.hpp)

set(EPOCH_PARTICLE_CORE_SOURCES
    src/core/fixed.cpp
    src/core/hash.cpp
    src/core/material_grid.cpp
    src/core/particle_pool.cpp
    src/core/random.cpp
    src/core/render_frame.cpp
    src/core/simulation.cpp
    src/core/task_arena.cpp
    src/core/uniform_grid.cpp
    src/scenes/boids.cpp
    src/scenes/cellular_automata.cpp
    src/scenes/default_scenes.cpp
    src/scenes/deterministic_fountain.cpp
    src/scenes/fire_smoke.cpp
    src/scenes/fireworks.cpp
    src/scenes/flow_field.cpp
    src/scenes/galaxy.cpp
    src/scenes/hybrid_sand.cpp
    src/scenes/particle_life.cpp
    src/scenes/particle_studio.cpp
    src/scenes/physarum.cpp
    src/scenes/reaction_diffusion.cpp
    src/scenes/sph_fluid.cpp
    src/scenes/spring_cloth.cpp
    src/scenes/weather.cpp)

function(epoch_particle_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /Zc:__cplusplus /EHsc /utf-8 /fp:precise)
        if(EPOCH_PARTICLE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wdouble-promotion
            -ffp-contract=off)
        if(EPOCH_PARTICLE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(epoch_particle_enable_sanitizers target)
    if(NOT EPOCH_PARTICLE_ENABLE_SANITIZERS)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /fsanitize=address)
        target_link_options(${target} PRIVATE /fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
    endif()
endfunction()

add_library(EpochParticleEngine ${EPOCH_PARTICLE_LIBRARY_TYPE}
    ${EPOCH_PARTICLE_CORE_SOURCES})
add_library(EpochParticleEngine::Core ALIAS EpochParticleEngine)
set_target_properties(EpochParticleEngine PROPERTIES
    EXPORT_NAME Core
    OUTPUT_NAME EpochParticleEngine
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

target_compile_features(EpochParticleEngine PUBLIC cxx_std_23)
target_include_directories(EpochParticleEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(EpochParticleEngine PUBLIC Threads::Threads)
target_sources(EpochParticleEngine
    PUBLIC
        FILE_SET public_headers TYPE HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
        FILES ${EPOCH_PARTICLE_PUBLIC_HEADERS})


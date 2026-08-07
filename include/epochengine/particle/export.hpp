#pragma once

#if defined(_WIN32)
    #if defined(EPOCH_PARTICLE_STATIC)
        #define EPOCH_PARTICLE_API
        #define EPOCH_PARTICLE_VULKAN_API
    #else
        #if defined(EPOCH_PARTICLE_BUILDING_LIBRARY)
            #define EPOCH_PARTICLE_API __declspec(dllexport)
        #else
            #define EPOCH_PARTICLE_API __declspec(dllimport)
        #endif
        #if defined(EPOCH_PARTICLE_VULKAN_BUILDING_LIBRARY)
            #define EPOCH_PARTICLE_VULKAN_API __declspec(dllexport)
        #else
            #define EPOCH_PARTICLE_VULKAN_API __declspec(dllimport)
        #endif
    #endif
#else
    #if defined(EPOCH_PARTICLE_BUILDING_LIBRARY)
        #define EPOCH_PARTICLE_API __attribute__((visibility("default")))
    #else
        #define EPOCH_PARTICLE_API
    #endif
    #if defined(EPOCH_PARTICLE_VULKAN_BUILDING_LIBRARY)
        #define EPOCH_PARTICLE_VULKAN_API __attribute__((visibility("default")))
    #else
        #define EPOCH_PARTICLE_VULKAN_API
    #endif
#endif

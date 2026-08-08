if(EPOCH_PARTICLE_BUILD_SHARED)
    target_compile_definitions(EpochParticleEngine PRIVATE EPOCH_PARTICLE_BUILDING_LIBRARY=1)
    set_target_properties(EpochParticleEngine PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES)
else()
    target_compile_definitions(EpochParticleEngine PUBLIC EPOCH_PARTICLE_STATIC=1)
endif()

epoch_particle_apply_warnings(EpochParticleEngine)
epoch_particle_enable_sanitizers(EpochParticleEngine)

if(EPOCH_PARTICLE_BUILD_MODULE)
    set_property(TARGET EpochParticleEngine PROPERTY CXX_SCAN_FOR_MODULES ON)
    target_sources(EpochParticleEngine PUBLIC
        FILE_SET particle_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
        FILES modules/epoch.particle.ixx)
endif()

set(EPOCH_PARTICLE_INSTALL_TARGETS EpochParticleEngine)

if(EPOCH_PARTICLE_BUILD_VULKAN)
    find_package(Vulkan 1.2 REQUIRED)
    find_package(unofficial-shaderc CONFIG REQUIRED)

    set(EPOCH_PARTICLE_NATIVE_SURFACE_SOURCE)
    set(EPOCH_PARTICLE_NATIVE_SURFACE_LIBRARIES)
    set(EPOCH_PARTICLE_LAB_PLATFORM_SOURCES)

    if(WIN32)
        set(EPOCH_PARTICLE_NATIVE_SURFACE_SOURCE src/vulkan/native_surface_win32.cpp)
        list(APPEND EPOCH_PARTICLE_NATIVE_SURFACE_LIBRARIES user32)
    elseif(APPLE)
        enable_language(OBJCXX)
        set(EPOCH_PARTICLE_NATIVE_SURFACE_SOURCE src/vulkan/native_surface_macos.mm)
        list(APPEND EPOCH_PARTICLE_LAB_PLATFORM_SOURCES app/cocoa_input_bridge.mm)
        find_library(EPOCH_PARTICLE_COCOA_FRAMEWORK Cocoa REQUIRED)
        find_library(EPOCH_PARTICLE_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
        list(APPEND EPOCH_PARTICLE_NATIVE_SURFACE_LIBRARIES
            ${EPOCH_PARTICLE_COCOA_FRAMEWORK}
            ${EPOCH_PARTICLE_QUARTZCORE_FRAMEWORK})
    elseif(UNIX)
        find_package(X11 REQUIRED)
        set(EPOCH_PARTICLE_NATIVE_SURFACE_SOURCE src/vulkan/native_surface_xlib.cpp)
        list(APPEND EPOCH_PARTICLE_NATIVE_SURFACE_LIBRARIES X11::X11)
    else()
        message(FATAL_ERROR
            "EpochParticleEngine Vulkan currently supports Win32, Xlib, and macOS Metal surfaces")
    endif()

    set(EPOCH_PARTICLE_VULKAN_HEADERS
        include/epochengine/particle/vulkan/renderer.hpp
        include/epochengine/particle/vulkan/compute_backend.hpp)
    add_library(EpochParticleEngineVulkan ${EPOCH_PARTICLE_LIBRARY_TYPE}
        src/vulkan/native_surface.hpp
        ${EPOCH_PARTICLE_NATIVE_SURFACE_SOURCE}
        src/vulkan/compute_backend.cpp
        src/vulkan/renderer.cpp)
    add_library(EpochParticleEngine::Vulkan ALIAS EpochParticleEngineVulkan)
    set_target_properties(EpochParticleEngineVulkan PROPERTIES
        EXPORT_NAME Vulkan
        OUTPUT_NAME EpochParticleEngineVulkan
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        POSITION_INDEPENDENT_CODE ON
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
    target_compile_features(EpochParticleEngineVulkan PUBLIC cxx_std_23)
    target_include_directories(EpochParticleEngineVulkan
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan)
    target_sources(EpochParticleEngineVulkan
        PUBLIC
            FILE_SET vulkan_headers TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
            FILES ${EPOCH_PARTICLE_VULKAN_HEADERS})
    target_link_libraries(EpochParticleEngineVulkan
        PUBLIC EpochParticleEngine
        PRIVATE
            Vulkan::Vulkan
            unofficial::shaderc::shaderc
            ${EPOCH_PARTICLE_NATIVE_SURFACE_LIBRARIES})

    if(WIN32)
        target_compile_definitions(EpochParticleEngineVulkan PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
            _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    endif()

    if(EPOCH_PARTICLE_BUILD_SHARED)
        target_compile_definitions(EpochParticleEngineVulkan
            PRIVATE EPOCH_PARTICLE_VULKAN_BUILDING_LIBRARY=1)
        set_target_properties(EpochParticleEngineVulkan PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES)
    else()
        target_compile_definitions(EpochParticleEngineVulkan PUBLIC EPOCH_PARTICLE_STATIC=1)
    endif()

    epoch_particle_apply_warnings(EpochParticleEngineVulkan)
    epoch_particle_enable_sanitizers(EpochParticleEngineVulkan)
    list(APPEND EPOCH_PARTICLE_INSTALL_TARGETS EpochParticleEngineVulkan)

    if(EPOCH_PARTICLE_BUILD_EXAMPLES)
        if(NOT EPOCH_PARTICLE_WITH_EPOCH_PLATFORM)
            message(FATAL_ERROR
                "EpochParticleLab requires EPOCH_PARTICLE_WITH_EPOCH_PLATFORM=ON; "
                "use EPOCH_PARTICLE_BUILD_EXAMPLES=OFF for a renderer-library-only build")
        endif()

        if(EPOCH_PARTICLE_WITH_EPOCHGUI)
            if(EPOCHGUI_SOURCE_DIR)
                if(NOT EXISTS "${EPOCHGUI_SOURCE_DIR}/CMakeLists.txt")
                    message(FATAL_ERROR "EPOCHGUI_SOURCE_DIR does not contain CMakeLists.txt")
                endif()
                if(NOT TARGET EpochGui)
                    add_subdirectory(
                        "${EPOCHGUI_SOURCE_DIR}"
                        "${CMAKE_CURRENT_BINARY_DIR}/epochgui"
                        EXCLUDE_FROM_ALL)
                endif()
            endif()
            if(NOT TARGET EpochGui)
                message(FATAL_ERROR
                    "EPOCH_PARTICLE_WITH_EPOCHGUI requires -DEPOCHGUI_SOURCE_DIR=<EpochGui checkout>")
            endif()
        endif()

        if(NOT TARGET EpochPlatformEngine::Static AND
           NOT TARGET EpochPlatformEngineStatic)
            if(EPOCH_PLATFORM_SOURCE_DIR)
                if(NOT EXISTS "${EPOCH_PLATFORM_SOURCE_DIR}/CMakeLists.txt")
                    message(FATAL_ERROR
                        "EPOCH_PLATFORM_SOURCE_DIR does not contain CMakeLists.txt")
                endif()
                set(EPOCH_PLATFORM_BUILD_STATIC ON CACHE BOOL "" FORCE)
                set(EPOCH_PLATFORM_BUILD_SHARED OFF CACHE BOOL "" FORCE)
                set(EPOCH_PLATFORM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
                set(EPOCH_PLATFORM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
                add_subdirectory(
                    "${EPOCH_PLATFORM_SOURCE_DIR}"
                    "${CMAKE_CURRENT_BINARY_DIR}/epochplatform"
                    EXCLUDE_FROM_ALL)
            elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../EpochPlatformEngine/CMakeLists.txt")
                set(EPOCH_PLATFORM_BUILD_STATIC ON CACHE BOOL "" FORCE)
                set(EPOCH_PLATFORM_BUILD_SHARED OFF CACHE BOOL "" FORCE)
                set(EPOCH_PLATFORM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
                set(EPOCH_PLATFORM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
                add_subdirectory(
                    "${CMAKE_CURRENT_SOURCE_DIR}/../EpochPlatformEngine"
                    "${CMAKE_CURRENT_BINARY_DIR}/epochplatform"
                    EXCLUDE_FROM_ALL)
            else()
                find_package(EpochPlatformEngine CONFIG QUIET)
            endif()
        endif()

        if(NOT TARGET EpochPlatformEngine::Static AND
           NOT TARGET EpochPlatformEngineStatic AND
           EPOCH_PARTICLE_FETCH_EPOCH_PLATFORM)
            include(FetchContent)
            set(EPOCH_PLATFORM_BUILD_STATIC ON CACHE BOOL "" FORCE)
            set(EPOCH_PLATFORM_BUILD_SHARED OFF CACHE BOOL "" FORCE)
            set(EPOCH_PLATFORM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(EPOCH_PLATFORM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            FetchContent_Declare(EpochPlatformEngine
                GIT_REPOSITORY https://github.com/Autodidac/EpochPlatformEngine.git
                GIT_TAG ${EPOCH_PARTICLE_PLATFORM_GIT_TAG}
                GIT_SHALLOW FALSE)
            FetchContent_MakeAvailable(EpochPlatformEngine)
        endif()

        if(NOT COMMAND epoch_platform_add_application)
            message(FATAL_ERROR
                "EpochPlatformEngine was found without epoch_platform_add_application")
        endif()
        if(NOT TARGET EpochPlatformEngine::Static AND
           NOT TARGET EpochPlatformEngineStatic)
            message(FATAL_ERROR
                "EpochParticleLab requires the EpochPlatformEngine static library")
        endif()

        set(EPOCH_PARTICLE_LAB_SOURCES
            app/main.cpp
            app/ui_overlay.cpp
            app/ui_overlay.hpp
            ${EPOCH_PARTICLE_LAB_PLATFORM_SOURCES})
        epoch_platform_add_application(EpochParticleLab
            WINDOWED
            LINKAGE STATIC
            SOURCES ${EPOCH_PARTICLE_LAB_SOURCES})
        target_link_libraries(EpochParticleLab PRIVATE EpochParticleEngineVulkan)
        target_compile_definitions(EpochParticleLab PRIVATE
            EPOCH_PARTICLE_WITH_EPOCHGUI=$<BOOL:${EPOCH_PARTICLE_WITH_EPOCHGUI}>)
        if(EPOCH_PARTICLE_WITH_EPOCHGUI)
            target_link_libraries(EpochParticleLab PRIVATE EpochGui)
        endif()
        set_target_properties(EpochParticleLab PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:EpochParticleLab>")
        epoch_particle_apply_warnings(EpochParticleLab)
        epoch_particle_enable_sanitizers(EpochParticleLab)
    endif()
endif()


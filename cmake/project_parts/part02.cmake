if(EPOCH_PARTICLE_BUILD_EXAMPLES)
    add_executable(EpochParticleHeadless examples/headless.cpp)
    target_compile_features(EpochParticleHeadless PRIVATE cxx_std_23)
    target_link_libraries(EpochParticleHeadless PRIVATE EpochParticleEngine)
    set_target_properties(EpochParticleHeadless PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
    epoch_particle_apply_warnings(EpochParticleHeadless)
    epoch_particle_enable_sanitizers(EpochParticleHeadless)

    add_executable(EpochParticleReplay examples/replay_validation.cpp)
    target_compile_features(EpochParticleReplay PRIVATE cxx_std_23)
    target_link_libraries(EpochParticleReplay PRIVATE EpochParticleEngine)
    set_target_properties(EpochParticleReplay PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
    epoch_particle_apply_warnings(EpochParticleReplay)
    epoch_particle_enable_sanitizers(EpochParticleReplay)
endif()

if(EPOCH_PARTICLE_BUILD_BENCHMARKS)
    add_executable(EpochParticleBenchmark benchmarks/scene_benchmark.cpp)
    target_compile_features(EpochParticleBenchmark PRIVATE cxx_std_23)
    target_link_libraries(EpochParticleBenchmark PRIVATE EpochParticleEngine)
    set_target_properties(EpochParticleBenchmark PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
    epoch_particle_apply_warnings(EpochParticleBenchmark)
endif()

if(BUILD_TESTING AND EPOCH_PARTICLE_BUILD_TESTS)
    add_executable(EpochParticleEngineTests tests/particle_engine_tests.cpp)
    target_compile_features(EpochParticleEngineTests PRIVATE cxx_std_23)
    target_link_libraries(EpochParticleEngineTests PRIVATE EpochParticleEngine)
    set_target_properties(EpochParticleEngineTests PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
    epoch_particle_apply_warnings(EpochParticleEngineTests)
    epoch_particle_enable_sanitizers(EpochParticleEngineTests)
    add_test(NAME EpochParticleEngine.Determinism COMMAND EpochParticleEngineTests)
    if(EPOCH_PARTICLE_ENABLE_SANITIZERS)
        set_tests_properties(EpochParticleEngine.Determinism PROPERTIES
            TIMEOUT 300
            ENVIRONMENT "EPOCH_PARTICLE_TEST_TICKS=30")
    else()
        set_tests_properties(EpochParticleEngine.Determinism PROPERTIES TIMEOUT 180)
    endif()
endif()

if(EPOCH_PARTICLE_BUILD_MODULE)
    install(TARGETS EpochParticleEngine
        EXPORT EpochParticleEngineTargets
        FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILE_SET particle_modules DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/epochengine/particle/modules
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    install(TARGETS EpochParticleEngine
        EXPORT EpochParticleEngineTargets
        FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(EPOCH_PARTICLE_BUILD_VULKAN)
    install(TARGETS EpochParticleEngineVulkan
        EXPORT EpochParticleEngineTargets
        FILE_SET vulkan_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(EXPORT EpochParticleEngineTargets
    FILE EpochParticleEngineTargets.cmake
    NAMESPACE EpochParticleEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EpochParticleEngine)

set(EPOCH_PARTICLE_PACKAGE_HAS_VULKAN ${EPOCH_PARTICLE_BUILD_VULKAN})
configure_package_config_file(
    cmake/EpochParticleEngineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/EpochParticleEngineConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EpochParticleEngine)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/EpochParticleEngineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/EpochParticleEngineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/EpochParticleEngineConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EpochParticleEngine)

export(EXPORT EpochParticleEngineTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/EpochParticleEngineTargets.cmake
    NAMESPACE EpochParticleEngine::)

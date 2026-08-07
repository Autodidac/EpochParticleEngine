cmake_minimum_required(VERSION 3.28)

project(EpochParticlePackageConsumer LANGUAGES CXX)

find_package(EpochParticleEngine 0.1 CONFIG REQUIRED)

add_executable(EpochParticlePackageConsumer main.cpp)
target_compile_features(EpochParticlePackageConsumer PRIVATE cxx_std_23)
target_link_libraries(EpochParticlePackageConsumer PRIVATE EpochParticleEngine::Core)
set_target_properties(EpochParticlePackageConsumer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

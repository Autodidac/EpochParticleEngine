cmake_minimum_required(VERSION 3.28)

# Header-first by default. Module dependency scanning is enabled only for the
# optional module facade target source.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

project(EpochParticleEngine
    VERSION 0.2.2
    DESCRIPTION "Deterministic grid and physics particle simulation engine with Vulkan rendering and Particle Studio"
    LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/project_parts/part00.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/project_parts/part01.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/project_parts/part02.cmake")

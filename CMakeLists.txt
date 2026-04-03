cmake_minimum_required(VERSION 3.10)
project(myCPU VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(isa STATIC src/isa/decoder.cpp)
target_include_directories(isa PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(mycpu src/main.cpp)
target_link_libraries(mycpu PRIVATE isa)

add_executable(test_decoder tests/test_decoder.cpp)
target_link_libraries(test_decoder PRIVATE isa)
enable_testing()
add_test(NAME decoder_test COMMAND test_decoder)

# CPU library and single-cycle test
add_library(cpu STATIC src/cpu/registers.cpp src/cpu/alu.cpp src/cpu/single_cycle.cpp src/cpu/pipeline.cpp)
target_include_directories(cpu PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cpu PRIVATE isa)

add_executable(test_singlecycle tests/test_singlecycle.cpp)
target_link_libraries(test_singlecycle PRIVATE cpu isa)
add_test(NAME singlecycle_test COMMAND test_singlecycle)

add_executable(test_pipeline tests/test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE cpu isa)
add_test(NAME pipeline_test COMMAND test_pipeline)

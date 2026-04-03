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
add_library(cpu STATIC src/cpu/registers.cpp src/cpu/alu.cpp src/cpu/single_cycle.cpp src/cpu/pipeline.cpp src/mem/memory.cpp src/cache/simple_cache.cpp src/cpu/csr.cpp src/periph/timer.cpp src/periph/timer_mmio.cpp src/periph/uart_mmio.cpp)
target_include_directories(cpu PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cpu PRIVATE isa)

add_executable(test_singlecycle tests/test_singlecycle.cpp)
target_link_libraries(test_singlecycle PRIVATE cpu isa)
add_test(NAME singlecycle_test COMMAND test_singlecycle)

add_executable(test_pipeline tests/test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE cpu isa)
add_test(NAME pipeline_test COMMAND test_pipeline)

add_executable(test_pipeline_forward tests/test_pipeline_forward.cpp)
target_link_libraries(test_pipeline_forward PRIVATE cpu isa)
add_test(NAME pipeline_forward_test COMMAND test_pipeline_forward)

add_executable(test_timer_interrupt tests/test_timer_interrupt.cpp)
target_link_libraries(test_timer_interrupt PRIVATE cpu isa)
add_test(NAME timer_interrupt_test COMMAND test_timer_interrupt)

add_executable(test_uart_mmio tests/test_uart_mmio.cpp)
target_link_libraries(test_uart_mmio PRIVATE cpu isa)
add_test(NAME uart_mmio_test COMMAND test_uart_mmio)

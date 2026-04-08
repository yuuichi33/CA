cmake_minimum_required(VERSION 3.10)
project(myCPU VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(isa STATIC src/isa/decoder.cpp)
target_include_directories(isa PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(mycpu src/main.cpp)
target_link_libraries(mycpu PRIVATE isa cpu)

add_executable(test_decoder tests/test_decoder.cpp)
target_link_libraries(test_decoder PRIVATE isa)
enable_testing()
add_test(NAME decoder_test COMMAND test_decoder)

# CPU library and single-cycle test
add_library(cpu STATIC src/cpu/registers.cpp src/cpu/alu.cpp src/cpu/single_cycle.cpp src/cpu/pipeline.cpp src/mem/memory.cpp src/cache/simple_cache.cpp src/cpu/csr.cpp src/periph/timer.cpp src/periph/timer_mmio.cpp src/periph/tohost_mmio.cpp src/periph/uart_mmio.cpp src/mmu/mmu.cpp src/elf/elf_loader.cpp)
target_include_directories(cpu PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cpu PRIVATE isa)

add_executable(test_singlecycle tests/test_singlecycle.cpp)
target_link_libraries(test_singlecycle PRIVATE cpu isa)
add_test(NAME singlecycle_test COMMAND test_singlecycle)

add_executable(test_pipeline tests/test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE cpu isa)
add_test(NAME pipeline_test COMMAND test_pipeline)

add_executable(test_pipeline_uart_rx tests/test_pipeline_uart_rx.cpp)
target_link_libraries(test_pipeline_uart_rx PRIVATE cpu isa)
add_test(NAME pipeline_uart_rx_test COMMAND test_pipeline_uart_rx)

add_executable(test_pipeline_forward tests/test_pipeline_forward.cpp)
target_link_libraries(test_pipeline_forward PRIVATE cpu isa)
add_test(NAME pipeline_forward_test COMMAND test_pipeline_forward)

add_executable(test_timer_interrupt tests/test_timer_interrupt.cpp)
target_link_libraries(test_timer_interrupt PRIVATE cpu isa)
add_test(NAME timer_interrupt_test COMMAND test_timer_interrupt)

add_executable(test_uart_mmio tests/test_uart_mmio.cpp)
target_link_libraries(test_uart_mmio PRIVATE cpu isa)
add_test(NAME uart_mmio_test COMMAND test_uart_mmio)

add_executable(test_elf_loader tests/test_elf_loader.cpp)
target_link_libraries(test_elf_loader PRIVATE cpu isa)
add_test(NAME elf_loader_test COMMAND test_elf_loader)

add_executable(test_integration_uart_timer tests/test_integration_uart_timer.cpp)
target_link_libraries(test_integration_uart_timer PRIVATE cpu isa)
add_test(NAME integration_uart_timer_test COMMAND test_integration_uart_timer)

add_executable(test_load_store_byte_half tests/test_load_store_byte_half.cpp)
target_link_libraries(test_load_store_byte_half PRIVATE cpu isa)
add_test(NAME load_store_byte_half_test COMMAND test_load_store_byte_half)

add_executable(test_cache_basic tests/test_cache_basic.cpp)
target_link_libraries(test_cache_basic PRIVATE cpu isa)
add_test(NAME cache_basic_test COMMAND test_cache_basic)

add_executable(test_cache_wb tests/test_cache_wb.cpp)
target_link_libraries(test_cache_wb PRIVATE cpu isa)
add_test(NAME cache_writeback_test COMMAND test_cache_wb)

add_executable(test_pipeline_load_store_instr tests/test_pipeline_load_store_instr.cpp)
target_link_libraries(test_pipeline_load_store_instr PRIVATE cpu isa)
add_test(NAME pipeline_load_store_instr_test COMMAND test_pipeline_load_store_instr)

add_executable(test_isa_immediates tests/test_isa_immediates.cpp)
target_link_libraries(test_isa_immediates PRIVATE cpu isa)
add_test(NAME isa_immediates_test COMMAND test_isa_immediates)

add_executable(test_csrs tests/test_csrs.cpp)
target_link_libraries(test_csrs PRIVATE cpu isa)
add_test(NAME csrs_test COMMAND test_csrs)

add_executable(test_mmu_basic tests/test_mmu_basic.cpp)
target_link_libraries(test_mmu_basic PRIVATE cpu isa)
add_test(NAME mmu_basic_test COMMAND test_mmu_basic)

add_executable(test_privilege_security tests/test_privilege_security.cpp)
target_link_libraries(test_privilege_security PRIVATE cpu isa)
add_test(NAME privilege_security_test COMMAND test_privilege_security)

add_executable(test_trap_delegation tests/test_trap_delegation.cpp)
target_link_libraries(test_trap_delegation PRIVATE cpu isa)
add_test(NAME trap_delegation_test COMMAND test_trap_delegation)

add_executable(test_sfence_vma tests/test_sfence_vma.cpp)
target_link_libraries(test_sfence_vma PRIVATE cpu isa)
add_test(NAME sfence_vma_test COMMAND test_sfence_vma)

cmake_minimum_required(VERSION 3.13)

# ---- Set target board ----
set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350)

# ---- Import Pico SDK ----
include(pico_sdk_import.cmake)

# ---- Project ----
project(LifeGuard C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ---- Init SDK ----
pico_sdk_init()

# ---- FreeRTOS ----
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel)
include(FreeRTOS_Kernel_import.cmake)

# ---- Executable ----
add_executable(LifeGuard
    main.c
    gps.c
    gsm.c
)

# ---- Include dirs ----
target_include_directories(LifeGuard PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}/.
)

# ---- Link Libraries ----
target_link_libraries(LifeGuard PRIVATE
    pico_stdlib
    FreeRTOS-Kernel-Heap4
    pico_async_context_freertos
)

# ---- Enable I/O ----
pico_enable_stdio_usb(LifeGuard 1)
pico_enable_stdio_uart(LifeGuard 0)

# ---- Create UF2, bin, etc. ----
pico_add_extra_outputs(LifeGuard)
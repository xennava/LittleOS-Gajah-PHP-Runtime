cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_NASM_COMPILER nasm)
project(LittleOS LANGUAGES CXX ASM_NASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_UNITY_BUILD ON)
# set(CMAKE_UNITY_BUILD_BATCH_SIZE 0)

set(CMAKE_ASM_NASM_FLAGS "-f elf64")

set(CMAKE_EXE_LINKER_FLAGS
    "-fuse-ld=lld -nostdlib -T ${CMAKE_SOURCE_DIR}/kernel/linker.lds -no-pie"
)

set(ASM_SRC
    kernel/entry.asm
    kernel/isr.asm
)

include_directories(kernel/include)
include_directories(kernel/include/kernel)
include_directories(kernel/include/runtime)
include_directories(kernel/include/userland)

file(GLOB_RECURSE KERNEL_SRC
    kernel/source/*.c
    kernel/source/*.cpp
)

add_executable(kernel ${KERNEL_SRC} ${ASM_SRC})

target_sources(kernel PRIVATE
    obj/kernel/source/wallpaper.o
    obj/kernel/source/mascot.o
    obj/kernel/source/menu_icon.o
)
set_target_properties(kernel PROPERTIES
    LINKER_LANGUAGE CXX
)
target_compile_options(kernel PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:
    -ffreestanding
    -O3
    -Wall
    -Wextra
    -Werror
    -fno-exceptions
    -fno-rtti
    -fno-pie
    -fno-PIC
    -march=x86-64
    -fno-lto
    -fno-stack-protector
    -m64
    -fno-threadsafe-statics
    -mno-red-zone
    -mabi=sysv
    -fdata-sections
    -ffunction-sections
    -fno-stack-check
    -target x86_64-elf
    -mcmodel=kernel
    -mgeneral-regs-only
  >
)

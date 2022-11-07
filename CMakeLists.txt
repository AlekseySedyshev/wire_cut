SET(PROJECT_NAME "VsCode_32f030_WireCut")

SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_VERSION 1)
cmake_minimum_required(VERSION 3.7)

# specify cross compilers and tools
SET(CMAKE_C_COMPILER_WORKS 1)
SET(CMAKE_CXX_COMPILER_WORKS 1)


SET(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F030F4Px_FLASH.ld)

SET(FPU_FLAGS "-mfloat-abi=soft")


SET(COMMON_FLAGS 
"-mcpu=cortex-m0plus \
${FPU_FLAGS} \
-mthumb \
-specs=nano.specs \
-specs=nosys.specs \
-Wall \
-ffunction-sections \
-fdata-sections \
-fno-exceptions \
-fstack-usage \
")

SET(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS} -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")
SET(CMAKE_C_FLAGS_INIT "${COMMON_FLAGS} -Wimplicit-fallthrough -Werror-implicit-function-declaration")
SET(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections,--print-memory-usage -T ${LINKER_SCRIPT} -lc -lm -lnosys")

PROJECT(${PROJECT_NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


add_definitions(
    -DSTM32F030x6
)

if ((CMAKE_BUILD_TYPE MATCHES "Debug") OR (CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo"))
    add_definitions(
        -DDEBUG
    )
endif()

file(GLOB_RECURSE SOURCES 
"Core/*.c" 
CMSIS/system_stm32f0xx.c
CMSIS/Startup/startup_stm32f030x6.s
)

include_directories(
    Core
    CMSIS
 )

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT})

set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map,--cref")

set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
        COMMENT "Building ${HEX_FILE}
Building ${BIN_FILE}")

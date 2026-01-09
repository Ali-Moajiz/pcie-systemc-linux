cmake_minimum_required(VERSION 3.10)
project(PCIE_SYSTEMC_PROJECT LANGUAGES C CXX)

# ======================================================
# REQUIRE USER TO SELECT BUILD TARGET
# ======================================================
if(NOT DEFINED BUILD_TARGET)
    message(FATAL_ERROR
        "You must specify a build target:\n"
        "  cmake -S . -B build -DBUILD_TARGET=testbench\n"
        "  OR\n"
        "  cmake -S . -B build -DBUILD_TARGET=main")
endif()

if(NOT BUILD_TARGET STREQUAL "testbench" AND NOT BUILD_TARGET STREQUAL "main")
    message(FATAL_ERROR "Invalid BUILD_TARGET=${BUILD_TARGET}. Allowed: testbench OR main")
endif()

message(STATUS "Selected BUILD_TARGET = ${BUILD_TARGET}")

# ======================================================
# REQUIRE SYSTEMC_HOME
# ======================================================
if(NOT DEFINED ENV{SYSTEMC_HOME})
    message(FATAL_ERROR
        "SYSTEMC_HOME is not set.\n"
        "Please run: export SYSTEMC_HOME=/usr/local/systemc-git")
endif()

set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})
message(STATUS "Using SYSTEMC_HOME = ${SYSTEMC_HOME}")

# Compiler setup
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g3 -O0 -DSC_INCLUDE_DYNAMIC_PROCESSES")

# ======================================================
# Locate SystemC Library
# ======================================================
if(EXISTS "${SYSTEMC_HOME}/lib/libsystemc.so")
    set(SYSTEMC_LIBRARY_DIRS "${SYSTEMC_HOME}/lib")
elseif(EXISTS "${SYSTEMC_HOME}/lib-linux64/libsystemc.so")
    set(SYSTEMC_LIBRARY_DIRS "${SYSTEMC_HOME}/lib-linux64")
else()
    message(FATAL_ERROR "libsystemc.so not found in SYSTEMC_HOME")
endif()

include_directories(${SYSTEMC_HOME}/include)
link_directories(${SYSTEMC_LIBRARY_DIRS})

# ======================================================
# Add libsystemctlm-soc sources using object libraries
# ======================================================
set(SCTLMSOC_DIR ${PROJECT_SOURCE_DIR}/../libsystemctlm-soc)

if(NOT EXISTS ${SCTLMSOC_DIR})
    message(FATAL_ERROR "libsystemctlm-soc not found in ../libsystemctlm-soc")
endif()

message(STATUS "Adding libsystemctlm-soc from: ${SCTLMSOC_DIR}")

# Common include directories for all object libraries
set(SCTLMSOC_INCLUDES
    ${SCTLMSOC_DIR}
    ${SCTLMSOC_DIR}/libremote-port
    ${SCTLMSOC_DIR}/tlm-bridges
    ${SCTLMSOC_DIR}/tlm-extensions
    ${SCTLMSOC_DIR}/tlm-modules
    ${SCTLMSOC_DIR}/soc/pci/core
    ${SCTLMSOC_DIR}/zynq
    ${SCTLMSOC_DIR}/zynqmp
)

include_directories(${SCTLMSOC_INCLUDES})

# Create sub-libraries as object libraries to avoid arg limits
# FIXED: Remote port needs BOTH .cc AND .c files
file(GLOB REMOTE_PORT_CC_SOURCES "${SCTLMSOC_DIR}/libremote-port/*.cc")
file(GLOB REMOTE_PORT_C_SOURCES "${SCTLMSOC_DIR}/libremote-port/*.c")
set(REMOTE_PORT_SOURCES ${REMOTE_PORT_CC_SOURCES} ${REMOTE_PORT_C_SOURCES})

if(REMOTE_PORT_SOURCES)
    list(LENGTH REMOTE_PORT_SOURCES REMOTE_PORT_COUNT)
    message(STATUS "Found ${REMOTE_PORT_COUNT} remote-port source files")
    add_library(sctlmsoc_remote_port OBJECT ${REMOTE_PORT_SOURCES})
    target_include_directories(sctlmsoc_remote_port PRIVATE ${SCTLMSOC_INCLUDES})
    # Explicitly set language for C files
    set_source_files_properties(${REMOTE_PORT_C_SOURCES} PROPERTIES LANGUAGE C)
endif()

file(GLOB TLM_BRIDGES_SOURCES "${SCTLMSOC_DIR}/tlm-bridges/*.cc")
if(TLM_BRIDGES_SOURCES)
    add_library(sctlmsoc_tlm_bridges OBJECT ${TLM_BRIDGES_SOURCES})
    target_include_directories(sctlmsoc_tlm_bridges PRIVATE ${SCTLMSOC_INCLUDES})
endif()

file(GLOB TLM_EXTENSIONS_SOURCES "${SCTLMSOC_DIR}/tlm-extensions/*.cc")
if(TLM_EXTENSIONS_SOURCES)
    add_library(sctlmsoc_tlm_extensions OBJECT ${TLM_EXTENSIONS_SOURCES})
    target_include_directories(sctlmsoc_tlm_extensions PRIVATE ${SCTLMSOC_INCLUDES})
endif()

file(GLOB TLM_MODULES_SOURCES "${SCTLMSOC_DIR}/tlm-modules/*.cc")
if(TLM_MODULES_SOURCES)
    add_library(sctlmsoc_tlm_modules OBJECT ${TLM_MODULES_SOURCES})
    target_include_directories(sctlmsoc_tlm_modules PRIVATE ${SCTLMSOC_INCLUDES})
endif()

file(GLOB_RECURSE SOC_SOURCES "${SCTLMSOC_DIR}/soc/*.cc")
if(SOC_SOURCES)
    add_library(sctlmsoc_soc OBJECT ${SOC_SOURCES})
    target_include_directories(sctlmsoc_soc PRIVATE ${SCTLMSOC_INCLUDES})
endif()

file(GLOB ZYNQ_SOURCES "${SCTLMSOC_DIR}/zynq/*.cc")
if(ZYNQ_SOURCES)
    add_library(sctlmsoc_zynq OBJECT ${ZYNQ_SOURCES})
    target_include_directories(sctlmsoc_zynq PRIVATE ${SCTLMSOC_INCLUDES})
endif()

file(GLOB ZYNQMP_SOURCES "${SCTLMSOC_DIR}/zynqmp/*.cc")
if(ZYNQMP_SOURCES)
    add_library(sctlmsoc_zynqmp OBJECT ${ZYNQMP_SOURCES})
    target_include_directories(sctlmsoc_zynqmp PRIVATE ${SCTLMSOC_INCLUDES})
endif()

# Combine all object libraries into a static lib
set(OBJECT_LIBS)
if(TARGET sctlmsoc_remote_port)
    list(APPEND OBJECT_LIBS $<TARGET_OBJECTS:sctlmsoc_remote_port>)
endif()
if(TARGET sctlmsoc_tlm_bridges)
    list(APPEND OBJECT_LIBS $<TARGET_OBJECTS:sctlmsoc_tlm_bridges>)
endif()
if(TARGET sctlmsoc_tlm_extensions)
    list(APPEND OBJECT_LIBS $<TARGET_OBJECTS:sctlmsoc_tlm_extensions>)
endif()
if(TARGET sctlmsoc_tlm_modules)
    list(APPEND OBJECT_LIBS $<TARGET_OBJECTS:sctlmsoc_tlm_modules>)
endif()
if(TARGET sctlmsoc_soc)
    list(APPEND OBJECT_LIBS $<TARGET_OBJECTS:sctlmsoc_soc>)
endif()
if(TARGET sctlmsoc_zynq)
    list(APPEND OBJECT_LIBS $<TARGET_OBJECTS:sctlmsoc_zynq>)
endif()
if(TARGET sctlmsoc_zynqmp)
    list(APPEND OBJECT_LIBS $<TARGET_OBJECTS:sctlmsoc_zynqmp>)
endif()

add_library(systemctlm-soc STATIC ${OBJECT_LIBS})

# ======================================================
# Build PCIe Model as Static Library
# ======================================================
set(PCIE_MODEL_DIR ${PROJECT_SOURCE_DIR}/../pcie-model)

# Collect all C files from libpcie/src/pcie
file(GLOB PCIE_MODEL_C_SOURCES
    "${PCIE_MODEL_DIR}/libpcie/src/pcie/*.c"
)

# Collect all C++ files from libpcie and tlm-modules
file(GLOB PCIE_MODEL_CXX_SOURCES
    "${PCIE_MODEL_DIR}/libpcie/src/pcie/*.cc"
    "${PCIE_MODEL_DIR}/tlm-modules/*.cc"
)

# Combine all sources
set(PCIE_MODEL_SOURCES ${PCIE_MODEL_C_SOURCES} ${PCIE_MODEL_CXX_SOURCES})

list(LENGTH PCIE_MODEL_SOURCES PCIE_SOURCE_COUNT)
message(STATUS "Found ${PCIE_SOURCE_COUNT} PCIe model source files")

if(NOT PCIE_MODEL_SOURCES)
    message(FATAL_ERROR "No PCIe model source files found in ${PCIE_MODEL_DIR}")
endif()

add_library(pcie_model STATIC ${PCIE_MODEL_SOURCES})

# Explicitly set language for C files
set_source_files_properties(${PCIE_MODEL_C_SOURCES} PROPERTIES LANGUAGE C)

# Add CONFIG_TLM define for TLM/SystemC mode
target_compile_definitions(pcie_model PRIVATE CONFIG_TLM)  

target_include_directories(pcie_model PUBLIC
    ${PCIE_MODEL_DIR}
    ${PCIE_MODEL_DIR}/tlm-modules
    ${PCIE_MODEL_DIR}/libpcie
    ${PCIE_MODEL_DIR}/libpcie/src
    ${PCIE_MODEL_DIR}/libpcie/src/pcie
    ${SCTLMSOC_INCLUDES}
)

# Link PCIe model with systemctlm-soc library
target_link_libraries(pcie_model
    PUBLIC
        systemctlm-soc
)

# ======================================================
# Select Final Executable
# ======================================================
if(BUILD_TARGET STREQUAL "testbench")
    set(EXE_NAME pcie_test)
    set(SRC_FILE testbench.cpp)
elseif(BUILD_TARGET STREQUAL "main")
    set(EXE_NAME pcie_main)
    set(SRC_FILE main.cc)
endif()

message(STATUS "Building executable: ${EXE_NAME}")

add_executable(${EXE_NAME} ${SRC_FILE})

# CRITICAL: Ensure executable target includes all needed submodule include directories
target_include_directories(${EXE_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PCIE_MODEL_DIR}
    ${PCIE_MODEL_DIR}/libpcie/src
    ${PCIE_MODEL_DIR}/libpcie
    ${PCIE_MODEL_DIR}/libpcie/src/pcie
    ${PCIE_MODEL_DIR}/tlm-modules
    ${SCTLMSOC_INCLUDES}
)

target_link_libraries(${EXE_NAME}
    pcie_model
    systemc
    pthread
)

set_target_properties(${EXE_NAME} PROPERTIES
    BUILD_RPATH "${SYSTEMC_LIBRARY_DIRS}"
    INSTALL_RPATH "${SYSTEMC_LIBRARY_DIRS}"
)

message(STATUS "Configuration complete.")
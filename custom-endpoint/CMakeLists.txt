cmake_minimum_required(VERSION 3.10)
project(PCIE_SYSTEMC_PROJECT LANGUAGES CXX)

# ======================================================
# Get SYSTEMC_HOME from environment
# ======================================================
if(NOT DEFINED ENV{SYSTEMC_HOME})
    message(FATAL_ERROR "SYSTEMC_HOME is not set! Please run: export SYSTEMC_HOME=/usr/local/systemc-git")
endif()

set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})

message(STATUS "Using SYSTEMC_HOME = ${SYSTEMC_HOME}")

# ======================================================
# Compiler settings
# ======================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O2 -DSC_INCLUDE_DYNAMIC_PROCESSES")

# ======================================================
# SystemC library detection
# ======================================================
# Check for library in both possible locations
if(EXISTS "${SYSTEMC_HOME}/lib/libsystemc.so")
    set(SYSTEMC_LIBRARY_DIRS "${SYSTEMC_HOME}/lib")
    set(SYSTEMC_LIB_PATH "${SYSTEMC_HOME}/lib")
    message(STATUS "Found SystemC library in: ${SYSTEMC_HOME}/lib")
elseif(EXISTS "${SYSTEMC_HOME}/lib-linux64/libsystemc.so")
    set(SYSTEMC_LIBRARY_DIRS "${SYSTEMC_HOME}/lib-linux64")
    set(SYSTEMC_LIB_PATH "${SYSTEMC_HOME}/lib-linux64")
    message(STATUS "Found SystemC library in: ${SYSTEMC_HOME}/lib-linux64")
else()
    message(FATAL_ERROR "Could not find libsystemc.so in ${SYSTEMC_HOME}/lib or ${SYSTEMC_HOME}/lib-linux64")
endif()

# ======================================================
# Include SystemC
# ======================================================
include_directories(${SYSTEMC_HOME}/include)
link_directories(${SYSTEMC_LIBRARY_DIRS})

# ======================================================
# Build PCIe Model as Static Library
# ======================================================
set(PCIE_MODEL_DIR ${PROJECT_SOURCE_DIR}/../pcie-model)

file(GLOB PCIE_MODEL_SOURCES
    "${PCIE_MODEL_DIR}/libpcie/src/pcie/*.c"
    "${PCIE_MODEL_DIR}/libpcie/src/pcie/*.cc"
    "${PCIE_MODEL_DIR}/libpcie/*.c"
    "${PCIE_MODEL_DIR}/libpcie/*.cc"
    "${PCIE_MODEL_DIR}/tlm-modules/*.c"
    "${PCIE_MODEL_DIR}/tlm-modules/*.cc"
)


# Create PCIe model static library
add_library(pcie_model STATIC ${PCIE_MODEL_SOURCES})

# Set include directories for the library (PUBLIC so consumers get them too)
target_include_directories(pcie_model PUBLIC
    ${PCIE_MODEL_DIR}/tlm-modules
    ${PCIE_MODEL_DIR}/libpcie
    ${PCIE_MODEL_DIR}/libpcie/src
)

message(STATUS "PCIe model library configured")

# ======================================================
# Build Your Testbench Executable
# ======================================================
include_directories(${PROJECT_SOURCE_DIR}/include)

set(SOURCES
    testbench.cpp
)


add_executable(pcie_test ${SOURCES})

# target_compile_options(pcie_test PRIVATE -g -O1 -fsanitize=address -fno-omit-frame-pointer)

# ======================================================
# Link Libraries
# ======================================================
target_link_libraries(pcie_test
    pcie_model      # PCIe model static library
    systemc         # SystemC library
    pthread         # Threading support
)

# ======================================================
# Runtime library path (LD_LIBRARY_PATH auto)
# ======================================================
set_target_properties(pcie_test PROPERTIES
    BUILD_RPATH "${SYSTEMC_LIB_PATH}"
    INSTALL_RPATH "${SYSTEMC_LIB_PATH}"
)

message(STATUS "SystemC include: ${SYSTEMC_HOME}/include")
message(STATUS "SystemC library: ${SYSTEMC_LIB_PATH}")
message(STATUS "PCIe model: ${PCIE_MODEL_DIR}")
message(STATUS "Configuration complete!")
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
# Include directories
# ======================================================
include_directories(
    ${SYSTEMC_HOME}/include
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/pcie/xilinx
)

# ======================================================
# Link directories
# ======================================================
link_directories(${SYSTEMC_LIBRARY_DIRS})

# ======================================================
# Source files
# ======================================================
set(SOURCES
    testbench.cpp
)

# ======================================================
# Executable
# ======================================================
add_executable(pcie_test ${SOURCES})

# ======================================================
# Link SystemC
# ======================================================
target_link_libraries(pcie_test systemc pthread)

# ======================================================
# Runtime library path (LD_LIBRARY_PATH auto)
# ======================================================
set_target_properties(pcie_test PROPERTIES
    BUILD_RPATH "${SYSTEMC_LIB_PATH}"
    INSTALL_RPATH "${SYSTEMC_LIB_PATH}"
)

message(STATUS "SystemC include: ${SYSTEMC_HOME}/include")
message(STATUS "SystemC library: ${SYSTEMC_LIB_PATH}")
message(STATUS "Configuration complete!")
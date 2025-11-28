# ===============================
# PCIe SystemC Makefile
# ===============================

# Force Makefile to use user environment SYSTEMC_HOME
SYSTEMC_HOME := $(shell echo $$SYSTEMC_HOME)

# Safety check
ifeq ($(SYSTEMC_HOME),)
  $(error SYSTEMC_HOME is not set. Run: export SYSTEMC_HOME=/home/moajiz/systemc)
endif

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g -O2 -DSC_INCLUDE_DYNAMIC_PROCESSES

# Include directories
INCLUDES = -I$(SYSTEMC_HOME)/include \
           -I./include \
           -I./pcie/xilinx

# Library paths
LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64

# Libraries
LIBS = -lsystemc -lpthread -lm

# Sources and objects
SRCS = testbench.cpp
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = pcie_test

# ===============================
# Default target
# ===============================
all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LIBS) -o $(TARGET)
	@echo "Build successful!"

%.o: %.cpp
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ===============================
# Run Simulation
# ===============================
run: $(TARGET)
	@echo "Running SystemC simulation..."
	LD_LIBRARY_PATH=$(SYSTEMC_HOME)/lib-linux64:$$LD_LIBRARY_PATH ./$(TARGET)

# ===============================
# Clean
# ===============================
clean:
	@echo "Cleaning..."
	rm -f $(OBJS) $(TARGET) *.vcd *.log *.d
	@echo "Clean complete!"

# ===============================
# Help
# ===============================
help:
	@echo "Usage:"
	@echo "  make              Build project"
	@echo "  make run          Build + run"
	@echo "  make clean        Clean build files"
	@echo ""
	@echo "SYSTEMC_HOME used:"
	@echo "  $(SYSTEMC_HOME)"

# ===============================
# Auto dependency generation
# ===============================
-include $(OBJS:.o=.d)

%.d: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< -MF $@

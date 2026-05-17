CXX       = g++
CXXFLAGS  = -std=c++17 -Wall -O2 -g

SYSTEMC_HOME = $(HOME)/opt/systemc-2.3.4
SYSTEMC_INC  = $(SYSTEMC_HOME)/include
SYSTEMC_LIB  = $(SYSTEMC_HOME)/lib

INCLUDES = -I$(SYSTEMC_INC) -I.
LDFLAGS  = -L$(SYSTEMC_LIB) -Wl,-rpath,$(SYSTEMC_LIB) -Wl,-stack_size,0x2000000
LIBS     = -lsystemc -lpthread

SRCS  = main.cpp \
        cpu/rv32-lt/cpu.cpp \
        cpu/rv32-lt/memory_interface.cpp \
        mem/memory.cpp

OBJS  = $(SRCS:.cpp=.o)
TARGET = my_rv32

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET) tests/firmware.hex

test: $(TARGET)
	@echo "=== Test 1: ALU ==="
	./$(TARGET) tests/test_alu.hex && echo "PASS"
	@echo ""
	@echo "=== Test 2: Memory ==="
	./$(TARGET) tests/test_mem.hex && echo "PASS"
	@echo ""
	@echo "=== Test 3: Loop ==="
	./$(TARGET) tests/test_loop.hex && echo "PASS"
	@echo ""
	@echo "=== Test 4: Call ==="
	./$(TARGET) tests/test_call.hex && echo "PASS"

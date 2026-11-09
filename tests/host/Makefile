CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined
ROOT     := ../..
INC      := -I$(ROOT)/lib/prolink -I$(ROOT)/lib/clock_engine
SRC      := test_main.cpp \
            $(ROOT)/lib/prolink/prolink_packets.cpp \
            $(ROOT)/lib/prolink/device_table.cpp \
            $(ROOT)/lib/clock_engine/clock_engine.cpp

test: run_tests
	./run_tests

run_tests: $(SRC) $(wildcard $(ROOT)/lib/*/*.h)
	$(CXX) $(CXXFLAGS) $(INC) $(SRC) -o $@

clean:
	rm -f run_tests

.PHONY: test clean

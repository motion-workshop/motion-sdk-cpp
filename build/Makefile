#
# @file    build/Makefile
# @author  Luke Tokheim, luke@motionshadow.com
# @version 3.0
#

CPPFLAGS = -O3 -Wall -Wextra -Werror -std=c++11
INCLUDE = -I../include

# Enables Clang Source-based Code Coverage
# https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
ifdef COVERAGE
CPPFLAGS += -fprofile-instr-generate -fcoverage-mapping
LDFLAGS += -fprofile-instr-generate
endif

# Static library
NAME = MotionSDK
TARGET = lib$(NAME).a
SRC = ../src/Client.cpp ../src/Format.cpp
OBJ = $(patsubst %.cpp,%.o,$(SRC))

# Unit test executable
TEST_TARGET = test_$(NAME)
TEST_SRC = ../test/test.cpp ../test/test_client.cpp ../test/test_format.cpp
TEST_OBJ = $(patsubst %.cpp,%.o,$(TEST_SRC))
# Run the unit test executable and test server.
TEST_EXEC = python3 ../test/test_server.py ./$(TEST_TARGET)

# Example executables
# Basic example, from the project README docs
EX_TARGET = example_$(NAME)
EX_SRC = ../example/example.cpp
EX_OBJ = $(patsubst %.cpp,%.o,$(EX_SRC))
# More in depth example, stream to CSV format
EX1_TARGET = example_stream_csv
EX1_SRC = ../example/example_stream_csv.cpp
EX1_OBJ = $(patsubst %.cpp,%.o,$(EX1_SRC))

# Detect macOS since we need to use xcrun for the llvm-* tools.
ifeq ($(shell uname),Darwin)
XCRUN = xcrun
endif

all: $(TARGET) $(TEST_TARGET) $(EX_TARGET) $(EX1_TARGET)

test: $(TEST_TARGET)
ifdef COVERAGE
	LLVM_PROFILE_FILE=coverage.profraw $(TEST_EXEC)
else
	$(TEST_EXEC)
endif

example: $(EX_TARGET) $(EX1_TARGET)

$(TARGET): $(OBJ)
	$(AR) rcs $@ $(OBJ)

$(TEST_TARGET): $(TARGET) $(TEST_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(TEST_OBJ) -L. -l$(NAME)

$(EX_TARGET): $(TARGET) $(EX_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(EX_OBJ) -L. -l$(NAME)

$(EX1_TARGET): $(TARGET) $(EX1_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(EX1_OBJ) -L. -l$(NAME)

%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) $(INCLUDE) -o $@ $<

# Non-standard target, any txt file. This will run the unit tests, generate
# the profiling data, index the raw profile data, and then export a line by line
# report in text format.
ifdef COVERAGE
%.profraw: $(TEST_TARGET)
	LLVM_PROFILE_FILE=$@ $(TEST_EXEC)

%.profdata: %.profraw
	$(XCRUN) llvm-profdata merge -output=$@ $<

%.txt: %.profdata
	$(XCRUN) llvm-cov show $(TEST_TARGET) -instr-profile=$< >$@

coverage: coverage.txt
endif

docs:
	$(MAKE) -C ../docs html

clean:
	@$(RM) $(OBJ) $(TARGET) $(TEST_OBJ) $(TEST_TARGET) $(EX_OBJ) $(EX_TARGET) $(EX1_OBJ) $(EX1_TARGET) coverage.*

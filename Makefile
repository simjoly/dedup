# Makefile for dedup (FASTQ deduplication tool)
# Supports macOS (Homebrew) and Linux

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall
LDFLAGS = -lz -lssl -lcrypto -lsqlite3

# Detect OS
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)  # macOS
    CXXFLAGS += -I/opt/homebrew/opt/zlib/include \
                -I/opt/homebrew/opt/openssl@3/include \
                -I/opt/homebrew/opt/sqlite/include
    LDFLAGS  += -L/opt/homebrew/opt/zlib/lib \
                -L/opt/homebrew/opt/openssl@3/lib \
                -L/opt/homebrew/opt/sqlite/lib
endif

# Target binary
TARGET = dedup

# Sources
SRCS = dedup.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
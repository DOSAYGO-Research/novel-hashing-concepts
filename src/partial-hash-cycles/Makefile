# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2

# OpenSSL paths (auto-detected via Homebrew)
OPENSSL_PREFIX := $(shell brew --prefix openssl)
CXXFLAGS += -I$(OPENSSL_PREFIX)/include
LIBS = -L$(OPENSSL_PREFIX)/lib -lssl -lcrypto

# Files
TARGET = phc-analyze
HEADERS = hash-cycle-monitor.h
SOURCES = phc-analyze.cpp

# Build target
all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

# Clean up build files
clean:
	rm -f $(TARGET)

.PHONY: all clean


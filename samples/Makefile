# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall

# Vcpkg configuration
VCPKG_ROOT ?= $(error VCPKG_ROOT is not set. Please set the VCPKG_ROOT environment variable)
ARCH = $(shell uname -m | sed 's/x86_64/x64/;s/i.86/x86/;s/aarch64/arm64/')
PLATFORM = $(shell uname | tr '[:upper:]' '[:lower:]')
TRIPLET = $(ARCH)-$(PLATFORM)

# Source and object files
SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)

# Include and library paths
CPPFLAGS += -I$(VCPKG_ROOT)/installed/$(TRIPLET)/include
LDFLAGS += -L$(VCPKG_ROOT)/installed/$(TRIPLET)/lib

# Library flags
LDLIBS += -lconfigcat -lcpr -lcurl -lz -lssl -lcrypto -lpthread -lhash-library

# Build target
TARGET = example

# Rules
all: install-vcpkg-packages $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

install-vcpkg-packages:
	(cd $(VCPKG_ROOT) && ./vcpkg install configcat:$(TRIPLET))

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all install-vcpkg-packages clean

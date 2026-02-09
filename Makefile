# Makefile for gpuio library

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -fPIC -O2 -g -I./include -I./src
LDFLAGS = -shared -lpthread -ldl
AR = ar
ARFLAGS = rcs

# Source files
SRCS = src/context.c \
       src/memory.c \
       src/stream.c \
       src/log.c \
       src/vendor_nvidia.c \
       src/vendor_amd.c \
       src/gpuio.c

# Object files
OBJS = $(SRCS:.c=.o)

# Library names
LIB_SHARED = libgpuio.so
LIB_STATIC = libgpuio.a

# Installation directories
PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

# Targets
.PHONY: all clean install uninstall test

all: $(LIB_SHARED) $(LIB_STATIC)

# Shared library
$(LIB_SHARED): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Static library
$(LIB_STATIC): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

# Object files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean
clean:
	rm -f $(OBJS) $(LIB_SHARED) $(LIB_STATIC)

# Install
install: all
	mkdir -p $(LIBDIR) $(INCLUDEDIR)/gpuio
	cp $(LIB_SHARED) $(LIB_STATIC) $(LIBDIR)/
	cp include/gpuio/*.h $(INCLUDEDIR)/gpuio/
	ldconfig

# Uninstall
uninstall:
	rm -f $(LIBDIR)/$(LIB_SHARED) $(LIBDIR)/$(LIB_STATIC)
	rm -rf $(INCLUDEDIR)/gpuio

# Run tests
test: all
	$(MAKE) -C tests test

# Debug build
debug: CFLAGS += -DDEBUG -g -O0
debug: all

# Release build with optimizations
release: CFLAGS += -O3 -DNDEBUG
release: all

# Help
help:
	@echo "gpuio Library Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build shared and static libraries"
	@echo "  debug    - Build with debug symbols"
	@echo "  release  - Build optimized release version"
	@echo "  install  - Install library and headers"
	@echo "  uninstall - Remove installed files"
	@echo "  test     - Run test suite"
	@echo "  clean    - Remove build artifacts"
	@echo "  help     - Show this help"

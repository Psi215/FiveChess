# Compiler
CC = gcc

# Common Flags
INCLUDES = -I include
CFLAGS_COMMON = -Wall -Wextra $(INCLUDES)

# Specific Flags
CFLAGS_DEBUG = $(CFLAGS_COMMON) -g
CFLAGS_RELEASE = $(CFLAGS_COMMON) -O2

# Source files
SRCS = board.c Game.c history.c Input.c Judge.c AIEngine.c main.c

# Object files
OBJS_DEBUG = $(SRCS:.c=_debug.o)
OBJS_RELEASE = $(SRCS:.c=_release.o)

# Targets
TARGET_DEBUG = FiveChess_debug.exe
TARGET_RELEASE = FiveChess_release.exe

# Default target
all: debug release

# Debug build
debug: $(TARGET_DEBUG)

$(TARGET_DEBUG): $(OBJS_DEBUG)
	$(CC) $(CFLAGS_DEBUG) -o $@ $^

# Release build
release: $(TARGET_RELEASE)

$(TARGET_RELEASE): $(OBJS_RELEASE)
	$(CC) $(CFLAGS_RELEASE) -o $@ $^

# Compilation rules
%_debug.o: %.c
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

%_release.o: %.c
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

# Clean up
clean:
	del /Q *_debug.o *_release.o *.o $(TARGET_DEBUG) $(TARGET_RELEASE) 2>nul

# Run (defaults to debug)
run: debug
	./$(TARGET_DEBUG)

.PHONY: all clean run debug release

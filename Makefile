# Headless Build by default since SDL is missing
CC = gcc
CFLAGS = -Wall -Iinclude -g
# LDFLAGS = -lSDL2 # Uncomment if SDL is present

# To build with SDL: make SDL=1
ifneq ($(SDL),)
CFLAGS += -DUSE_SDL
# Check if sdl2-config works by running it
SDL_VER := $(shell sdl2-config --version 2> /dev/null)

ifneq ($(SDL_VER),)
    # Found sdl2-config
    CFLAGS += $(shell sdl2-config --cflags)
    LDFLAGS += $(shell sdl2-config --libs)
else
    # Fallback for Windows/MinGW if sdl2-config is missing
    # Assuming standard MSYS2/MinGW64 paths
    # Use -I. to ensure local includes are found too
    CFLAGS += -I/mingw64/include/SDL2 -Dmain=SDL_main
    LDFLAGS += -lmingw32 -lSDL2main -lSDL2
endif
endif

SRC_DIR = src
OBJ_DIR = .

SRCS = $(wildcard $(SRC_DIR)/*.c)
# Exclude test files from main build
SRCS := $(filter-out $(SRC_DIR)/test_%.c, $(SRCS))

OBJS = $(SRCS:.c=.o)
TARGET = gba_emu

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

test_cpu: src/cpu.o src/test_cpu.o src/memory.o
	$(CC) src/cpu.o src/test_cpu.o src/memory.o -o test_cpu -g

test_ppu: src/ppu.o src/test_ppu.o src/memory.o
	$(CC) src/ppu.o src/test_ppu.o src/memory.o -o test_ppu -g

test_input: src/memory.o src/test_input.o
	$(CC) src/memory.o src/test_input.o -o test_input -g

test_integration: src/cpu.o src/ppu.o src/memory.o src/test_integration.o
	$(CC) src/cpu.o src/ppu.o src/memory.o src/test_integration.o -o test_integration -g

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o $(TARGET) test_cpu

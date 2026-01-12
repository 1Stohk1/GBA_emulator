CC = "C:/msys64/mingw64/bin/gcc.exe"
CFLAGS = -Wall -Iinclude -g
LDFLAGS = -lmingw32 -lSDL2main -lSDL2

SRC_DIR = src
OBJ_DIR = .

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = gba_emu

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q $(SRC_DIR)\*.o $(TARGET).exe

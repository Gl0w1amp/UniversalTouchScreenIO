CC = gcc
CFLAGS = -shared -O2 -Wall -I.
LDFLAGS = -luser32 -lgdi32 -static-libgcc
TARGET = mai2io.dll
SRCS = mai2io.c config.c touch_impl.c dprintf.c
DEF = mai2io.def

all: $(TARGET)

$(TARGET): $(SRCS) $(DEF)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(DEF) $(LDFLAGS)

clean:
	del $(TARGET)

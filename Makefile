CC      = gcc
CFLAGS  = -Wall -std=c99 -O2
SRCS    = lexer.c parser.c interp.c compiler.c vm_impl.c langweb.c main.c
TARGET  = lang

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) -lm

install: all
	sudo cp $(TARGET) /usr/local/bin/$(TARGET)

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET)

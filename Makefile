CC      = gcc
CFLAGS  = -Wall -std=c99 -O2
SRCS    = lexer.c parser.c interp.c compiler.c vm_impl.c langweb.c langdb.c main.c
TARGET  = lang
LIBS    = -lm -lsqlite3

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

install: all
	sudo cp $(TARGET) /usr/local/bin/$(TARGET)
	@echo "Lang installed to /usr/local/bin/lang"

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET)

test: all
	@./$(TARGET) tests.lang

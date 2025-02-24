# Makefile for Caecode with GtkSourceView

CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 gtksourceview-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0 gtksourceview-3.0`
TARGET = caecode
SRC = caecode.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
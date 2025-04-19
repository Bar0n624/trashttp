CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDFLAGS = -lssl -lcrypto

SOURCES = main.c http.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
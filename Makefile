CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -g
LDFLAGS = -lssl -lcrypto

SOURCES = ./src/main.c ./src/http.c ./src/server.c ./src/thread_pool.c ./src/ssl_utils.c ./src/config.c ./src/logger.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
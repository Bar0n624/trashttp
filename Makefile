CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -g
LDFLAGS = -lssl -lcrypto

SOURCES = ./src/main.c ./src/http.c ./src/server.c ./src/thread_pool.c ./src/ssl_utils.c ./src/config.c ./src/logger.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = trashttp

DEBUG_CFLAGS = -Wall -Wextra -O0 -pthread -g -DDEBUG
DEBUG_OBJECTS = $(SOURCES:.c=.debug.o)
DEBUG_TARGET = trashttp_debug

all: $(TARGET) clean

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(DEBUG_OBJECTS)
	$(CC) $(DEBUG_CFLAGS) -o $@ $^ $(LDFLAGS)

%.debug.o: %.c
	$(CC) $(DEBUG_CFLAGS) -c $< -o $@

run: $(TARGET) clean
	./$(TARGET)

clean:
	rm -f $(OBJECTS) $(DEBUG_OBJECTS) $(DEBUG_TARGET)
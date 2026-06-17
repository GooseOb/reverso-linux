VERSION ?= $(shell { git describe --tags --match 'v*' 2>/dev/null || echo "0.0.1"; } | sed 's/^v//')
CFLAGS = -O3 -Wall -Wextra -pedantic -std=c11 -DVERSION=\"$(VERSION)\" $(shell pkg-config --cflags libcurl json-c gtk+-3.0)
LDLIBS = $(shell pkg-config --libs libcurl json-c gtk+-3.0) -lm

BUILD_DIR = build
SRC = src/main.c src/translator.c src/gui.c src/config.c
OBJ = $(SRC:src/%.c=$(BUILD_DIR)/%.o)
TARGET = reverso-linux

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

PREFIX ?= /usr/local
DESTDIR ?=

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

format:
	clang-format -i src/*.c src/*.h

.PHONY: all clean install uninstall format

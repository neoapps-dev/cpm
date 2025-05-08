CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = -lcurl

TARGET = cpm
SRC = cpm.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	mkdir -p $(DESTDIR)/usr/local/bin
	cp $(TARGET) $(DESTDIR)/usr/local/bin/
	chmod 755 $(DESTDIR)/usr/local/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: clean install uninstall all

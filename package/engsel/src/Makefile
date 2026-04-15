CC ?= gcc
CFLAGS += -Wall -Os -I./include
LDFLAGS += -lssl -lcrypto -lcurl -lm -lpthread -lqrencode

SRC := $(shell find src -name "*.c")
TARGET := engsel

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

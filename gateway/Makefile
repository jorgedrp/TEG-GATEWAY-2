CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?=
BINDIR ?= .

# Detectar automáticamente la versión de libgpiod
LIBGPIOD_VER := $(shell pkg-config --modversion libgpiod 2>/dev/null || echo "2.0.0")
LIBGPIOD_MAJOR := $(shell echo $(LIBGPIOD_VER) | cut -d. -f1)

ifeq ($(LIBGPIOD_MAJOR),2)
CFLAGS += -DUSE_LIBGPIOD_V2=1
else
CFLAGS += -DUSE_LIBGPIOD_V2=0
endif

TARGETS = standby_mode event_mode time_mode config_mode detect_mode clock test_spi

.PHONY: all clean

all: $(TARGETS)

standby_mode: standby_mode.c lora.c spi.c
	$(CC) $(CFLAGS) standby_mode.c -o $(BINDIR)/$@ -lgpiod -lpthread $(LDFLAGS)

event_mode: event_mode.c lora.c spi.c curl.c
	$(CC) $(CFLAGS) event_mode.c -o $(BINDIR)/$@ -lgpiod -lcurl -lpthread -lm $(LDFLAGS)

time_mode: time_mode.c lora.c spi.c curl.c
	$(CC) $(CFLAGS) time_mode.c -o $(BINDIR)/$@ -lgpiod -lcurl -lpthread -lm $(LDFLAGS)

config_mode: config_mode.c lora.c spi.c
	$(CC) $(CFLAGS) config_mode.c -o $(BINDIR)/$@ -lgpiod -lpthread $(LDFLAGS)

detect_mode: detect_mode.c lora.c spi.c
	$(CC) $(CFLAGS) detect_mode.c -o $(BINDIR)/$@ -lgpiod -lpthread $(LDFLAGS)

clock: clock.c lora.c spi.c
	$(CC) $(CFLAGS) clock.c -o $(BINDIR)/$@ -lgpiod -lpthread -lm $(LDFLAGS)

test_spi: test_spi.c lora.c spi.c
	$(CC) $(CFLAGS) test_spi.c -o $(BINDIR)/$@ -lgpiod -lpthread $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?=
BINDIR ?= .

TARGETS = standby_mode event_mode time_mode config_mode detect_mode clock

.PHONY: all clean

all: $(TARGETS)

standby_mode: standby_mode.c lora.c spi.c
	$(CC) $(CFLAGS) standby_mode.c -o $(BINDIR)/$@ -lwiringPi -lgpiod -lpthread $(LDFLAGS)

event_mode: event_mode.c lora.c spi.c curl.c
	$(CC) $(CFLAGS) event_mode.c -o $(BINDIR)/$@ -lwiringPi -lgpiod -lcurl -lpthread -lm $(LDFLAGS)

time_mode: time_mode.c lora.c spi.c curl.c
	$(CC) $(CFLAGS) time_mode.c -o $(BINDIR)/$@ -lwiringPi -lgpiod -lcurl -lpthread -lm $(LDFLAGS)

config_mode: config_mode.c lora.c spi.c
	$(CC) $(CFLAGS) config_mode.c -o $(BINDIR)/$@ -lwiringPi -lgpiod -lpthread $(LDFLAGS)

detect_mode: detect_mode.c lora.c spi.c
	$(CC) $(CFLAGS) detect_mode.c -o $(BINDIR)/$@ -lwiringPi -lgpiod -lpthread $(LDFLAGS)

clock: clock.c lora.c spi.c
	$(CC) $(CFLAGS) clock.c -o $(BINDIR)/$@ -lwiringPi -lgpiod -lpthread -lm $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

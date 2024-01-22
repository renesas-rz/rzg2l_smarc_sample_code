APP = can-test
SRC = can-test.c

all: $(APP)

CC ?= gcc

# Options for development
CFLAGS = -g -O0 -Wall -DDEBUG_LOG -pthread

$(APP):
	$(CC) -o $(APP) $(SRC) $(CFLAGS)

clean:
	rm -rf $(APP)


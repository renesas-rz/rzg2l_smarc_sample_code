PROJ=gpio-key-irq
SRC=irq_gpio_keys.c
LIBS=-lgpiod
CFLAGS=-D CONSUMER=\"$(PROJ)\"

all:
	$(CC) $(SRC)  $(LIBS) $(CFLAGS) -o $(PROJ)

clean:
	rm $(PROJ)

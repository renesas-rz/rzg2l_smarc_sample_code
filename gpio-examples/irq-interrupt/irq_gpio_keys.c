#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <unistd.h>
#include <gpiod.h>

#define INPUT_EVENT "/dev/input/event0"

#ifndef CONSUMER
#define CONSUMER	"Consumer"
#endif

int main() {
  //to read event
  int fd = 0;
  struct input_event ev;
  int size = sizeof(ev);
  int irq_val = 0;

  //for LED output using gpiod
  char *chipname = "gpiochip0";
	unsigned int line_num = 346;	// GPIO Pin P43_2 = 43*8 + 0 =344 //Pmod0-8
	unsigned int val;
	struct gpiod_chip *chip;
	struct gpiod_line *line;
	int i, ret;

  //event read
  fd = open(INPUT_EVENT, O_RDONLY);
  if (fd < 0) {
        printf("\nOpen " INPUT_EVENT " failed!\n");
        return 1;
  }

  //reading chip, setting output line
  chip = gpiod_chip_open_by_name(chipname);
	if (!chip) {
		perror("Open chip failed\n");
		return -1;
	}

	line = gpiod_chip_get_line(chip, line_num);
	if (!line) {
		perror("Get line failed\n");
		gpiod_chip_close(chip);
		return -1;
	}

	ret = gpiod_line_request_output(line, CONSUMER, 0);
	if (ret < 0) {
		perror("Request line as output failed\n");
		gpiod_line_release(line);
	}

  val = 0;
  while (1) {
    if (read(fd, &ev, size) < size) {
            printf("\nReading from " INPUT_EVENT " failed!\n");
            return 1;
    }
    //detect button pressed/released and blink the led off/on on each event
    if (ev.type != EV_SYN && ev.value==1) {
      ret = gpiod_line_set_value(line, val);
		  if (ret < 0) {
			  perror("Set line output failed\n");
			  gpiod_line_release(line);
		  }
      printf("Output %u on line #%u\n", val, line_num);
		  sleep(1);
		  val = !val;
    }
  }
  close(fd);
  return 0;
}
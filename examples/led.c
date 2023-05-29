#include <avrtos.h>

void toggle_led(void *arg) {
  uint8_t pin = *(int *)arg;
  uint8_t curr = LOW;

  for (;;) {
    gpio_pin_write(pin, curr);
    curr = !curr;
    task_delay(1000);
  }
}

int main(void) {
  uint8_t pin = 10;
  gpio_set_pin_mode(pin, OUTPUT);

  task_init(toggle_led, &pin, "led toggle", 128, 1);

  scheduler_init();
  return 0;
}

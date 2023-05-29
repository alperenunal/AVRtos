#include <avrtos.h>

#include <stdio.h>

void hello(void *arg) {
  (void)arg;

  for (;;) {
    printf("Hello World!\n");
    task_delay(1000);
  }
}

int main(void) {
  uart_init();

  task_init(hello, NULL, "hello task", 128, 1);

  scheduler_init();
  return 0;
}

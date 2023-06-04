#include <avrtos.h>

#include <stdio.h>

queue_t queue;

void consumer(void *arg) {
  (void)arg;

  for (;;) {
    int message;
    queue_receive(queue, &message, MAX_DELAY);
    print("Consumer: %d\n", message);
    task_delay(2000);
  }
}

void producer(void *arg) {
  int val = *(int *)arg;

  for (;;) {
    queue_send(queue, &val, MAX_DELAY);
    print("Producer: %d\n", val);
    val++;
    task_delay(1000);
  }
}

int main(void) {
  uart_init();

  int val = 0;
  queue = queue_init(4, sizeof(int));

  task_init(consumer, NULL, "consumer", 128, 1);
  task_init(producer, &val, "producer", 128, 1);

  scheduler_init();
  return 0;
}

#include <avrtos.h>

#include <stdio.h>

semaphore_t sem;

void consumer1(void *arg) {
  (void)arg;

  for (;;) {
    bool acquired = semaphore_take(sem, 1000);
    if (acquired) {
      print("Consumer 1: Acquired\n");
    } else {
      print("Consumer 1: Could not acquire\n");
    }
  }
}

void consumer2(void *arg) {
  (void)arg;

  for (;;) {
    semaphore_take(sem, MAX_DELAY);
    print("Consumer 2: Acquired\n");
  }
}

void producer(void *arg) {
  (void)arg;

  for (;;) {
    semaphore_give(sem);
    print("Producer: Released\n");
    task_delay(1500);
  }
}

int main(void) {
  uart_init();
  sem = semaphore_init(0);

  task_init(consumer1, NULL, "consumer 1", 128, 1);
  task_init(consumer2, NULL, "consumer 2", 128, 1);
  task_init(producer, NULL, "producer", 128, 1);

  scheduler_init();
  return 0;
}

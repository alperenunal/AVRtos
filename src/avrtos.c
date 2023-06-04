#include "avrtos.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/setbaud.h>

#define TASK_NAME_LENGTH 15 ///< Maximum length of task name

#define TASK_CONTEXT_SIZE 35 ///< Context size of a task

/**
 * @brief Save context of a task in its stack
 *
 */
#define SAVE_CONTEXT()                                                         \
  asm volatile("push r0\n\t"                                                   \
               "in r0, __SREG__\n\t"                                           \
               "cli\n\t"                                                       \
               "push r0\n\t"                                                   \
               "push r1\n\t"                                                   \
               "clr r1\n\t"                                                    \
               "push r2\n\t"                                                   \
               "push r3\n\t"                                                   \
               "push r4\n\t"                                                   \
               "push r5\n\t"                                                   \
               "push r6\n\t"                                                   \
               "push r7\n\t"                                                   \
               "push r8\n\t"                                                   \
               "push r9\n\t"                                                   \
               "push r10\n\t"                                                  \
               "push r11\n\t"                                                  \
               "push r12\n\t"                                                  \
               "push r13\n\t"                                                  \
               "push r14\n\t"                                                  \
               "push r15\n\t"                                                  \
               "push r16\n\t"                                                  \
               "push r17\n\t"                                                  \
               "push r18\n\t"                                                  \
               "push r19\n\t"                                                  \
               "push r20\n\t"                                                  \
               "push r21\n\t"                                                  \
               "push r22\n\t"                                                  \
               "push r23\n\t"                                                  \
               "push r24\n\t"                                                  \
               "push r25\n\t"                                                  \
               "push r26\n\t"                                                  \
               "push r27\n\t"                                                  \
               "push r28\n\t"                                                  \
               "push r29\n\t"                                                  \
               "push r30\n\t"                                                  \
               "push r31\n\t"                                                  \
               "lds r26, current_task\n\t"                                     \
               "lds r27, current_task+1\n\t"                                   \
               "in r0, __SP_L__\n\t"                                           \
               "st x+, r0\n\t"                                                 \
               "in r0, __SP_H__\n\t"                                           \
               "st x+, r0\n\t");

/**
 * @brief Restore a tasks context from its stack
 *
 */
#define RESTORE_CONTEXT()                                                      \
  asm volatile("lds r26, current_task\n\t"                                     \
               "lds r27, current_task+1\n\t"                                   \
               "ld r28, x+\n\t"                                                \
               "out __SP_L__, r28\n\t"                                         \
               "ld r29, x+\n\t"                                                \
               "out __SP_H__, r29\n\t"                                         \
               "pop r31\n\t"                                                   \
               "pop r30\n\t"                                                   \
               "pop r29\n\t"                                                   \
               "pop r28\n\t"                                                   \
               "pop r27\n\t"                                                   \
               "pop r26\n\t"                                                   \
               "pop r25\n\t"                                                   \
               "pop r24\n\t"                                                   \
               "pop r23\n\t"                                                   \
               "pop r22\n\t"                                                   \
               "pop r21\n\t"                                                   \
               "pop r20\n\t"                                                   \
               "pop r19\n\t"                                                   \
               "pop r18\n\t"                                                   \
               "pop r17\n\t"                                                   \
               "pop r16\n\t"                                                   \
               "pop r15\n\t"                                                   \
               "pop r14\n\t"                                                   \
               "pop r13\n\t"                                                   \
               "pop r12\n\t"                                                   \
               "pop r11\n\t"                                                   \
               "pop r10\n\t"                                                   \
               "pop r9\n\t"                                                    \
               "pop r8\n\t"                                                    \
               "pop r7\n\t"                                                    \
               "pop r6\n\t"                                                    \
               "pop r5\n\t"                                                    \
               "pop r4\n\t"                                                    \
               "pop r3\n\t"                                                    \
               "pop r2\n\t"                                                    \
               "pop r1\n\t"                                                    \
               "pop r0\n\t"                                                    \
               "out __SREG__, r0\n\t"                                          \
               "pop r0");

/**
 * @brief Representation of task.
 *
 */
struct task {
  uint8_t *stack_top;              ///< Top of tasks stack (sp)
  uint8_t *stack;                  ///< Start of stack (low address)
  uint8_t priority;                ///< Priority of task
  task_state_t state;              ///< Current state of task
  uint16_t wake_tick;              ///< Tick which the tasks should be waken
  void *channel;                   ///< Channel that tasks blocked/suspended
  char name[TASK_NAME_LENGTH + 1]; ///< Name of the task (for debugging)
};

/**
 * @brief Priority queue for tasks
 *
 */
typedef struct task_queue {
  uint8_t length;   ///< Length of queue
  uint8_t capacity; ///< Capacity of queue
  task_t *tasks;    ///< Tasks
} task_queue_t;

/**
 * @brief Semaphore for mutex
 *
 */
struct semaphore {
  uint8_t count; ///< Count of semaphore
  void *channel; ///< Channel of semaphore
};

/**
 * @brief FIFO queue for inter-task communication
 *
 */
struct queue {
  uint8_t length;    ///< Length of queue
  uint8_t capacity;  ///< Maximum capacity of queue
  uint8_t item_size; ///< Size of items stored in queue
  uint8_t *items;    ///< Buffer to store items

  uint8_t read_waiting;  ///< Number of task waiting to read
  uint8_t write_waiting; ///< Number of task waiting to write

  void *read_channel;  ///< Read channel
  void *write_channel; ///< Write channel
};

static task_t current_task; ///< Currently running task

static task_queue_t ready_tasks; ///< Queue of ready tasks

static task_queue_t blocked_tasks; ///< Queue of blocked tasks

static task_queue_t suspended_tasks; ///< Queue of suspended tasks

static uint16_t global_tick_count; ///< Tick count from the start of scheduler

static semaphore_t uart_sem;

static inline double log2(double x) { return log(x) / M_LN2; }

static int pow2(int x) {
  int res = 1;
  for (int i = 0; i < x; i++) {
    res *= 2;
  }
  return res;
}

/**
 * @brief Initialize the stack of task
 *
 * @param sp Address the stack pointer points to
 * @param fn Function the task runs
 * @param arg Argument passed to function
 */
static void task_stack_init(uint8_t *sp, uintptr_t fn, uintptr_t arg) {
  *sp = fn & 0x00ff;
  sp--;
  *sp = (fn >> 8) & 0x00ff;
  sp--;

  *sp = 0;
  sp--;
  *sp = 0x80;
  sp--;
  *sp = 0;
  sp--;

  for (int i = 2; i < 24; i++) {
    *sp = i;
    sp--;
  }

  *sp = arg & 0xff;
  sp--;
  *sp = (arg >> 8) & 0xff;
  sp--;

  for (int i = 26; i < 32; i++) {
    *sp = i;
    sp--;
  }
}

/**
 * @brief Create a task
 *
 * @param fn Function the task runs
 * @param arg Argument passed to function
 * @param name Name of the task (For debugging)
 * @param stack_size Size of the tasks stack
 * @param priority Priority of the task
 * @return task_t
 */
static task_t task_create(void (*fn)(void *), void *arg, const char *name,
                          size_t stack_size, uint8_t priority) {
  task_t task = malloc(sizeof(*task));
  if (task == NULL) {
    goto task_error;
  }

  uint8_t *stack = malloc(stack_size);
  if (stack == NULL) {
    goto stack_error;
  }
  uint8_t *stack_top = stack + stack_size - 1;
  task_stack_init(stack_top, (uintptr_t)fn, (uintptr_t)arg);
  stack_top -= TASK_CONTEXT_SIZE;

  task->stack = stack;
  task->stack_top = stack_top;
  task->priority = priority;
  task->state = READY;
  task->channel = NULL;
  strncpy(task->name, name, TASK_NAME_LENGTH);
  task->name[TASK_NAME_LENGTH] = '\0';

  return task;

stack_error:
  free(task);
task_error:
  return NULL;
}

/**
 * @brief Swap two tasks
 *
 * @param x
 * @param y
 */
static void task_swap(task_t *x, task_t *y) {
  task_t tmp = *x;
  *x = *y;
  *y = tmp;
}

/**
 * @brief Bubble up the higher priority task
 *
 * @param queue Task priority queue
 * @param idx Initial index of the task
 */
static void task_queue_bubble_down(task_queue_t *queue, uint8_t idx) {
  while (2 * idx + 1 < queue->length) {
    uint8_t left_child = 2 * idx + 1;
    uint8_t right_child = 2 * idx + 2;
    uint8_t max_child = (queue->tasks[left_child]->priority >
                         queue->tasks[right_child]->priority)
                            ? left_child
                            : right_child;
    if (queue->tasks[idx]->priority < queue->tasks[max_child]->priority) {
      task_swap(&queue->tasks[idx], &queue->tasks[max_child]);
      idx = max_child;
    } else {
      break;
    }
  }
}

/**
 * @brief Bubble down the lower priority task
 *
 * @param queue Task priority queue
 * @param idx Initial index of the task
 */
static void task_queue_bubble_up(task_queue_t *queue, uint8_t idx) {
  while (idx > 0 &&
         queue->tasks[idx]->priority > queue->tasks[(idx - 1) / 2]->priority) {
    task_swap(&queue->tasks[idx], &queue->tasks[(idx - 1) / 2]);
    idx = (idx - 1) / 2;
  }
}

/**
 * @brief Insert task into priority queue
 *
 * @param queue Task queue
 * @param task Task
 * @return bool
 */
static bool task_queue_insert(task_queue_t *queue, task_t task) {
  uint8_t level = log2(queue->length + 1);
  uint8_t exp_cap = (uint8_t)(pow2(level + 1)) - 1;

  if (queue->capacity < exp_cap) {
    task_t *tmp = realloc(queue->tasks, sizeof(task_t) * exp_cap);
    if (tmp == NULL) {
      return false;
    }
    queue->tasks = tmp;
    queue->capacity = exp_cap;
  }

  queue->tasks[queue->length] = task;
  task_queue_bubble_up(queue, queue->length);
  queue->length++;

  return true;
}

/**
 * @brief Remove a task from the queue
 *
 * @param queue Task queue
 * @param task Task
 * @return bool
 */
static bool task_queue_delete(task_queue_t *queue, task_t task) {
  uint8_t i;
  for (i = 0; i < queue->length; i++) {
    if (queue->tasks[i] == task) {
      break;
    }
  }

  if (i == queue->length) {
    return false;
  }

  queue->length--;
  queue->tasks[i] = queue->tasks[queue->length];
  task_queue_bubble_down(queue, i);

  return true;
}

/**
 * @brief Top of the queue
 *
 * @param queue Task queue
 * @return task_t
 */
static task_t task_queue_top(task_queue_t *queue) {
  if (queue->length == 0) {
    return NULL;
  }
  return queue->tasks[0];
}

/**
 * @brief Create a task and put it into ready tasks queue
 *
 * @param fn Function the task runs
 * @param arg Argument passed to function
 * @param name Name of the task (For debugging)
 * @param stack_size Size of the tasks stack
 * @param priority Priority of the task
 * @return task_t
 */
task_t task_init(void (*fn)(void *), void *arg, const char *name,
                 size_t stack_size, uint8_t priority) {
  task_t task = task_create(fn, arg, name, stack_size, priority);
  if (task == NULL) {
    goto task_error;
  }

  if (!task_queue_insert(&ready_tasks, task)) {
    goto insert_error;
  }

  return task;

insert_error:
  task_destroy(task);
task_error:
  return NULL;
}

/**
 * @brief Deallocate tasks recourses
 *
 * @param task Task handle
 */
void task_destroy(task_t task) {
  cli();

  if (task != NULL) {
    switch (task->state) {
    case READY:
      task_queue_delete(&ready_tasks, task);
      break;
    case BLOCKED:
      task_queue_delete(&blocked_tasks, task);
      break;
    case SUSPENDED:
      task_queue_delete(&suspended_tasks, task);
      break;
    case RUNNING:
      current_task = task_queue_top(&ready_tasks);
      current_task->state = RUNNING;
      RESTORE_CONTEXT();
      asm volatile("reti");
    }

    free(task->stack);
    free(task);
  }

  sei();
}

/**
 * @brief Yield the execution of the task
 *
 */
static void task_yield(void) __attribute__((naked));
static void task_yield(void) {
  SAVE_CONTEXT();

  current_task = task_queue_top(&ready_tasks);
  current_task->state = RUNNING;

  RESTORE_CONTEXT();
  asm volatile("reti");
}

/**
 * @brief Delay the task for specified milliseconds
 *
 * @param ms Milliseconds to delay
 */
void task_delay(uint16_t ms) {
  cli();
  current_task->wake_tick = global_tick_count + ms / 10;
  current_task->state = BLOCKED;
  task_queue_delete(&ready_tasks, current_task);
  task_queue_insert(&blocked_tasks, current_task);
  task_yield();
}

/**
 * @brief Block on channel
 *
 * @param chan Channel to block on
 */
static void task_block(void *chan) {
  current_task->channel = chan;
  current_task->state = BLOCKED;
  task_queue_delete(&ready_tasks, current_task);
  task_queue_insert(&blocked_tasks, current_task);
  task_yield();
}

/**
 * @brief Suspend on channel
 *
 * @param chan Channel to suspend on
 */
static void task_suspend(void *chan) {
  current_task->channel = chan;
  current_task->state = SUSPENDED;
  task_queue_delete(&ready_tasks, current_task);
  task_queue_insert(&suspended_tasks, current_task);
  task_yield();
}

/**
 * @brief Wake all the tasks blocked/suspended on channel
 *
 * @param chan Block/Suspend channel
 */
static void task_wake(void *chan) {
  for (uint8_t i = 0; i < blocked_tasks.length; i++) {
    task_t task = blocked_tasks.tasks[i];
    if (task->channel == chan) {
      task->state = READY;
      task->channel = NULL;
      task_queue_delete(&blocked_tasks, task);
      task_queue_insert(&ready_tasks, task);
      i--;
    }
  }

  for (uint8_t i = 0; i < suspended_tasks.length; i++) {
    task_t task = suspended_tasks.tasks[i];
    if (task->channel == chan) {
      task->state = READY;
      task->channel = NULL;
      task_queue_delete(&suspended_tasks, task);
      task_queue_insert(&ready_tasks, task);
      i--;
    }
  }
}

/**
 * @brief Create a semaphore
 *
 * @param count Initial count of semaphore
 * @return semaphore_t
 */
semaphore_t semaphore_init(uint8_t count) {
  semaphore_t sem = malloc(sizeof(*sem));
  if (sem == NULL) {
    goto sem_error;
  }

  void *channel = malloc(1);
  if (channel == NULL) {
    goto chan_error;
  }

  sem->count = count;
  sem->channel = channel;

  return sem;

chan_error:
  free(sem);
sem_error:
  return NULL;
}

/**
 * @brief Wait to acquire semaphore. If timeout is MAX_DELAY suspend until
 * acquire, else block to acquire until timeout expires
 *
 * @param sem Semaphore
 * @param timeout Time of block or MAX_DELAY for suspending
 * @return true
 * @return false
 */
bool semaphore_take(semaphore_t sem, uint16_t timeout) {
  cli();

  if (timeout != MAX_DELAY) {
    current_task->wake_tick = global_tick_count + timeout / 10;
  }

  while (sem->count == 0) {
    if (timeout != MAX_DELAY) {
      if (current_task->wake_tick <= global_tick_count) {
        sei();
        return false;
      }
      task_block(sem->channel);
    } else {
      task_suspend(sem->channel);
    }
  }

  sem->count--;
  sei();
  return true;
}

/**
 * @brief Give back the semaphore
 *
 * @param sem Semaphore
 */
void semaphore_give(semaphore_t sem) {
  cli();
  sem->count++;
  task_wake(sem->channel);
  sei();
}

/**
 * @brief Deallocate the resources of semaphore
 *
 * @param sem Semaphore
 */
void semaphore_destroy(semaphore_t sem) {
  cli();
  task_wake(sem->channel);
  if (sem != NULL) {
    free(sem->channel);
    free(sem);
  }
  sei();
}

/**
 * @brief Create a FIFO queue
 *
 * @param capacity Capacity of queue
 * @param item_size Size of items going to be stored in queue
 * @return queue_t
 */
queue_t queue_init(uint8_t capacity, uint8_t item_size) {
  queue_t queue = malloc(sizeof(*queue));
  if (queue == NULL) {
    goto queue_error;
  }

  queue->capacity = capacity;
  queue->item_size = item_size;
  queue->items = calloc(capacity, item_size);
  queue->read_waiting = 0;
  queue->write_waiting = 0;
  queue->read_channel = malloc(1);
  queue->write_channel = malloc(1);

  if (queue->items == NULL) {
    goto items_error;
  }

  return queue;

items_error:
  free(queue);
queue_error:
  return NULL;
}

/**
 * @brief Send to back of queue
 *
 * @param queue Message queue
 * @param item Message
 * @param timeout Timeout
 * @returns bool
 */
bool queue_send(queue_t queue, void *item, uint16_t timeout) {
  cli();
  if (timeout != MAX_DELAY) {
    current_task->wake_tick = global_tick_count + timeout / 10;
  }
  queue->write_waiting++;

  while (queue->length == queue->capacity) {
    if (timeout != MAX_DELAY) {
      if (current_task->wake_tick <= global_tick_count) {
        queue->write_waiting--;
        sei();
        return false;
      }
      task_block(queue->write_channel);
    } else {
      task_suspend(queue->write_channel);
    }
  }

  memcpy(queue->items + queue->length * queue->item_size, item,
         queue->item_size);
  queue->length++;
  queue->write_waiting--;

  if (queue->read_waiting) {
    task_wake(queue->read_channel);
  }

  sei();
  return true;
}

/**
 * @brief Receive from front of the queue
 *
 * @param queue Message queue
 * @param item Message
 * @param timeout Timeout
 * @return bool
 */
bool queue_receive(queue_t queue, void *item, uint16_t timeout) {
  cli();
  if (timeout != MAX_DELAY) {
    current_task->wake_tick = global_tick_count + timeout / 10;
  }
  queue->read_waiting++;

  while (queue->length == 0) {
    if (timeout != MAX_DELAY) {
      if (current_task->wake_tick <= global_tick_count) {
        queue->read_waiting--;
        sei();
        return false;
      }
      task_block(queue->read_channel);
    } else {
      task_suspend(queue->read_channel);
    }
  }

  queue->length--;
  memcpy(item, queue->items, queue->item_size);
  memmove(queue->items, queue->items + queue->item_size,
          queue->item_size * queue->length);
  memset(queue->items + queue->item_size * queue->length, 0, queue->item_size);
  queue->read_waiting--;

  if (queue->write_waiting) {
    task_wake(queue->write_channel);
  }

  sei();
  return true;
}

/**
 * @brief Deallocate queues resources
 *
 * @param queue Queue to deallocate
 */
void queue_destroy(queue_t queue) {
  cli();
  task_wake(queue->read_channel);
  task_wake(queue->write_channel);

  if (queue) {
    free(queue->write_channel);
    free(queue->read_channel);
    free(queue->items);
    free(queue);
  }
  sei();
}

/**
 * @brief Wake up expired tasks
 *
 */
static void wake_expired_tasks(void) {
  for (uint8_t i = 0; i < blocked_tasks.length; i++) {
    task_t task = blocked_tasks.tasks[i];
    if (task->wake_tick <= global_tick_count) {
      task->state = READY;
      task_queue_delete(&blocked_tasks, task);
      task_queue_insert(&ready_tasks, task);
      i--;
    }
  }
}

/**
 * @brief Timer interrupt ISR
 *
 */
ISR(TIMER1_COMPA_vect, ISR_NAKED) {
  SAVE_CONTEXT();

  global_tick_count++;
  if (global_tick_count % 100 == 0) {
    system_tick();
  }

  wake_expired_tasks();

  current_task->state = READY;
  task_queue_delete(&ready_tasks, current_task);
  task_queue_insert(&ready_tasks, current_task);

  current_task = task_queue_top(&ready_tasks);
  current_task->state = RUNNING;

  RESTORE_CONTEXT();
  asm volatile("reti");
}

/**
 * @brief Set the timer interrupt object
 *
 */
static void set_timer_interrupt(void) {
  TCCR1B = (1 << CS10) | (1 << CS12) | (1 << WGM12);
  OCR1A = 16000000 / 1024 / 100; // 10ms
  TIMSK1 = (1 << OCIE1A);
  sei();
}

/**
 * @brief Idle task function
 *
 * @param arg
 */
static void idle_task_fn(void *arg) {
  (void)arg;
  for (;;)
    ;
}

/**
 * @brief Start scheduler
 *
 * @warning If initialization is successful this function does not return
 */
void scheduler_init(void) {
  task_t idle_task = task_init(idle_task_fn, NULL, "idle task", 64, 0);
  if (idle_task == NULL) {
    return;
  }

  current_task = idle_task;
  set_timer_interrupt();

  for (;;)
    ;
}

/**
 * @brief Write a character in stream
 *
 * @param c Character to write
 * @param stream Stream to write
 * @return int
 */
static int uart_putchar(char c, FILE *stream) {
  if (c == '\n') {
    uart_putchar('\r', stream);
  }
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

/**
 * @brief Read a character from stream
 *
 * @param stream Stream to read
 * @return int
 */
static int uart_getchar(FILE *stream) {
  (void)stream;
  loop_until_bit_is_set(UCSR0A, RXC0);
  return UDR0;
}

static FILE uart_output =
    FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
static FILE uart_input =
    FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);

/**
 * @brief Initialize the UART driver
 *
 */
void uart_init(void) {
  uart_sem = semaphore_init(1); // error handle

  UBRR0H = UBRRH_VALUE;
  UBRR0L = UBRRL_VALUE;

  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);

  stdout = &uart_output;
  stdin = &uart_input;
}

/**
 * @brief Set pin mode
 *
 * @param pin
 * @param mode
 */
void gpio_set_pin_mode(uint8_t pin, gpio_pin_mode mode) {
  uint8_t direction = (mode == OUTPUT);
  uint8_t pin_value = (mode == INPUT_PULLUP);

  if (pin < 8) {
    PORTD |= (pin_value << pin);
    DDRD |= (direction << pin);
  } else if (pin < 14) {
    PORTB |= (pin_value << (pin - 8));
    DDRB |= (direction << (pin - 8));
  } else if (pin <= A5) {
    PORTC |= (pin_value << (pin - 14));
    DDRC |= (direction << (pin - 14));
  }
}

/**
 * @brief Write low or high to a pin
 *
 * @param pin
 * @param value
 */
void gpio_pin_write(uint8_t pin, uint8_t value) {
  if (pin < 8) {
    if (value) {
      PORTD = PORTD | (1 << pin);
    } else {
      PORTD = PORTD & (0 << pin);
    }
  } else if (pin < 14) {
    if (value) {
      PORTB = PORTB | (1 << (pin - 8));
    } else {
      PORTB = PORTB & (0 << (pin - 8));
    }
  } else if (pin <= A5) {
    if (value) {
      PORTC = PORTC | (1 << (pin - 14));
    } else {
      PORTC = PORTC & (0 << (pin - 14));
    }
  }
}

/**
 * @brief Read the pin
 *
 * @param pin
 * @return uint8_t
 */
uint8_t gpio_pin_read(uint8_t pin) {
  uint8_t value = 0;

  if (pin < 8) {
    value = PIND & (1 << pin);
  } else if (pin < 14) {
    value = PINB & (1 << (pin - 8));
  } else if (pin <= A5) {
    value = PINC & (1 << (pin - 14));
  }

  return value;
}

int print(const char *fmt, ...) {
  semaphore_take(uart_sem, MAX_DELAY);
  va_list args;
  va_start(args, fmt);

  int bytes = vprintf(fmt, args);

  va_end(args);
  semaphore_give(uart_sem);
  return bytes;
}

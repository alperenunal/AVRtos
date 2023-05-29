#ifndef AVRTOS_H
#define AVRTOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief delay to indicate blocking call should suspend the task instead
 * without timeout
 *
 */
#define MAX_DELAY 0xffff

#define HIGH 1 ///< High voltage
#define LOW 0  ///< Low voltage

/**
 * @brief State of a task
 *
 */
typedef enum task_state {
  RUNNING,
  READY,
  BLOCKED,
  SUSPENDED,
} task_state_t;

/**
 * @brief Arduino UNO analog pings
 *
 */
enum { A0 = 14, A1, A2, A3, A4, A5 };

/**
 * @brief Pin modes
 *
 */
typedef enum {
  OUTPUT,
  INPUT,
  INPUT_PULLUP,
  INPUT_PULLDOWN,
} gpio_pin_mode;

typedef struct task *task_t;

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
                 size_t stack_size, uint8_t priority);

/**
 * @brief Delay the task for specified milliseconds
 *
 * @param ms Milliseconds to delay
 */
void task_delay(uint16_t ms);

/**
 * @brief Deallocate tasks recourses
 *
 * @param task Task handle
 */
void task_destroy(task_t task);

typedef struct semaphore *semaphore_t;

/**
 * @brief Create a semaphore
 *
 * @param count Initial count of semaphore
 * @return semaphore_t
 */
semaphore_t semaphore_init(uint8_t count);

/**
 * @brief Wait to acquire semaphore. If timeout is MAX_DELAY suspend until
 * acquire, else block to acquire until timeout expires
 *
 * @param sem Semaphore
 * @param timeout Time of block or MAX_DELAY for suspending
 * @return true
 * @return false
 */
bool semaphore_take(semaphore_t sem, uint16_t ms);

/**
 * @brief Give back the semaphore
 *
 * @param sem Semaphore
 */
void semaphore_give(semaphore_t sem);

/**
 * @brief Deallocate the resources of semaphore
 *
 * @param sem Semaphore
 */
void semaphore_destroy(semaphore_t sem);

typedef struct queue *queue_t;

/**
 * @brief Create a FIFO queue
 *
 * @param capacity Capacity of queue
 * @param item_size Size of items going to be stored in queue
 * @return queue_t
 */
queue_t queue_init(uint8_t capacity, uint8_t item_size);

/**
 * @brief Send to back of queue
 *
 * @param queue Message queue
 * @param item Message
 * @param timeout Timeout
 * @returns bool
 */
bool queue_send(queue_t queue, void *item, uint16_t timeout);

/**
 * @brief Receive from front of the queue
 *
 * @param queue Message queue
 * @param item Message
 * @param timeout Timeout
 * @return bool
 */
bool queue_receive(queue_t queue, void *item, uint16_t timeout);

/**
 * @brief Deallocate queues resources
 *
 * @param queue Queue to deallocate
 */
void queue_destroy(queue_t queue);

/**
 * @brief Start scheduler
 *
 * @warning If initialization is successful this function does not return
 */
void scheduler_init(void);

/**
 * @brief Initialize the UART driver
 *
 */
void uart_init(void);

/**
 * @brief Set pin mode
 *
 * @param pin
 * @param mode
 */
void gpio_set_pin_mode(uint8_t pin, gpio_pin_mode mode);

/**
 * @brief Write low or high to a pin
 *
 * @param pin
 * @param value
 */
void gpio_pin_write(uint8_t pin, uint8_t value);

/**
 * @brief Read the pin
 *
 * @param pin
 * @return uint8_t
 */
uint8_t gpio_pin_read(uint8_t pin);

#endif

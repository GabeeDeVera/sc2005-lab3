#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define MAX_SEMAPHORES 32

struct semaphore {
  struct spinlock lock;
  int value;
  int in_use;
};

struct semaphore semaphores[MAX_SEMAPHORES];
struct spinlock sem_table_lock;

void
seminit(void)
{
  initlock(&sem_table_lock, "sem_table");
  for(int i = 0; i < MAX_SEMAPHORES; i++) {
    semaphores[i].in_use = 0;
    initlock(&semaphores[i].lock, "semaphore");
  }
}

int
sem_alloc(int value)
{
  acquire(&sem_table_lock);
  for(int i = 0; i < MAX_SEMAPHORES; i++) {
    if(!semaphores[i].in_use) {
      semaphores[i].in_use = 1;
      semaphores[i].value = value;
      release(&sem_table_lock);
      return i;
    }
  }
  release(&sem_table_lock);
  return -1;  // No free semaphores
}

int
sem_free(int semid)
{
  if(semid < 0 || semid >= MAX_SEMAPHORES)
    return -1;
  
  acquire(&sem_table_lock);
  semaphores[semid].in_use = 0;
  release(&sem_table_lock);

  return 0;
}

// ! NOTE TO SELF: recall that -sem.value represents the number of processes in the waiting queue
// ! meanwhile, if sem.value = 1, no waiting processes, but if sem.value = 0, there is some waiting process
int
sem_wait(int semid)
{
  acquire(&sem_table_lock);
  if(semid < 0 || semid >= MAX_SEMAPHORES || !semaphores[semid].in_use) {
    release(&sem_table_lock);
    return -1;
  }
  
  // Retrieve semaphore: uncomment below
  struct semaphore *sem = &semaphores[semid];
  release(&sem_table_lock); // ! We are done accessing the semaphore table, release
  
  // TODO decrement semaphore value atomically
  acquire(&sem->lock); // ! Forgot to acquire lock before entering critical section, silly
  
  // ! Use while instead of if (ask gemini/GPT why)
  // ! ah, while loop forces the semaphore to recheck the condition
  /*
  ! This is VERY subtle. in general, just remember that you need to recheck
  ! the condition after sleep because of the lock release and reacquisition in between

  Consider the following scenario if you used "if" instead:
  1. Producer 1 waits until an empty slot (granted immediately, 1 -> 0)
  2. Producer 2 waits until an empty slot (sleeps, empty = 0)
  3. Consumer reads (signals, 0 -> 1, both producer 1 and 2 are RUNNABLE)
  4. Producer 1 is granted time first and writes another thing (buffer now full)
  5. Producer 2 blindly assumes there's still an empty slot (race condition)
  */
  while(sem->value <= 0) {
    // sleep when semaphore is zero
    // ! Note to self: sleep automatically releases lk before blocking
    // ! then re-acquires it before proceeding
    sleep(sem, &sem->lock); 
  }
  sem->value--;
  release(&sem->lock);
  return 0;
}

int
sem_signal(int semid)
{
  acquire(&sem_table_lock); // ! NOTE: I added this
  if(semid < 0 || semid >= MAX_SEMAPHORES || !semaphores[semid].in_use) {
    release(&sem_table_lock);
    return -1;
  }
  
  // Retrieve semaphore: uncomment below
  struct semaphore *sem = &semaphores[semid];
  release(&sem_table_lock); // ! We are done accessing the semaphore table, release
  
  // increment semaphore value atomically, notifying sleeping processes
  acquire(&sem->lock);
  // ! NOTE TO SELF: sem->value <= 0 implies that there are processes waiting for the semaphore. wake them up!
  if(sem->value <= 0) {
    // ! NOTE: wakeup sleeping processes
    // ! NOTE: void* chan is a pointer to a struct with the condition data we want to check
    wakeup(sem);
  }
  sem->value++;
  release(&sem->lock);
  return 0;
}

int
sem_getvalue(int semid)
{
  acquire(&sem_table_lock);
  if(semid < 0 || semid >= MAX_SEMAPHORES || !semaphores[semid].in_use) {
    release(&sem_table_lock);
    return -1;
  }
  // TODO return value of semid atomically
  // ! NOTE: get the return value here from inside the critical section
  struct semaphore *sem = &semaphores[semid];
  release(&sem_table_lock);
  acquire(&sem->lock);
  int cur_val = semaphores[semid].value;
  // ! Then, release the lock
  release(&sem->lock);
  // ! Finally, return the obtained value outside the critical section
  return cur_val;
}

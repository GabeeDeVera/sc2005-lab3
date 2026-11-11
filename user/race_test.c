#include "user.h"

#define T_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("Assertion failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            exit(1); \
        } \
    } while (0)


int N = 1000;

void sem_basic() {
    printf("[sem_basic] Initializing semaphore to 1...\n");
    int semid = sem_init(1);
    T_ASSERT(sem_getvalue(semid) == 1);

    printf("[sem_basic] Waiting (should decrement to 0)...\n");
    sem_wait(semid);
    T_ASSERT(sem_getvalue(semid) == 0);

    printf("[sem_basic] Posting (should increment to 1)...\n");
    sem_signal(semid);
    T_ASSERT(sem_getvalue(semid) == 1);

    sem_free(semid);
    printf("[sem_basic] PASS: Basic semaphore operations work.\n");
}

// ! custom semaphore test that iterates sem_basic 1000 times
void sem_iter() {
    int semid = sem_init(1);
    T_ASSERT(sem_getvalue(semid) == 1);

    for(int i = 0; i < 1000; i++) {
      sem_wait(semid);
      T_ASSERT(sem_getvalue(semid) == 0);

      sem_signal(semid);
      T_ASSERT(sem_getvalue(semid) == 1);
    }

    sem_free(semid);
    printf("[sem_iter] PASS: Basic semaphore operations work.\n");
}

void do_work(int pid, int semid) {
    for (int i = 0; i < N; i++) {
        // ! increment a shared counter, making sure to use avoid race condition.
        // Tip: use a semaphore.
        sem_wait(semid);
        int val = ucnt_get(0);
        ucnt_set(0, val + 1);
        sem_signal(semid);
    }
}

void race_test() {
    ucnt_set(0, 0);
    int semid = sem_init(1); // ! create semaphore properly

    int pid = fork();
    T_ASSERT(pid >= 0);
    if (pid == 0) { // child
        do_work(pid, semid);
        exit(0);
    } else { // parent
        do_work(getpid(), semid);
        wait(0);
        sem_free(semid); // ! forgot to free the semaphore. free the semaphore lmao
        int final = ucnt_get(0);
        printf("[race_test] final counter=%d (expected %d)\n", final, 2*N);
        T_ASSERT(final == 2*N);
        printf("[race_test] PASS: consistent counter\n");
    }

}    

struct buf_sem {
    // ! added semaphores as required
    int full_semid; // tbh, a more intuitive name would be "occupied" or "notempty" to signify that this semaphore signals exactly when there is at least one thing written
    int empty_semid;
    int mutex_semid;
};

void buf_sem_free(struct buf_sem b) {
  sem_free(b.full_semid);
  sem_free(b.empty_semid);
  sem_free(b.mutex_semid);
}

void consumer(struct buf_sem b, int loops, int valid[]) {
    char tmp;
    for(int i = 0; i < loops; i++) {
        // TODO: wait buffer slot full and signal empty slot
        sem_wait(b.full_semid);
        sem_wait(b.mutex_semid); 
        tmp = ubuf_read();
        char str[2] = {tmp, '\0'};
        printf("%s", str);
        printf("%d", (int)tmp);
        T_ASSERT(valid[(unsigned char)tmp]);
        sem_signal(b.mutex_semid);
        sem_signal(b.empty_semid);
    }
}

void producer(const char* msg, struct buf_sem b) {
    for (const char* p = msg; *p != '\0'; p++) {
        // TODO: wait buffer slot emtpy and signal used slot
        sem_wait(b.empty_semid);
        sem_wait(b.mutex_semid);
        ubuf_write(*p);
        sem_signal(b.mutex_semid);
        sem_signal(b.full_semid);
    }
}

void producer_consumer() {
    #define MSG_SIZE 48
    const char msg[MSG_SIZE + 1] = "cafefacefacefacefacefacefacefacefacecafecafecafe";
    const int NUM_PROD = 2;
    int valid[256] = {0};
    for (const char* p = msg; *p != '\0'; p++) {
        unsigned char uc = (unsigned char)*p;
        valid[uc] = 1;
    }

    // ! init semaphores as required
    // struct buf_sem b = {
    //   .full_semid = sem_init(0),
    //   .empty_semid = sem_init(UBUF_SIZE),
    //   .mutex_semid = sem_init(1),
    // };
    struct buf_sem b;
    b.full_semid = sem_init(0);
    b.empty_semid = sem_init(UBUF_SIZE);
    b.mutex_semid = sem_init(1);

    printf("%d, %d, %d\n", b.full_semid, b.empty_semid, b.mutex_semid);

    for (int i = 0; i < NUM_PROD; i++) {
        int pid = fork();
        T_ASSERT(pid >= 0);
        if (pid == 0) { // child
            producer(msg, b);
            exit(0);
        }
    }

    consumer(b, MSG_SIZE * NUM_PROD, valid);
    for (int i = 0; i < NUM_PROD; i++) {
        wait(0);
    }
    printf("[producer_consumer] PASS: message delivered\n");
    // TODO clean-up semaphores
    buf_sem_free(b);
}



int main() {
    sem_basic();

    sem_iter();
    
    race_test();

    producer_consumer(); // ! TODO implement (4.1)

    printf("ALL TESTS PASSED\n");

    exit(0);
}

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// shared variables
int r1 = 0, r2 = 0;
int X = 0, Y = 0;
int Z = 0;

volatile int jump_completed = 0;
volatile bool test_running = true;

// Thread 1: Executes a conditional jump in a while loop branching condition
// with potential for reordering across conditional jumps
void *thread_1(void* arg) {
  while (test_running) {
    X = 0;
    Y = 0;
    r1 = 0;

    // Create memory barrier to ensure resets are visible
    __sync_synchronize();

    do {
      X = 1;
      // conditional jump
    } while(Z == 0);
    r1 = Y;

    jump_completed = 1;
  }
  return NULL;
}

// Thread 2: Detects occurrences of reorderings
void *thread_2(void *arg) {
  int observed_anomalies = 0;
  int total_tests = 0;

  while (test_running) {
    // Make sure while loop branching condition fails
    Z = 1;

    // Memory barrier to make sure Z write is visible
    __sync_synchronize();

    // Wait for thread 1 to execute the jump
    while (jump_completed == 0) {
#if defined(__x86_64__) || defined(__i386__)
      // x86/x86-64 architecture
      __asm__ volatile("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
      // ARM architecture
      __asm__ volatile("yield" ::: "memory");
#elif defined(__riscv)
      // RISC-V architecture
      __asm__ volatile("pause" ::: "memory");
#else
      // Force compilation to fail with an error message
      #error "Unsupported architecture for spin-wait instruction"
#endif
    }

    Y = 1;
#if defined(__x86_64__) || defined(__i386__)
      // x86/x86-64 architecture
      __asm__ volatile("mfence" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
      // ARM architecture
      __asm__ volatile("dmb" ::: "memory");
#elif defined(__riscv)
      // RISC-V architecture
      __asm__ volatile("fence" ::: "memory");
#else
      // Force compilation to fail with an error message
      #error "Unsupported architecture for spin-wait instruction"
#endif
    r2 = X;

    // Check for reordering anomaly
    // If r2=0 (meaning X=1 from before the jump hasn't been observed yet)
    // but r1=1 (meaning Y=1 was observed after the jump)
    // then the instruction before the conditional jump has been reordered over
    // the conditional jump
    if (r2 == 0 && r1 == 1) {
      observed_anomalies++;
      printf("Anomaly detected! r2(X)=%d, r1(Y)=%d - instructions reordered across conditional jump\n", r2, r1);
    }

    total_tests++;

    jump_completed = 0;

    if (total_tests % 1000 == 0) {
      printf("Completed %d tests, observed %d anomalies (%.4f%%)\n",
          total_tests, observed_anomalies,
          (float)observed_anomalies / total_tests * 100);
    }
  }

  printf("Final results: completed %d tests, observed %d anomalies (%.4f%%)\n",
      total_tests, observed_anomalies,
      (float)observed_anomalies / total_tests * 100);

  return NULL;
}

int main() {
  pthread_t t1, t2;

  printf("Litmus test for reordering of instructions across conditional jumps\n\n");

  pthread_create(&t1, NULL, thread_1, NULL);
  pthread_create(&t2, NULL, thread_2, NULL);

  printf("Running test for 10 seconds...\n\n");
  sleep(10);

  test_running = false;

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  return 0;
}

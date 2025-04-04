#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

// Shared variables
int X = 0;
int Y = 0;
int r1 = 0;
int r2 = 0;
volatile int iterations_completed = 0;
volatile bool test_running = true;

// Thread 1: Executes a loop with potential for reordering across iterations
void* thread_1(void* arg) {
    while (test_running) {
        // Reset variables
        X = 0;
        Y = 0;
        r1 = 0;
        
        // Create a memory barrier to ensure resets are visible
        __sync_synchronize();
        
        // Execute loop with potential for reordering across iterations
        for (int i = 0; i < 2; i++) {
            if (i == 0) {
                X = 1;        // Store in first iteration
            } else {
                r1 = Y;       // Load in second iteration that might be reordered before the first iteration's store
            }
        }
        
        // Signal that iterations are completed
        iterations_completed = 1;
    }
    
    return NULL;
}

// Thread 2: Tests if reordering happened
void* thread_2(void* arg) {
    int observed_anomalies = 0;
    int total_tests = 0;
    
    while (test_running) {
        // Wait for thread 1 to execute its loop iterations
        while (iterations_completed == 0) {
            __asm__ volatile("pause" ::: "memory");
        }
        
        // Now set Y=1
        Y = 1;
        
        // Read final value of X
        r2 = X;
        
        // Check for reordering anomaly:
        // If r2=0 (meaning X=1 from first iteration wasn't observed) 
        // but r1=1 (meaning Y=1 was observed in the second iteration)
        // then the second iteration's load happened before the first iteration's store
        if (r2 == 0 && r1 == 1) {
            observed_anomalies++;
            printf("Anomaly detected! r2(X)=%d, r1(Y)=%d - instructions reordered across iterations\n", r2, r1);
        }
        
        total_tests++;
        
        // Reset for next test
        iterations_completed = 0;
        
        // Print progress every 1000 tests
        if (total_tests % 1000 == 0) {
            printf("Completed %d tests, observed %d anomalies (%.4f%%)\n", 
                   total_tests, observed_anomalies, 
                   (float)observed_anomalies / total_tests * 100);
        }
    }
    
    printf("Final results: %d anomalies out of %d tests (%.4f%%)\n", 
           observed_anomalies, total_tests, 
           (float)observed_anomalies / total_tests * 100);
    
    return NULL;
}

int main() {
    pthread_t t1, t2;
    
    printf("Litmus Test for Loop Iteration Boundary Reordering\n");
    printf("------------------------------------------------\n");
    printf("This test checks if instructions from different loop iterations\n");
    printf("can be reordered by the processor or compiler.\n\n");
    
    // Create threads
    pthread_create(&t1, NULL, thread_1, NULL);
    pthread_create(&t2, NULL, thread_2, NULL);
    
    // Run test for 10 seconds
    printf("Running test for 10 seconds...\n\n");
    sleep(10);
    
    // Stop test
    test_running = false;
    
    // Wait for threads to finish
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    
    return 0;
}

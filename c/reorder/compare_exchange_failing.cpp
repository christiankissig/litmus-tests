/**
 * Litmus Test for Reordering of Instructions over the Failing Branch of a CAS
 * 
 * This test checks whether a processor/compiler reorders instructions across
 * the failing branch of a Compare-and-Swap operation, which can lead to
 * subtle concurrency bugs in lock-free algorithms.
 */

#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <cassert>

// Shared variables
std::atomic<int> X{0};
std::atomic<int> Y{0};
std::atomic<int> Z{0};
std::atomic<bool> ready{false};
std::atomic<int> iterations_completed{0};

// Results collection
struct TestResult {
    int x_final;
    int y_final;
    int z_final;
    bool cas_successful;
};

std::vector<TestResult> results;
std::atomic<bool> test_complete{false};

void thread_A() {
    while (!ready.load(std::memory_order_acquire)) {
        // Wait for the starting signal
    }
    
    // This is the core of the test
    // The read of Y, which happens before the CAS in program order,
    // should not be reordered to happen after a failed CAS on X
    int y_observed = Y.load(std::memory_order_relaxed);
    
    // Try to CAS X from 0 to 1, which may fail if thread_B has already changed X
    int expected = 0;
    bool cas_result = X.compare_exchange_strong(expected, 1, std::memory_order_acq_rel);
    
    // This store is intended to happen after the CAS, whether it succeeds or fails
    Z.store(42, std::memory_order_relaxed);
    
    // Record results
    results.push_back({X.load(std::memory_order_relaxed), 
                       y_observed, 
                       Z.load(std::memory_order_relaxed),
                       cas_result});
                       
    iterations_completed.fetch_add(1, std::memory_order_relaxed);
}

void thread_B() {
    while (!ready.load(std::memory_order_acquire)) {
        // Wait for the starting signal
    }
    
    // First change X, which will cause thread_A's CAS to fail
    X.store(2, std::memory_order_relaxed);
    
    // Then change Y, which thread_A might observe
    // depending on whether reads are reordered across the failing CAS
    Y.store(1, std::memory_order_relaxed);
    
    iterations_completed.fetch_add(1, std::memory_order_relaxed);
}

void analyze_results(const std::vector<TestResult>& results) {
    int reordering_observed = 0;
    int total_iterations = results.size();
    
    for (const auto& result : results) {
        // If CAS failed (expected) AND Y was observed as 1 AND X is 2
        // This means the load of Y was reordered to happen after the failed CAS
        if (!result.cas_successful && result.y_final == 1 && result.x_final == 2) {
            reordering_observed++;
            std::cout << "Reordering detected in iteration: Y=" << result.y_final 
                      << ", X=" << result.x_final 
                      << ", Z=" << result.z_final << std::endl;
        }
    }
    
    std::cout << "Reordering observed in " << reordering_observed << " out of " 
              << total_iterations << " iterations (" 
              << (100.0 * reordering_observed / total_iterations) << "%)" << std::endl;
              
    if (reordering_observed > 0) {
        std::cout << "RESULT: The system allows reordering of instructions across the failing branch of a CAS." << std::endl;
    } else {
        std::cout << "RESULT: No reordering of instructions observed across the failing branch of a CAS." << std::endl;
        std::cout << "Note: Absence of evidence is not evidence of absence. More iterations might be needed." << std::endl;
    }
}

int main() {
    const int NUM_ITERATIONS = 10000;
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Reset shared variables
        X.store(0, std::memory_order_relaxed);
        Y.store(0, std::memory_order_relaxed);
        Z.store(0, std::memory_order_relaxed);
        iterations_completed.store(0, std::memory_order_relaxed);
        
        // Create threads
        std::thread t1(thread_A);
        std::thread t2(thread_B);
        
        // Start the test
        ready.store(true, std::memory_order_release);
        
        // Wait for both threads to complete
        t1.join();
        t2.join();
        
        // Reset ready flag for next iteration
        ready.store(false, std::memory_order_relaxed);
        
        // Ensure both threads completed their work
        assert(iterations_completed.load(std::memory_order_relaxed) == 2);
    }
    
    // Analyze all collected results
    analyze_results(results);
    
    return 0;
}

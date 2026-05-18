#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // ─────────────────────────────────────────────────────
    // Error tests — run BEFORE the infinite ping-pong
    // (otherwise we never reach them)
    // ─────────────────────────────────────────────────────
    
    printf("=== Error tests ===\n");
    
    // Test 1: yield to a non-existent PID
    int r = co_yield(9999, 1);
    printf("co_yield(9999, 1) returned %d (expected -1)\n", r);
    
    // Test 2: yield to self
    r = co_yield(getpid(), 1);
    printf("co_yield(self, 1) returned %d (expected -1)\n", r);
    
    // Test 3: invalid PID (zero)
    r = co_yield(0, 1);
    printf("co_yield(0, 1) returned %d (expected -1)\n", r);
    
    // Test 4: negative PID
    r = co_yield(-5, 1);
    printf("co_yield(-5, 1) returned %d (expected -1)\n", r);
    
    // Test 5: yield to a killed process
    int killed_pid = fork();
    if (killed_pid == 0) {
        // Child just exits immediately, becoming a zombie
        exit(0);
    }
    // Wait briefly so the child has time to exit
    sleep(5);
    r = co_yield(killed_pid, 1);
    printf("co_yield(killed_pid=%d, 1) returned %d (expected -1)\n", killed_pid, r);
    wait(0);  // clean up the zombie
    
    printf("=== Error tests done ===\n");
    printf("=== Starting infinite ping-pong ===\n");
    
    // ─────────────────────────────────────────────────────
    // Infinite ping-pong (as in the assignment specification)
    // ─────────────────────────────────────────────────────
    
    int pid1 = getpid();      // parent PID
    int pid2 = fork();        // create child
    
    if (pid2 == 0) {
        // Child
        for (;;) {
            int value = co_yield(pid1, 1);
            printf("Child received: %d\n", value);   // should print 2
        }
    } else {
        // Parent
        for (;;) {
            int value = co_yield(pid2, 2);
            printf("parent received: %d\n", value);  // should print 1
        }
    }
    
    exit(0);   // unreachable, but compiler-friendly
}
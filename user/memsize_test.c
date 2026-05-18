#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int size;
    void *ptr;

    // (a) print how many bytes of memory the process is using
    size = memsize();
    printf("Memory size before allocation: %d bytes\n", size);

    // (b) allocate 20K bytes of memory using malloc
    ptr = malloc(20000);
    if (ptr == 0) {
        printf("Memory allocation failed\n");
        exit(1);
    }
    // (c) print how many bytes of memory the process is using again
    size = memsize();
    printf("Memory size after allocation: %d bytes\n", size);
    // (d) free the allocated memory
    free(ptr);
    // (e) print how many bytes of memory the process is using again
    size = memsize();
    printf("Memory size after freeing: %d bytes\n", size);
    exit(0);
}

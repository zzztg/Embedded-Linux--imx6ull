#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    fprintf(stderr, "new program stderr: pid=%ld; stderr was not redirected\n",
            (long)getpid());
    printf("new program: pid=%ld, ppid=%ld, argc=%d\n",
           (long)getpid(), (long)getppid(), argc);
    for (int index = 0; index < argc; index++) {
        printf("argv[%d]=%s\n", index, argv[index]);
    }
    printf("new program: returning 7\n");
    return 7;
}

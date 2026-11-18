#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>

int main(void)
{
    printf("stdout: pid=%ld\n", (long)getpid());
    fflush(stdout);
    fprintf(stderr, "stderr: pid=%ld\n", (long)getpid());
    printf("stdout: work complete\n");
    fflush(stdout);
    fprintf(stderr, "stderr: diagnostic complete\n");
    return 7;
}

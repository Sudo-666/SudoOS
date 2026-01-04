#include "lib/stdio.h"
#include "lib/syscall.h"


void shell_main();

int usrmain() {
    printf("\n");
    printf("================================\n");
    printf("        Welcome to SudoOS       \n");
    printf("================================\n");

int pid = fork();
    if(pid == 0) {
        printf("[System Check] Fork test: Child (PID %d) works.\n", getpid());
        exit(0);
    } else {
        int status;
        wait4(pid, &status, 0, 0);
        printf("[System Check] Fork test: Parent received child exit.\n");
    }

    printf("Starting Shell...\n");
    shell_main();

    while(1);
    return 0;
}

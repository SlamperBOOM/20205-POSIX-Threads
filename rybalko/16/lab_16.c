#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>

#define NUM_STRINGS 10

sem_t* sem_parent = SEM_FAILED;
sem_t* sem_child = SEM_FAILED;

void CloseAllSem() {
    if (sem_parent != SEM_FAILED) {
        if (sem_close(sem_parent) < 0) {
            perror("Failed while SEM_CLOSE(3) for sem_parent.");
        }
    }

    if (sem_child != SEM_FAILED) {
        if (sem_close(sem_child) < 0) {
            perror("Failed while SEM_CLOSE(3) for sem_child.");
        }
    }
}

void UnlinkAllSem() {
    if (sem_parent != SEM_FAILED) {
        if (sem_unlink("/ParentSem") < 0) {
            perror("Failed while SEM_UNLINK(3) for sem_parent.");
        }
    }

    if (sem_child != SEM_FAILED) {
        if (sem_unlink("/ChildSem") < 0) {
            perror("Failed while SEM_UNLINK(3) for sem_child.");
        }
    }
}

int main() {
    sem_parent = sem_open("/ParentSem", O_CREAT, 0777, 1);
    sem_child = sem_open("/ChildSem", O_CREAT, 0777, 0);

    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        perror("Failed to SEM_OPEN(3)");
        CloseAllSem();
        UnlinkAllSem();
        exit(-1);
    }

    switch (fork()) {
        case -1:
            perror("FORK(2) failed");
            CloseAllSem();
            UnlinkAllSem();
            exit(-1);
        case 0:
            for (int i = 1; i <= NUM_STRINGS; i++) {
                sem_wait(sem_parent);
                printf("CHILD:  %d\n", i);
                sem_post(sem_child);
            }
            break;
        default:
            for (int i = 1; i <= NUM_STRINGS; i++) {
                sem_wait(sem_child);
                printf("PARENT: %d\n", i);
                sem_post(sem_parent);
            }

            if (wait(NULL) < 0) {
                perror("Failed while WAIT(2)");
            }

            CloseAllSem();
            UnlinkAllSem();
            break;
    }
    return 0;
}

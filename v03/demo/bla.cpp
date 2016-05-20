/* Interprocess communication */
#include <sys/ipc.h>
#include <sys/shm.h>

#include "tasklab.h"

int main (int argc, char* argv[]) {
    key_t    shmKEY;
    int      shmID;
    TaskLab* tl;

    shmKEY = ftok(".", 'x');
    shmID = shmget(shmKEY, sizeof(TaskLab), 0666);

    if (shmID < 0) {
        std::cout << "Couldn't create shared memory in client.\n";

        return 1;
    }

    tl = (TaskLab*) shmat(shmID, NULL, 0);

    if ((int64_t) tl == -1) {
        std::cout << "Couldn't open shared memory in client.\n";

        return 1;
    }

    std::cout << tl << std::endl;

    if (tl->hasEvent(Evt::TASK)) {
        printf("yayyy!\n");
    } else {
        printf("nope...\n");
    }

    shmdt((void *) tl);
}
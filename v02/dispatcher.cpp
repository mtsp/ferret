#include "tasklab.h"

#include <iostream>

int main(int argc, char* argv[]) {
    char* filename;
    char c;

    TaskLab tl;

    // Check if the argument is valid to proceed
    if ((c = getopt (argc, argv, "f:")) != EOF) {
        filename = optarg;
    } else {
        std::cout << "Invalid argument, please try again.\n";
        std::cout << "usage: dispatcher -f filename\n";

        exit(1);
    }

    // Read graph
    tl.restore(filename);

    tl.dispatch(RT::MTSP);

    return 0;
}

#include "common.h"
#include "dispatcher.h"

#include <kmp.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    char* filename;
    char c;
    Graph* graph;

    // Check if the argument is valid to proceed
    if ((c = getopt (argc, argv, "f:")) != EOF) {
        filename = optarg;
    } else {
        std::cout << "Invalid argument, please try again.\n";
        std::cout << "usage: dispatcher -f filename\n";

        exit(1);
    }

    // Read graph
    Graph::restore(&graph, filename);

    Dispatcher::dispatch(graph);

    delete graph;

    return 0;
}

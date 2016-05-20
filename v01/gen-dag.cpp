/**
 *
 * Program that generates a directed acyclic graph 
 * to be taken as a task graph.
 *
 * Example: gen-dag -t -m [-f] [-d] [-l]
 * -t is the number of tasks to be generated;
 * -m is the maximum number of IN/INOUT dependencies that
 * has to be created on each task;
 * -f is the filename of where the graph will be saved (without extensions);
 * -d is how far a predecessor may be from a parent;
 * -l is the range of load time per task.
 *
 * */

#include "gen-dag.h"

#include <iostream>

void usage() {
    std::cout << "Invalid argument, please try again.\n";
    std::cout << "usage: gen-dag -n -m [-f] [-d] [-t] [-r]\n";
    std::cout << "\n-n\tnumber of tasks to be generated;\n";
    std::cout << "-m\tmaximum number of IN/INOUT dependencies\n";
    std::cout << "\tthat has to be created on each task;\n";
    std::cout << "-f\tname of filename that will be saved (OPTIONAL);\n";
    std::cout << "-d\thow far a predecessor may be from a parent (OPTIONAL);\n";
    std::cout << "-t\tstandard load time per task, in ms (OPTIONAL);\n";
    std::cout << "-r\tmax. range from standard load time (0-1).\n\n";
}

int main (int argc, char* argv[]) {
    Graph*  graph;
    char*   filename = DEFAULT_NAME;

    // Read arguments and intialize structures
    {
        uint    dep_range  = DEFAULT_DEP_RANGE,
                load_time  = DEFAULT_LOAD_TIME,
                num_tasks  = 0,
                max_dep    = 0;

        float   load_range = DEFAULT_LOAD_RANGE;

        char    c;

        // Check if the argument is valid to proceed
        while ((c = getopt (argc, argv, "n:m:f:d:t:r:")) != -1) {
            switch (c) {
                case 'n':
                    num_tasks = atoi(optarg);
                    break;
                case 'm':
                    max_dep = atoi(optarg);
                    break;
                case 'f':
                    filename = optarg;
                    break;
                case 'd':
                    dep_range = atoi(optarg);
                    break;
                case 't':
                    load_time = atoi(optarg);
                    break;
                case 'r':
                    load_range = atoi(optarg);
                    break;
                default:
                    usage();

                    exit (1);
                    break;
            }
        }   

        // If essential arguments weren't set
        if (!max_dep || !num_tasks) {
            usage();

            exit(1);
        }

        // Generate graph
        Gen_dag::generate(&graph, num_tasks, max_dep, dep_range, load_time, 
                          load_range);
    }

    // save .dot
    Graph::show(*graph, filename);

    // serialize
    Graph::save(*graph, filename);

    delete graph;

    return 0;
}
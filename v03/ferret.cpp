/** 
 * ferret.cpp
 *   a tasklab application
 *
 *   usage: simply start as "./ferret" and follow instructions
 **/

#include "tasklab.h"

#include <iostream>

/* Interprocess communication */
#include <sys/ipc.h>
#include <sys/shm.h>

/* Definitions */
#define INVALID 0
#define EXIT   -1

typedef enum { UINT, FLOAT, RUNTIME, EVENT, PLOT, TRACE, BURNIN } tp;
typedef enum { APP = 1, TG = 2 } tr;
typedef enum { RANDOM = 1, DATA = 2 } bi;

/* ************************
 * Software interface
 * ************************ */
/* Print instructions for software usage */
void instructions() {
    std::cout << "Available options:\n";
    std::cout << " \"generate\" or \"g\" in order to generate a random task graph;\n";
    std::cout << " \"run\"      or \"r\" in order to run a current loaded task graph;\n";
    std::cout << " \"burnin\"   or \"b\" in order to generate multiple task graphs and run them;\n";
    std::cout << " \"trace\"    or \"t\" in order to trace a program;\n";
    std::cout << " \"save\"     or \"s\" to save a current loaded task graph;\n";
    std::cout << " \"restore\"  or \"x\" to restore and load a saved task graph;\n";
    std::cout << " \"plot\"     or \"p\" to plot a current loaded task graph.\n";
}

/* Set welcome message */
void welcome() {
    std::cout << "Welcome to FERRET, a TaskLab application!\n";
    instructions();
    std::cout << "For help, type \"help\" or \"h\".\n";
}

/* Wait for input */
void wait() {
    std::cout << "(ferret) ";
}

/* Undefined command default */
void undefined(const char* s) {
    std::cout << "Undefined command: \"" << s << "\". Try \"help\" or \"h\".\n";
}

/* ************************
 * Helper functions regarding input validation
 * ************************ */
/* Check if a value is INVALID, else assign default value 
 *  r: value to be checked
 *  df: default value
 * */
template<typename T>
void check(T* r, T df) {
    if (*r == INVALID) {
        *r = df;
    }
}

/* Get correct input and assign it
 *  str: instructions
 *  opt: if the input is optional or not (i.e. if can be INVALID)
 *  t:   type of the input
 *  r:   value to receive input 
 * */
template<typename T>
int read(const char* str, bool opt, uint8_t t, T* r) {
    char buf[256];
    bool valid = false;

    /* Initialize as fail */
    *r = false;

    /* Get valid input */
    while(!valid) {
        /* Print instructions */
        std::cout << str;

        /* Get input */
        fgets(buf, 256, stdin);

        /* Get rid of garbage */
        buf[strlen(buf) - 1] = '\0';

        /* Check case */
        if (t == tp::UINT) {
            *r = atoi(buf);

        } else if (t == tp::FLOAT) {
            *r = atof(buf);

        } else if (t == tp::RUNTIME) {
            if (strcasecmp("MTSP", buf) == 0) {
                *r = RT::MTSP;
            }

        } else if (t == tp::EVENT) {
            if (strcasecmp("HIGH TASK", buf) == 0 || 
                strcasecmp("HTASK", buf) == 0) {
                *r = Evt::HTASK;
            } else if (strcasecmp("LOW TASK", buf) == 0 ||
                strcasecmp("LTASK", buf) == 0) {
                *r = Evt::LTASK;
            }

        } else if (t == tp::PLOT) {
            if (strcasecmp("DOT", buf) == 0) {
                *r = Plot::DOT;
            }
            else if (strcasecmp("LOW LEVEL", buf) == 0 ||
                     strcasecmp("LL", buf) == 0) {
                *r = Plot::LL;

            } else if (strcasecmp("INFO", buf) == 0) {
                *r = Plot::INFO;

            }

        } else if (t == tp::TRACE) {
            if (strcasecmp("APPLICATION", buf) == 0 ||
                strcasecmp("APP", buf) == 0 ||
                strcasecmp("A", buf) == 0) {
                *r = tr::APP;
            }
            else if (strcasecmp("TASKGRAPH", buf) == 0 ||
                     strcasecmp("T", buf) == 0) {
                *r = tr::TG;

            }
        } else if (t == tp::BURNIN) {
            if (strcasecmp("Random", buf) == 0 || 
                strcasecmp("r", buf) == 0) {
                *r = bi::RANDOM;
            } else if (strcasecmp("Data", buf) == 0 ||
                strcasecmp("d", buf) == 0) {
                *r = bi::DATA;
            }
        }

        if (*r == INVALID && !opt) {
            /* If user is willing to quit this option */
            if (strcasecmp("q", buf) == 0) {
                return EXIT;
            }

            std::cout << "\t\t\"" << buf << "\" is an invalid input.\n";
        } else {
            valid = true;
        }
    }

    return 0;
}


/* ************************
 * Main
 * ************************ */
int main (int argc, char* argv[]) {
    TaskLab tl;
    char    buf[256], c;

    welcome();

    while(true) {
        wait();

        fgets(buf, 256, stdin);

        /* Get argument */
        c = buf[0];

        switch (c) {
            case 'g':
                /* Generate task graph */
                {
                uint32_t dep_range,
                         exec_time,
                         num_tasks,
                         max_dep;

                float    exec_range;

                sprintf(buf, "\tNumber of tasks to be generated: ");
                if (read(buf, false, tp::UINT, &num_tasks) == EXIT) {
                    break;
                }

                sprintf(buf, "\tMaximum number of IN/INOUT dependencies: ");
                if (read(buf, false, tp::UINT, &max_dep) == EXIT) {
                    break;
                }

                sprintf(buf, "\tHow far a predecessor may be from a parent: \
(OPTIONAL, default is %d) ", DEFAULT_DEP_RANGE);
                if (read(buf, true, tp::UINT, &dep_range) == EXIT) {
                    break;
                }

                check(&dep_range, DEFAULT_DEP_RANGE);

                sprintf(buf, "\tStandard execution per task, i.e. amount of \
iterations: (OPTIONAL, default is %d) ", DEFAULT_EXECUTION_SIZE);
                if (read(buf, true, tp::UINT, &exec_time) == EXIT) {
                    break;
                }

                check(&exec_time, DEFAULT_EXECUTION_SIZE);

                sprintf(buf, "\tMax. range from standard execution size (0-1): \
(OPTIONAL, default is %0.2f) ", DEFAULT_EXECUTION_RANGE);
                if (read(buf, true, tp::FLOAT, &exec_range) == EXIT) {
                    break;
                }

                check(&exec_range, DEFAULT_EXECUTION_RANGE);

                // Generate graph
                tl.generate(num_tasks, max_dep, dep_range, exec_time, 
                            exec_range);

                std::cout << "Task graph successfully generated!\n";
                }

                break;

            case 'r':
                {
                uint8_t rt;

                sprintf(buf, "\tRuntime to be run: ");
                if (read(buf, false, tp::RUNTIME, &rt) == EXIT) {
                    break;
                }

                /* Run! */
                tl.run(rt);
                }

                break;

            case 'b':
                {
                uint8_t bi_t;

                sprintf(buf, "\tRandom or data (randomly generates task graphs or stress from existing data): ");
                if (read(buf, false, tp::BURNIN, &bi_t) == EXIT) {
                    break;
                }

                if (bi_t == RANDOM) {
                    uint32_t nruns;
                    uint32_t max_t;
                    uint8_t  rt;

                    sprintf(buf, "\tNumber of graphs to be generated: ");
                    if (read(buf, false, tp::UINT, &nruns) == EXIT) {
                        break;
                    }

                    sprintf(buf, "\tMax. no. of tasks that a graph may obtain: ");
                    if (read(buf, false, tp::UINT, &max_t) == EXIT) {
                        break;
                    }

                    sprintf(buf, "\tRuntime that will be used for dispatching: ");
                    if (read(buf, false, tp::RUNTIME, &rt) == EXIT) {
                        break;
                    }

                    tl.burnin(nruns, max_t, rt);

                } else {
                    char a_path[256];
                    uint32_t nruns;
                    uint8_t  rt;

                    std::cout << "\tPath of the database: ";
                    std::cin >> a_path;

                    /* Garbage */
                    getchar();

                    sprintf(buf, "\tMax. no. of iterations per file: ");
                    if (read(buf, false, tp::UINT, &nruns) == EXIT) {
                        break;
                    }

                    sprintf(buf, "\tRuntime that will be used for dispatching: ");
                    if (read(buf, false, tp::RUNTIME, &rt) == EXIT) {
                        break;
                    }

                    tl.burnin(a_path, nruns, rt);

                }
                }

                break;

            case 't':
                {
                uint8_t o, e, rt;
                char    a_path[128];
                char    a_arg[128];

                sprintf(buf, "\tType of tracing (taskgraph or application): ");
                if (read(buf, false, tp::TRACE, &o) == EXIT) {
                    break;
                }

                /* Trace an application */
                if (o == APP) {
                    /* Get application path */
                    std::cout << "\tApplication to be traced (full path): ";
                    std::cin >> a_path;

                    /* Garbage */
                    getchar();

                    /* Get application path */
                    std::cout << "\tApplication arguments (OPTIONAL): ";
                    fgets(a_arg, 128, stdin);

                    /* Garbage */
                    a_arg[strlen(a_arg) - 1] = '\0';
                }

                /* Get event type */
                sprintf(buf, "\tEvent to be watched (high task or low task): ");
                if (read(buf, false, tp::EVENT, &e) == EXIT) {
                    break;
                }

                sprintf(buf, "\tRuntime to be loaded into execution: ");
                if (read(buf, false, tp::RUNTIME, &rt) == EXIT) {
                    break;
                }

                /* Set environment variable */
                sprintf(buf, "%d", e);
                setenv(EVT_VAR, buf, true);

                /* Set cmd */
                sprintf(buf, "%s %s", a_path, a_arg);

                if (o == APP) {
                    /* Run application! */
                    system(buf);
                } else {
                    /* Run taskgraph! */
                    tl.run(rt);
                }

                /* Clean environment variable */
                setenv(EVT_VAR, "", true);

                /* Restore created object */
                sprintf(buf, "%s%s", TMPDIR, DEFAULT_NAME);
                tl.restore(buf);
                }

                break;

            case 's':
                {
                std::cout << "\tSave task graph as (without extension): ";
                std::cin >> buf;

                /* Garbage */
                getchar();

                if (tl.save(buf)) {
                    std::cout << "Task graph successfully saved as \"" << buf 
                              << ".dat\".\n";
                }
                }

                break;

            case 'x':
                {
                std::cout << "\tTask graph to be restored (without extension): ";
                std::cin >> buf;

                /* Garbage */
                getchar();

                if (tl.restore(buf)) {
                    std::cout << "Task graph successfully restored.\n";
                }
                }

                break;

            case 'p':
                {
                uint8_t pt;
                char instr[256];

                std::cout << "\tPlot task graph as (without extension): ";
                std::cin >> buf;

                /* Garbage */
                getchar();

                sprintf(instr, "\tPlot type (dot, low level or info): ");
                if (read(instr, false, tp::PLOT, &pt) == EXIT) {
                    break;
                }

                if (tl.plot(buf, pt)) {
                    std::cout << "Task graph successfully plotted as \"" << buf;

                    if (pt == Plot::DOT) {
                        std::cout << "_00xx.dot\"\n";
                    } else if (pt == Plot::LL) {
                        std::cout << ".tsk\"\n";
                    } else {
                        std::cout << ".info\"\n";
                    }
                }
                }

                break;

            case 'h':
                /* Help*/
                instructions();

                break;

            case '\n':
                /* None */
                break;

            case 'Q':
            case 'q':
                /* Quit application */
                return 0;

            default:
                /* Skip newline */
                buf[strlen(buf) - 1] = '\0';

                /* Undefined command, try again */
                undefined(buf);

                break;
        }
    }

    return 0;
}
/**
 * Tasklab.cpp
 *   API implementation
 *
 * */

#include "tasklab.h"

#include <cstdio>
#include <dlfcn.h>              // find function symbols

#include <boost/filesystem.hpp> // burnin utilities

/* ************************
 * File management
 * ************************ */
namespace fs = boost::filesystem;

/* ************************
 * Dispatch function symbols
 * ************************ */
typedef void  (*fc_t)(ident*, kmp_int32, kmpc_micro, ...);
typedef void *(*ta_t)(ident*, kmp_int32, kmp_int32, kmp_uint32, kmp_uint32, kmp_routine_entry);
typedef void  (*td_t)(ident*, kmp_int32, kmp_task*, kmp_int32, kmp_depend_info*, kmp_int32, kmp_depend_info*);
typedef void  (*tw_t)(ident*, kmp_int32);

typedef void  (*tp_t)();

fc_t fork_call          = NULL;
ta_t omp_task_alloc     = NULL;
td_t omp_task_with_deps = NULL;
tw_t omp_taskwait       = NULL;

tp_t pretty_dump        = NULL;

/* ***************
 * Task structure handler
 *   -- for tasklab own INTERNAL validation!
 * *************** */
bool _task::hasdep(uint32_t ID) {
    /* Iterate over dependencies in order to try to find the ID */
    std::list<_dep>::iterator it;
    for (it = successors.begin(); 
         it != successors.end(); ++it) {
        if (it->dID == ID) {
            return true;
        }
    }

    /* None was found */
    return false;
}

/* ************************
 * Task graph builders
 * ************************ */
TaskGraph::TaskGraph() : TaskGraph(0, DEFAULT_DEP_RANGE,
                                   DEFAULT_EXECUTION_SIZE, 
                                   DEFAULT_EXECUTION_RANGE) {};

TaskGraph::TaskGraph(const uint32_t n, const uint32_t d,
                     const uint32_t t, float r)
         : ntasks(n), dep_r(d), exec_t(t), max_r(r) {
    tasks.resize(n);
    ll.resize(0);

    ndeps = 0;       // initialize num of deps as none
    nvar  = 0;       // initialize num of var as none
}

void TaskGraph::create_tasks(const uint32_t max_dep) {
    /* Keep in track with the id of each dependency */
    uint32_t dep_id = 0;

    /* -- Initialize tasks construction */
    /* First task */
    tasks[0].tID = 0;

    // Randomly set load time of task (negative or positive)
    tasks[0].exec = (rand() % 2) == 0 ? 1 : -1;
    tasks[0].exec *= (float)(rand() % 100)/100 * max_r;

    /* Iterate over tasks */
    for (uint32_t i = 1; i < ntasks; i++) {
        uint32_t range_min, range_max;
        uint32_t npred;

        // Set task ID, i.e. its access at task graph
        tasks[i].tID = i;

        // Get the max numbers of predecessors as possible
        uint32_t cur_dep = i <= max_dep ? i - 1 : max_dep;

        // Define minimum index of dependency
        if (i < dep_r)
            range_min = 0;
        else
            range_min = i - dep_r;

        // Define maximum index of dependency
        if (range_min + dep_r >= i)
            range_max = i;
        else
            range_max = range_min + dep_r;

        // are soon-to-be parents able to take care of the no. of dep.?
        cur_dep = std::min(cur_dep, range_max - range_min);

        // Randomly set load time of task (negative or positive)
        tasks[i].exec = (rand() % 2) == 0 ? 1 : -1;
        tasks[i].exec *= (float)(rand() % 100)/100 * max_r;

        // Generate a number in the range 1 to cur_dep
        npred = cur_dep == 0 ? 1 : rand() % cur_dep + 1;

        // Allocate list of predecessors
        tasks[i].predecessors.resize(npred);

        // Describe each dependency of the current task
        describe_deps(tasks[i].tID, &dep_id, range_min, range_max);

        // Increment total dependencies
        ndeps += npred;

        // By default, no. of variables := no. of deps
        nvar  = ndeps;
    }
}

void TaskGraph::describe_deps(const uint32_t tID, uint32_t* dep_id, 
                              const uint32_t min, const uint32_t max) {
    // Keep track of results
    std::vector<uint32_t> results;

    // For each predecessor, define a task and the dependency type
    std::list<_dep>::iterator it;
    for (it = tasks[tID].predecessors.begin(); 
        it != tasks[tID].predecessors.end(); ++it) {
        // Define a task that hasn't been picket yet
        do {
            it->task = min + (rand() % (max - min));
        } while (std::find(results.begin(), results.end(), it->task) 
            != results.end());

        // Set as already used
        results.push_back(it->task);

        // Set a random type for the dependency, either IN or INOUT
        it->type = Type(rand() % 2);
        it->dID = it->var = *dep_id;

        // Set itself as a successor dependency (in case someone relies 
        // on this variable, aka *dep_id for us)
        _dep c_dep;                      // current dependency
        c_dep.task = 0;                  // irrelevant
        c_dep.type = it->type;           // type of current dependency
        c_dep.dID = c_dep.var = *dep_id; // current id

        // Add it to itself
        tasks[tID].successors.push_back(c_dep);

        // "Backwards" dependency with its parent, parent earns a new 
        // variable to write on!
        _dep b_dep;
        b_dep.task = tID;
        b_dep.type = Type::OUT;
        b_dep.dID = b_dep.var = *dep_id;

        // Set it as a successor at predecessor task
        tasks[it->task].successors.push_back(b_dep);

        (*dep_id)++;
    }
}

/* ***************
 * Trace handlers
 * *************** */
void TaskGraph::add_task(task t) {
    _task f_t; // final task to be added

    /* Translates task to _task */
    f_t.exec  = 0;      // for now, set as default

    f_t.tID   = ntasks; // id of task

#ifdef DEBUG
    printf("iterate over task %d...\n", f_t.tID);
#endif

    /* Now, the tricky part: manage dependencies! */
    for (int c = 0; c < t.ndeps; ++c) {
        /* Address of dependency variable */
        uint64_t cur_ptr  = t.deparr[c].varptr;
        uint8_t  cur_mode = t.deparr[c].mode;
        uint32_t cur_var  = nvar;
        uint32_t cur_dep  = ndeps;

        /// If the new task is writing (i.e. different from IN), it must:
        ///     if there are previous readers
        ///         become dependent of all previous readers
        ///     if there is no previous reader
        ///         become dependent of the last writer
        ///     become the new "producer", i.e., the last task writing
        ///     reset the readers from the last writing
        if (cur_mode != Type::IN) {
#ifdef DEBUG
            printf("\ti'm a writer!\n");
#endif

            /* If there was a previous reader */
            if (in_map.find(cur_ptr) != in_map.end()) {
                /* Become dependent of all previous readers and adjust cur_var */
                std::vector<_dep>::iterator it;
                for (it = in_map[cur_ptr].begin(), cur_var = it->var; 
                     it != in_map[cur_ptr].end(); ++it) {
                    uint32_t p_tID = it->task; // parent task ID
                    uint32_t p_dID = it->dID;  // parent dep. ID

                    /* Set cur_var according to parent */
                    cur_var = out_map[cur_ptr].var;

                    /* Predecessor */
                    _dep p_dep = {
                        p_tID,     // task that is heading towards to (parent)
                        cur_mode,  // children dependency type
                        p_dID,     // parent dependency ID
                        cur_var    // var ID
                    };

                    /* Add predecessor */
                    f_t.predecessors.push_back(p_dep);
                }

            } else if (out_map.find(cur_ptr) != out_map.end()) {
                /* No previous reader. If there is a last writer... */
                uint32_t p_tID = out_map[cur_ptr].task; // parent task ID
                uint32_t p_dID = out_map[cur_ptr].dID;  // parent dep. ID

#ifdef DEBUG
                printf("\ti have a father, at %d!\n", p_tID);
#endif

                /* Set cur_var according to parent */
                cur_var = out_map[cur_ptr].var;

                /* Become dependent  of last writer */
                /* Predecessor */
                _dep p_dep = {
                    p_tID,       // task that is heading towards to (parent)
                    cur_mode,    // children dependency type
                    p_dID,       // parent dependency ID
                    cur_var      // var ID
                };

                /* Add predecessor */
                f_t.predecessors.push_back(p_dep);
            }

            /* Becomes the new "producer", i.e. the last task writing */
            out_map[cur_ptr].task = f_t.tID;
            out_map[cur_ptr].type = cur_mode;
            out_map[cur_ptr].dID  = cur_dep;
            out_map[cur_ptr].var  = cur_var;

#ifdef DEBUG
            printf("\tand my cur_ptr is %lu!\n\n", cur_ptr);
#endif

            /* Reset readers */
            if (in_map.find(cur_ptr) != in_map.end()) {
                in_map[cur_ptr].clear();
                in_map.erase(cur_ptr);
            }

        } else {
            /// If we are reading:
            ///     - if there was a previous producer/writer the new task become 
            ///       dependent
            ///     -       if not, the new task does not have dependences
            ///     - is always added to the set of last readers

            /* Is there a previous writer? */
            if (out_map.find(cur_ptr) != out_map.end()) {
                uint32_t p_tID = out_map[cur_ptr].task; // parent task ID
                uint32_t p_dID = out_map[cur_ptr].dID;  // parent dep. ID

                cur_var = out_map[cur_ptr].var;

                /* Predecessor */
                _dep p_dep = {
                    p_tID,       // task that is heading towards to (parent)
                    cur_mode,    // children dependency type
                    p_dID,       // parent dependency ID
                    cur_var
                };

                /* Add predecessor */
                f_t.predecessors.push_back(p_dep);
            }

            /* Add to the set of last readers */
            in_map[cur_ptr].push_back({f_t.tID, cur_mode, cur_dep, cur_var});
        }

        /* -- add variable as a dependency to our final task */
        /* Successor dependency */
        _dep s_dep = {
            0,                     // irrelevant, since it doesnt have to rely
                                   // on a children task
            cur_mode,              // parent dependency type
            cur_dep,               // parent dependency ID
            cur_var
        };

        f_t.successors.push_back(s_dep);

        /* Update dependencies count, since added a new one */
        ++ndeps;

        /* Is this a new variable? */
        if (cur_var == nvar) {
            ++nvar;
        }
    }

    /* Finally add task to task graph */
    tasks.push_back(f_t);
    ++ntasks;
}

/* ************************
 * Main functions regarding tasklab API
 * ************************ */
void TaskLab::generate(const uint32_t n, const uint32_t m,
                       const uint32_t d = DEFAULT_DEP_RANGE,
                       const uint32_t t = DEFAULT_EXECUTION_SIZE, 
                       const float    r = DEFAULT_EXECUTION_RANGE) {
    /* If there was something there, get rid of it! */
    if (tg != NULL) {
        delete tg;
    }

    tg = new TaskGraph(n, d, t, r);

    /* Seed random generator */
    srand(time(NULL));

    /* Create tasks */
    tg->create_tasks(m);
}

bool TaskLab::run(const uint8_t rt) {
    /* Check if there is a high task graph available to be dispatched */
    if (empty(HTASK)) {
        fprintf(stderr, "[ERROR] There isn't any graph to be dispatched!\n");

        return false;
    }

    tg_t = tg;       // set temp. task graph
    r_error = false; // for now, it wans't found any error

    /* Initialize runtime functions based on the runtime */
    if (!init_run(rt)) {
        /* Uh oh! Something went wrong! */
        return false;
    }

    if (rt == RT::MTSP) {
        // Start execution of a parallel region
        fork_call(NULL, 0, (kmpc_micro) microtask);
    }

    tg_t = NULL;    // clean up your mess!

    /* Was the execution successful? */
    if (r_error) {
        /* Error found, set r_error to its default value */
        r_error = false;

        printf("[ERROR] The graph did not executed correctly!\n");

        return false;
    } else {
        /* Everything went fine! */
        return true;
    }
}

/* ***************
 * Helper functions regarding simulation
 * *************** */
void TaskLab::burnin(const uint32_t nruns, const uint32_t max_t, const uint8_t rt) {
    uint32_t n, m, d, i;
    bool     r = false;
    char     gr_n[100]; // graph name

    uint32_t e = 0;     // keep track of the errors;

    /* Generate nruns graphs */
    for (i = 0; i < nruns; i++) {
        /* Seed random generator */
        srand(time(NULL) + i);

        n = rand() % max_t + 1;
        m = rand() % n/2 + 1;
        d = rand() % n + 1;

        fprintf(stdout, "%u) Generating task graph of %d tasks...\n", i, n);

        generate(n, m, d);

        fprintf(stdout, "\tDone generation!\n");

        r = run(rt);

        /* If there was en error */
        if (!r) {
            sprintf(gr_n, "%s_failed_%04d", DEFAULT_NAME, e++);

            /* Save graph as both formats */
            save(add_extension(DEFAULT_NAME, gr_n));
            plot(add_extension(DEFAULT_NAME, gr_n), DOT);
            plot(add_extension(DEFAULT_NAME, gr_n), LL);
            plot(add_extension(DEFAULT_NAME, gr_n), INFO);

            fprintf(stderr, "Execution failed!\n\tFile saved and plotted as \
'%s'.\n\n", gr_n);
        }
    }
}

void TaskLab::burnin(const char* path, const uint16_t n, const uint8_t rt) {
    /* Is the path correct? */
    if (!fs::exists(path) || !fs::is_directory(path)) {
        fprintf(stderr, "[ERROR] Directory \"%s\" does not exist.\n", path);

        return;
    }

    // save graph into internal representation
    const char* filename_ = "burnin_feedback.txt";
    std::ofstream ofs (filename_, std::ofstream::out);

    bool r = false;
    fs::recursive_directory_iterator it(path);
    fs::recursive_directory_iterator endit;

    while(it != endit)
    {
        /* Check if it is a valid file */
        if (fs::is_regular_file(*it) && it->path().extension() == ".dat") {
            const char* cur_f = add_extension("/", it->path().stem().c_str());
            const char* cur_p = add_extension(it->path().parent_path().c_str(), cur_f);

            /* Restore it */
            restore(cur_p);

            ofs << "Execution of " << cur_p << "\n";

            for (int i = 0; i < n; ++i) {
                r = run(rt);

                if (!r) {
                    ofs << "\t" << i + 1 << ": failed.\n";
                } else {
                    ofs << "\t" << i + 1 << ": success! \n";
                }
            }

            ofs << "\n";
        }

        ++it;
    }

    fprintf(stdout, "Success! Output is at %s\n", filename_);

    ofs.close();
}

/* ***************
 * Trace functions
 * *************** */
bool TaskLab::hasEvent(const uint8_t event) {
    if (event < EVENT_S) {
        /* Return if event is set */
        return t_e[event];
    } else {
        /* Event not found */
        return false;
    }
}

void TaskLab::watchEvent(const uint8_t event) {
    if (event < EVENT_S) {
        /* Set it to be watched */
        t_e[event] = true;
    } else {
        fprintf(stderr, "[ERROR] Event is not supported.\n");
    }
}

void TaskLab::eventOccurred(const uint8_t event, const void* t_p) {
    switch (event) {
        case Evt::HTASK:
        {
            task t = *(task*) t_p;

            /* If there isn't a task graph instantiated yet */
            if (empty()) {
                tg = new TaskGraph();
            }

            tg->add_task(t);

            break;
        }

        case Evt::LTASK:
        {
            uint64_t t = *(uint64_t*) t_p;

            /* If there isn't a task graph instantiated yet */
            if (empty()) {
                tg = new TaskGraph();
            }

            tg->ll.push_back(t);

            break;
        }

        default:
            fprintf(stderr, "[ERROR] Event is not supported.\n");
    }
}

/* ************************
 * Graph management
 * ************************ */
bool TaskLab::save(const char* filename) {
    /* Check if there is a task graph available */
    if (empty()) {
        fprintf(stderr, "[ERROR] There isn't any graph to be saved!\n");

        return false;
    }

    // save graph into internal representation
    const char* filename_ = add_extension(filename, ".dat");

    // make an archive
    std::ofstream ofs(filename_);

    if (ofs.is_open()) {
        boost::archive::text_oarchive oa(ofs);

        oa << tg;
    } else {
        fprintf(stderr, "[ERROR] Invalid filename. Couldn't save task graph.\n");

        delete filename_;
        return false;
    }

    delete filename_;

    return true;
}

bool TaskLab::restore(const char* filename) {
    // restore by default internal representation
    const char* filename_ = add_extension(filename, ".dat");

    // if task graph is not empty, clean it up!
    if (!empty()) {
        delete tg;
        tg = NULL;
    }

    // allocate new memory
    tg = new TaskGraph();

    // open the archive
    std::ifstream ifs(filename_);

    if (ifs.is_open()) {
        boost::archive::text_iarchive ia(ifs);

        // restore the schedule from the archive
        tg->tasks.clear();
        ia >> tg;
    } else {
        fprintf(stderr, "[ERROR] Invalid filename. Couldn't restore task graph.\n");

        /* Throws away */
        delete filename_;
        delete tg;
        tg = NULL;

        return false;
    }

    delete filename_;

    return true;
}

bool TaskLab::plot(const char* filename, const uint8_t fm) {
    if (fm == Plot::DOT) {
        /* Check if there is a task graph available */
        if (empty(HTASK)) {
            fprintf(stderr, "[ERROR] There isn't any high level graph to be plotted!\n");

            return false;
        }

        /* Get correct number of chunks to create the dot file for the 
         * task graph */
        uint32_t chunks = ceil((float)tg->ntasks/MAX_DOT_P);

        for (uint32_t j = 0; j < chunks; j++) {
            /* Set start and end of current chunk */
            uint32_t start = j * MAX_DOT_P;
            uint32_t end   = start + MAX_DOT_P > tg->ntasks ? tg->ntasks : 
                                                              start + MAX_DOT_P;

            /* Plot, by default, a dot file */                
            char filename_[256];
            sprintf(filename_, "%s_%04d.dot", filename, j);

            std::ofstream ofs (filename_, std::ofstream::out);

            ofs << "digraph taskgraph {\n";

            /* Print every task at the chunk into the file */
            for (uint32_t i = start; i < end; i++) {
                ofs << "\tT" << i << " [label= \"T" << i << "\\n load: " 
                    << tg->tasks[i].exec << "\"];\n";

                // if there are predecessors on the following task
                if (!tg->tasks[i].predecessors.empty()) {
                    std::list<_dep>::const_iterator it;

                    for (it = tg->tasks[i].predecessors.begin(); 
                        it != tg->tasks[i].predecessors.end(); ++it) {
                        ofs << "\tT" << it->task << " -> T" << i << "[label=" 
                            << it->dID << "];\n";
                    }
                }
            }

            ofs << "}";

            ofs.close();
        }

    } else if (fm == LL) {
        if (empty(LTASK)) {
            fprintf(stderr, "[ERROR] There isn't any low level information to be plotted!\n");

            return false;
        }

        // plot, by default, a low level task graph .tsk
        const char* filename_ = add_extension(filename, ".tsk");

        std::ofstream ofs (filename_, std::ofstream::out);

        std::vector<uint64_t>::iterator it;
        for (it = tg->ll.begin(); it != tg->ll.end(); ++it) {
            ofs << std::hex << *it << "\n";
        }

        ofs.close();

        delete filename_;
    } else if (fm == INFO) {
        /* Check if there is a task graph available */
        if (empty(HTASK)) {
            fprintf(stderr, "[ERROR] There isn't any high level information to be displayed!\n");

            return false;
        }

        // plot, by dfault, an info file .info
        const char* filename_ = add_extension(filename, ".info");

        std::ofstream ofs (filename_, std::ofstream::out);

        // found information regarding execution time and count of dependencies
        float    max_r    = 0,
                 min_r    = 1;
        uint32_t dep_c[4] = {0}; 

        // what is the min. and max. execution time from all tasks?
        // types of dependencies
        std::vector<_task>::iterator it;
        for (it = tg->tasks.begin(); it != tg->tasks.end(); ++it) {
            if (it->exec > max_r) {
                max_r = it->exec;
            }

            if (it->exec < min_r) {
                min_r = it->exec;
            }

            // if there are successors on the following task...
            //   check dependencies!
            if (!it->successors.empty()) {
                std::list<_dep>::const_iterator itt;

                for (itt = it->successors.begin(); 
                    itt != it->successors.end(); ++itt) {
                    ++dep_c[itt->type];
                }
            }
        }

        // display information
        ofs << "--- Task graph general information                    ---\n";
        ofs << "\tTotal no. of tasks:                     " << tg->ntasks << "\n";
        ofs << "\tTotal no. of variables:                 " << tg->nvar << "\n";
        ofs << "\tTotal no. of unique dependencies:       " << tg->ndeps << "\n";


        ofs << "\t\tin:                                 " << dep_c[Type::IN] << "\n";
        ofs << "\t\tinout:                              " << dep_c[Type::INOUT] << "\n";
        ofs << "\t\tout:                                " << dep_c[Type::OUT] << "\n";

        ofs << "\n--- Information regarding randomly generated graphs ---\n";
        ofs << "\tStandard amount of iterations per task: " << tg->exec_t << "\n";

        ofs << "\tMinimum amount of iterations is:        " << std::fixed << 
               std::setprecision(0) << (tg->exec_t * min_r) + tg->exec_t << "\n";
        ofs << "\tMaximum amount of iterations is:        " << std::fixed << 
               std::setprecision(0) << (tg->exec_t * max_r) + tg->exec_t << "\n";

        ofs.close();

        delete filename_;
    }

    return true;
}

/* ***************
 * Helper functions regarding verification
 * *************** */
bool TaskLab::empty(uint8_t evt) {
    if (tg == NULL)
        return true;

    switch (evt) {
        case HTASK:
            return tg->tasks.empty();

        case LTASK:
            return tg->ll.empty();

        default:
            return false;
    }
}

/* ************************
 * Default constructor&destructor
 * ************************ */
TaskLab::TaskLab() {
    tg = NULL;
}

TaskLab::~TaskLab() {
    if (tg != NULL) {
        delete tg;
    }
}

/* ************************
 * Dispatcher handlers
 * ************************ */
bool TaskLab::init_run(const uint8_t rt) {
    /* Get function pointer from according runtime */
    if (rt == RT::MTSP) {
        fork_call          = (fc_t)dlsym(RTLD_NEXT, "__kmpc_fork_call");
        omp_task_alloc     = (ta_t)dlsym(RTLD_NEXT, "__kmpc_omp_task_alloc");
        omp_task_with_deps = (td_t)dlsym(RTLD_NEXT, "__kmpc_omp_task_with_deps");
        omp_taskwait       = (tw_t)dlsym(RTLD_NEXT, "__kmpc_omp_taskwait");
#ifdef TIOGA
        pretty_dump        = (tp_t)dlsym(RTLD_NEXT, "pretty_dump");
#endif
    }

    /* Was everything ok? */
    if (omp_task_alloc == NULL || omp_task_with_deps == NULL
        || omp_taskwait == NULL) {
        /* Failed */
        fprintf(stderr, "Please, set LD_PRELOAD accordingly to your runtime.\n");

        return false;
    } else {
#ifdef TIOGA
        if (pretty_dump == NULL) {
            /* Failed */
            fprintf(stderr, "Please, set LD_PRELOAD accordingly to your runtime.\n");

            return false;
        }
#endif
        /* Good to go! */
        return true;
    }
}


void TaskLab::microtask(int gid, int tid, void* param) {
    /* Variables that keep in track with the digraph 
     * workflow when dispatching */
    bool*      dep_chk; /* Dependency validation (task graph) */

    bool*      varptr;  /* Pointer for variables addresses */
    tparam_t*  params;  /* Our own param. manager, will be 
                         * initialized later on */

    dep_chk = new bool[tg_t->ndeps];
    varptr  = new bool[tg_t->nvar];
    params  = new tparam_t[tg_t->ntasks]; 

    memset(dep_chk, false, tg_t->ndeps * sizeof(bool));
    memset(varptr, false, tg_t->nvar * sizeof(bool));

    #ifdef DEBUG
    printf("Number of dependencies:\t %d\n", tg_t->ndeps);
    printf("Number of variables:\t %d\n", tg_t->nvar);
    printf("Number of tasks:\t %d\n", tg_t->ntasks);
    #endif

    std::cout << "Start Dispatching tasks!\n";

    // Since a task only depends on the previous tasks (in the vector index), a
    // valid approach is to dispatch the tasks in the vector order
    std::vector<_task>::iterator it;
    for (it = tg_t->tasks.begin(); it != tg_t->tasks.end(); ++it) {
        kmp_depend_info*          dep_list;
        kmp_task*                 task;
        std::list<_dep>::iterator itt;

        uint cur_pred, cur_succ;     // Counter for predecessors and successors
        uint n_dep;                  // Total number for dependencies

        uint32_t cur_task = it->tID; // Task id

        /* Initialize task structure */
        task = (kmp_task*)omp_task_alloc(NULL, 0, 0, sizeof(kmp_task) + 8, 0,
                                         (kmp_routine_entry)ptask_f);

        // -- Set configurations regarding task

        /* Compute number of dependencies for current task */
        params[cur_task].pred_s = it->predecessors.size();
        params[cur_task].succ_s = it->successors.size();

        #ifdef DEBUG
        printf("-- Task no.%d has %d predecessors and %d successors --\n\n", it->tID,
               params[cur_task].pred_s, params[cur_task].succ_s);
        #endif

        // -- Set our own data regarding task graph verification
        params[cur_task].tID  = cur_task;
        params[cur_task].exec = it->exec;

        params[cur_task].pred = new bool*[params[cur_task].pred_s];
        params[cur_task].succ = new bool*[params[cur_task].succ_s];

        /* Pointer to our params indexes */
        cur_pred = cur_succ = 0;

        /* Total of dependecies relying on different variables from current task */
        n_dep = params[cur_task].succ_s;

        /* Initialize dep_list to dispatch it to runtime */
        dep_list = new kmp_depend_info[n_dep + 1];

        /* Set first position which will be used as pointer ref. */
        dep_list[0].base_addr = (kmp_intptr) &(params[cur_task]);
        dep_list[0].len       = sizeof(params[cur_task]);
        dep_list[0].flags.in  = true;       // technically, this variable
                                            // is read at the function
        dep_list[0].flags.out = false;

        /* Pointer to our dep_list indexes */
        uint32_t i = 1;

        // -- Describe dependencies that will be dispatched
        for (itt = it->successors.begin(); itt != it->successors.end(); ++itt) {
            // -- Describe dep_list
            /* Address to rely on */
            dep_list[i].base_addr = (kmp_intptr) &varptr[itt->var];
            dep_list[i].len = sizeof(varptr[itt->var]);

            /* Dependency type */
            dep_list[i].flags.in = (itt->type != Type::OUT) ? true : false;
            dep_list[i].flags.out = (itt->type != Type::IN) ? true : false;

            ++i;

            // -- Dependency validation pointer
            params[cur_task].succ[cur_succ] = &dep_chk[itt->dID];
            ++cur_succ;
        }

        // -- Set our own verifier
        for (itt = it->predecessors.begin(); itt != it->predecessors.end(); ++itt) {
            // -- Dependency validation pointer
            params[cur_task].pred[cur_pred] = &dep_chk[itt->dID];
            ++cur_pred;
        }

        /* Finally, dispatch task! */
        std::cout << "\tdispatching task " << cur_task << "\n";
        omp_task_with_deps(NULL, 0, task, n_dep + 1, dep_list, 0, NULL);

        /* Clean up the mess */
        delete dep_list;
    }

    std::cout << "\tDone Dispatching!\n";

    // Wait until all tasks have been executed
    omp_taskwait(nullptr, 0);

    std::cout << "\tDone executing!\n";

    /* Free memory */
    for (it = tg_t->tasks.begin(); it != tg_t->tasks.end(); ++it) {
        uint32_t cur_task = it->tID; // task id

        delete params[cur_task].pred;
        delete params[cur_task].succ;
    }

    delete dep_chk;
    delete varptr;
    delete params;
}

void TaskLab::f(tparam_t param) {
    bool cur = true;

    // Load time to be executed on the current task
    int load = (param.exec * tg_t->exec_t) + tg_t->exec_t;

    int i, foo;

    // Waiting!
    // std::this_thread::sleep_for(std::chrono::milliseconds(load));

    // do some arbitrary work for amount of load
    for (i = 0, foo = 0; i < load; i++) {
        foo++;
    }

#ifdef DEBUG
    printf("Executing task no. %d.\n", param.tID);
#endif

    // -- Compute if task execution is valid --

    // Check if all input dependencies are true (if in_s is 0, then cur will
    // be true as expected)
    for (uint i = 0; cur && i < param.pred_s; i++) {
        cur = cur && *(param.pred[i]);

#ifdef DEBUG
        if (!cur) {
            printf("Error at dependency no. %d.\n", i);
        }
#endif
    }

    // Print error message if execution is incorrect
    if (!cur) {
        std::string err_str;

        err_str = "invalid execution of task " + std::to_string(param.tID) + "\n";

        /* Error found */
        r_error = true;

        std::cerr << err_str;
    }

    // Propagate whether current task execution is valid
    for (uint i = 0; i < param.succ_s; i++) {
        *(param.succ[i]) = cur;
    }
}

void TaskLab::ptask_f(kmp_int32 gtid, void* param) {
    kmp_task* t = (kmp_task*) param;
    mtsp_task_metadata* md = t->metadata;

    /* --- get param! --- */
    tparam_t* p = (tparam_t*) md->dep_list[0].base_addr;

#ifdef DEBUG
	printf("Executed with exec time no. %lf!\n", p->exec);

#ifdef TIOGA
    pretty_dump();
#endif
#endif

    f(*p);
}

/* Initialize static helper variables regarding dispatching */
TaskGraph* TaskLab::tg_t = NULL;
bool       TaskLab::r_error    = false;

/* ************************
 * Helpers
 * ************************ */
const char* TaskLab::add_extension(const char* filename, const char* extension) {
    // save graph into internal representation
    char* filename_ = new char[strlen(filename) + strlen(extension) + 1];

    // add extension
    strcpy(filename_, filename);
    strcat(filename_, extension);

    return filename_;
}


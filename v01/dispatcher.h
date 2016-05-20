/**
 * Dispatcher.h
 * Description: API responsible for dispatching a graph to the MTSP runtime
 *  - Execute the graph structure by dispatching it to the runtime
 *  - Displays an error message if the graph was incorrectly dispatched
 * */

#include "common.h"

#include <kmp.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>

/**
 * Task parameter data structure
 */
typedef struct task_parameter {
    uint id;
    bool** in_var;
    bool** out_var;
    uint in_sz;
    uint out_sz;
    float load;
} tparam_t;

/* Dispatcher API */
class Dispatcher {
public:
    /**
     * Dispatch a graph to the runtime
     * */
    static void dispatch(Graph* g) {
        graph = g;

        // Allocate array of variables to be used as dependencies
        dep_vars = new bool[graph->total_deps];
        params = new tparam_t[graph->total_tasks];

        // Start execution of a parallel region
        __kmpc_fork_call(NULL, 0, (kmpc_micro) microtask);

        // Clean the mess
        delete dep_vars;
        delete params;
    }

private:
    /* Static variables that keep in track with the graph workflow */
    static Graph* graph;
    static bool* dep_vars;  
    static tparam_t* params;

    /**
     * Main dispatcher according to runtime signature, manage dependencies
     * between tasks, set dependency checker and dispatch them.
     */
    static void microtask(int gid, int tid, void* param) {
        uint cur_task = 0;

        std::cout << "Start Dispatching tasks!\n";

        // Initialize dep_vars with false
        for (uint i = 0; i < graph->total_deps; i++)
            dep_vars[i] = false;

        // Since a task only depends on the previous tasks (in the vector index), a
        // valid order is to dispatch the tasks in the vector order
        std::vector<Task>::iterator it;
        for (it = graph->tasks.begin(); it != graph->tasks.end(); ++it) {
            kmp_depend_info* dep_list;
            kmp_task* task;
            std::list<Dep>::iterator itt;
            uint cur_out, cur_in;   // Counter for in and out dependencies
            uint n_dep;             // First dependency and number of dependencies

            // Initialize task struckture
            task = __kmpc_omp_task_alloc(NULL, 0, 0, sizeof(kmp_task) + 8, 0,
                                         (kmp_routine_entry)ptask_f);

            // Compute number of dependencies for current task
            params[cur_task].in_sz = 0;
            params[cur_task].out_sz = it->successors.size();
            n_dep = params[cur_task].out_sz;

            for (itt = it->predecessors.begin(); itt != it->predecessors.end(); 
                 ++itt) {
                params[cur_task].in_sz += 1;
                if (itt->type == INOUT)
                    params[cur_task].out_sz += 1;
            }

            n_dep += params[cur_task].in_sz;

            params[cur_task].id = cur_task;
            params[cur_task].load = it->load;
            params[cur_task].in_var = new bool*[params[cur_task].in_sz];
            params[cur_task].out_var = new bool*[params[cur_task].out_sz];
            dep_list = new kmp_depend_info[n_dep+1];

            // Prepare task dependencies
            cur_in = cur_out = 0;
            uint i = 1; // Index for dep_list
            dep_list[0].base_addr = (kmp_intptr) &(params[cur_task]);
            dep_list[0].len = sizeof(params[cur_task]);
            dep_list[0].flags.in = false;
            dep_list[0].flags.out = false;

            for (itt = it->successors.begin(); itt != it->successors.end(); ++itt) {

                params[cur_task].out_var[cur_out] = &dep_vars[itt->index];

                dep_list[i].base_addr = (kmp_intptr) &dep_vars[itt->index];
                dep_list[i].len = sizeof(dep_vars[itt->index]);
                dep_list[i].flags.in = false;
                dep_list[i].flags.out = true;

                i++;
                cur_out++;
            }

            for (itt = it->predecessors.begin(); itt != it->predecessors.end(); ++itt) {
                params[cur_task].in_var[cur_in] = &dep_vars[itt->index];

                dep_list[i].base_addr = (kmp_intptr) &dep_vars[itt->index];
                dep_list[i].len = sizeof(dep_vars[itt->index]);
                dep_list[i].flags.in = true;
                dep_list[i].flags.out = false;

                if (itt->type == INOUT) {
                    params[cur_task].out_var[cur_out] = &dep_vars[itt->index];
                    dep_list[i].flags.out = true;

                    cur_out++;
                } else {
                    dep_list[i].flags.out = false;
                }

                i++;
                cur_in++;
            }

            // Dispatch task
            std::cout << "\tdispatching task " << cur_task << "\n";
            __kmpc_omp_task_with_deps(NULL, 0, task, n_dep + 1, dep_list, 0, NULL);


            delete dep_list;
            cur_task++;
        }

        std::cout << "Done Dispatching!\n";

        // Wait until all tasks have been executed
        __kmpc_omp_taskwait(NULL, 0);

        std::cout << "Done executing!\n";
    }

    /**
     * Function to be executed by each task
     * */
    static void f(tparam_t param) {
        bool cur = true;

        // Load time to be executed on the current task
        int load = (param.load * graph->load_time) + graph->load_time;

        // Waiting!
        std::this_thread::sleep_for(std::chrono::milliseconds(load));

        // -- Compute if task execution is valid --

        // Check if all input dependencies are true (if in_sz is 0, then cur will
        // be true as expected)
        for (uint i = 0; cur && i < param.in_sz; i++)
            cur = cur && *(param.in_var[i]);

        // Print error message if execution is incorrect
        if (!cur) {
            std::string err_str;

            err_str = "invalid execution of task " + std::to_string(param.id) + "\n";
            std::cerr << err_str;
        }

        // Propagate whether current task execution is valid
        for (uint i = 0; i < param.out_sz; i++)
            *(param.out_var[i]) = cur;
    }

    /**
     * Default function called by each task when executed
     * */
    static void ptask_f(kmp_int32 gtid, void* param) {
        kmp_task* t = (kmp_task*) param;
        mtsp_task_metadata* md = t->metadata;

        tparam_t* p = (tparam_t*) md->dep_list[0].base_addr;

        f(*p);
    }
};

/* Declare static variables */
Graph*    Dispatcher::graph;
bool*     Dispatcher::dep_vars;
tparam_t* Dispatcher::params;

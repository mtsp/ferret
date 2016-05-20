/**
 * Gen-dag.h
 * Description: API responsible for generating a random task graph
 *  - Receive user parameters to generate the graph
 *  - Create tasks, choose a number of dependencies and describe each of them
 *    by choosing its predecessor and the dependency type (IN/INOUT)
 *  - Also set an OUT dependency for each parent node
 * */

#include "common.h"

// standard value for how far a predecessor may be
#define DEFAULT_DEP_RANGE  10

// standard load time (in milisseconds)
#define DEFAULT_LOAD_TIME  1000

// max. range from standard load time (0 to 1)
#define DEFAULT_LOAD_RANGE 0.25

// default name for the graph
#define DEFAULT_NAME       (char*)"graph"

/* Gen_dag API */
class Gen_dag {
public:
    /* 
     * Generates a directed acyclic graph to be taken as a task graph 
     *  graph is the graph pointer (must be deleted)
     *  n     is the number of tasks to be generated;
     *  m     is the maximum number of IN/INOUT dependencies that
     *        has to be created on each task;
     *  d     is how far a predecessor may be from a parent;
     *  t     is the standard load time per task (ms);
     *  r     is the max. range from standard load time (0-1).
     * */
    static void generate(Graph** graph, uint n, uint m,
                         uint d = DEFAULT_DEP_RANGE,
                         uint t = DEFAULT_LOAD_TIME, 
                         float r = DEFAULT_LOAD_RANGE) {
        *graph = new Graph(n, d, t, r);
        
        /* Seed random generator */
        srand(time(NULL));

        Gen_dag::create_tasks(*graph, m);
    }

private:
    /* 
     * Generate tasks
     *  max_dep: max. number of dependencies per task
     * */
    static void create_tasks(Graph* graph, uint max_dep) {
        /* Keep in track with the id of each dependency */
        uint dep_id = 0;

        /* Initialize main task */
        graph->tasks[0].C_dep_tasks = 0;

        // Choose signal and then set load time of task
        graph->tasks[0].load = (rand() % 2) == 0 ? 1 : -1;
        graph->tasks[0].load *= (float)(rand() % 100)/100 * graph->max_range;

        /* Iterate over tasks */
        for (uint i = 1; i < graph->total_tasks; i++) {
            uint range_min, range_max;

            // Get the max numbers of predecessors as possible
            uint cur_dep = i <= max_dep ? i - 1 : max_dep;

            // Define minimum index of dependency
            if (i < graph->dep_range)
                range_min = 0;
            else
                range_min = i - graph->dep_range;

            // Define maximum index of dependency
            if (range_min + graph->dep_range >= i)
                range_max = i;
            else
                range_max = range_min + graph->dep_range;

            // Choose signal and then set load time of task
            graph->tasks[i].load = (rand() % 2) == 0 ? 1 : -1;
            graph->tasks[i].load *= (float)(rand() % 100)/100 * graph->max_range;

            // Generate a number in the range 1 to cur_dep
            graph->tasks[i].C_dep_tasks = cur_dep == 0 ? 1 : rand() % cur_dep + 1;

            // Allocate list
            graph->tasks[i].predecessors.resize(graph->tasks[i].C_dep_tasks);

            // Describe dependencies of the current task
            Gen_dag::describe_deps(graph, i, &dep_id, range_min, range_max);

            // Increment total dependencies
            graph->total_deps += graph->tasks[i].C_dep_tasks;
        }
    }

    /**
     * Describe dependency between tasks of the graph 
     *  graph:   current task graph
     *  task_id: id of the current task
     *  dep_id:  array of id of dependencies
     *  min:     min. predecessor id of the graph
     *  max:     max. predecessor id of the graph
     * */
    static void describe_deps(Graph* graph, uint task_id, 
                              uint* dep_id, uint min, uint max) {
        // Keep track of results
        std::vector<uint> results;

        // For each predecessor, define a task and the dependency type
        std::list<Dep>::iterator it;
        for (it = graph->tasks[task_id].predecessors.begin(); 
            it != graph->tasks[task_id].predecessors.end(); ++it) {
            // Define a task that hasn't been picket yet
            do {
                it->task = min + (rand() % (max - min));
            } while (std::find(results.begin(), results.end(), it->task) 
                != results.end());

            // Set as already used
            results.push_back(it->task);

            // Set a random type for the dependency, either IN or INOUT
            it->type = Type(rand() % 2);
            it->index = *dep_id;

            Dep b_dep; // "backwards" dependency
            b_dep.task = task_id;
            b_dep.type = OUT;
            b_dep.index = *dep_id;

            // Set itself as a successor at predecessor task
            graph->tasks[it->task].successors.push_back(b_dep);

            (*dep_id)++;
        }
    }
};
/**
 * Tasklab.cpp
 *   API implementation
 *
 * */

 #include "tasklab.h"

/* ************************
 * Task graph builders
 * ************************ */
TaskGraph::TaskGraph() {
    // just set everything to zero, since it is arbitrary
    TaskGraph(0, 0, 0, 0);
}

TaskGraph::TaskGraph(const uint32_t n, const uint32_t d,
                     const uint32_t t, float r)
       : ntasks(n), dep_r(d), exec_t(t), max_r(r) {
    tasks.resize(n);

    ndeps = 0;       // initialize num of deps as none
}

void TaskGraph::create_tasks(const uint32_t max_dep) {
    /* Keep in track with the id of each dependency */
    uint32_t dep_id = 0;

    /* Initialize main task */
    tasks[0].npred = 0;

    // Choose signal and then set load time of task
    tasks[0].exec = (rand() % 2) == 0 ? 1 : -1;
    tasks[0].exec *= (float)(rand() % 100)/100 * max_r;

    /* Iterate over tasks */
    for (uint32_t i = 1; i < ntasks; i++) {
        uint32_t range_min, range_max;

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

        // Choose signal and then set load time of task
        tasks[i].exec = (rand() % 2) == 0 ? 1 : -1;
        tasks[i].exec *= (float)(rand() % 100)/100 * max_r;

        // Generate a number in the range 1 to cur_dep
        tasks[i].npred = cur_dep == 0 ? 1 : rand() % cur_dep + 1;

        // Allocate list
        tasks[i].predecessors.resize(tasks[i].npred);

        // Describe dependencies of the current task
        describe_deps(i, &dep_id, range_min, range_max);

        // Increment total dependencies
        ndeps += tasks[i].npred;
    }
}

void TaskGraph::describe_deps(const uint32_t tID, uint32_t* dep_id, 
                              const uint32_t min, const uint32_t max) {
    // Keep track of results
    std::vector<uint> results;

    // For each predecessor, define a task and the dependency type
    std::list<dep>::iterator it;
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
        it->dID = *dep_id;

        dep b_dep; // "backwards" dependency
        b_dep.task = tID;
        b_dep.type = Type::OUT;
        b_dep.dID = *dep_id;

        // Set itself as a successor at predecessor task
        tasks[it->task].successors.push_back(b_dep);

        (*dep_id)++;
    }
}

/* ************************
 * Main functions regarding tasklab API
 * ************************ */
void TaskLab::generate(const uint32_t n, const uint32_t m,
                       const uint32_t d, const uint32_t t, 
                       const float    r) {
    tg = new TaskGraph(n, d, t, r);

    /* Seed random generator */
    srand(time(NULL));

    tg->create_tasks(m);
}

void TaskLab::dispatch(const uint8_t rt) {
    tg_t = tg;

    if (rt == RT::MTSP) {
        // Start execution of a parallel region
        __kmpc_fork_call(NULL, 0, (kmpc_micro) microtask);
    }

    tg_t = NULL;
}

/* ************************
 * Dispatcher handlers
 * ************************ */
void TaskLab::microtask(int gid, int tid, void* param) {
    /* Variables that keep in track with the gidraph workflow when dispatching */
    bool*      dep_vars;  
    tparam_t*  params;

    // Allocate array of variables to be used as dependencies
    dep_vars = new bool[tg_t->ndeps];
    params = new tparam_t[tg_t->ntasks];

    uint cur_task = 0;

    std::cout << "Start Dispatching tasks!\n";

    // Initialize dep_vars with false
    for (uint i = 0; i < tg_t->ndeps; i++)
        dep_vars[i] = false;

    // Since a task only depends on the previous tasks (in the vector index), a
    // valid approach is to dispatch the tasks in the vector order
    std::vector<task>::iterator it;
    for (it = tg_t->tasks.begin(); it != tg_t->tasks.end(); ++it) {
        kmp_depend_info* dep_list;
        kmp_task* task;
        std::list<dep>::iterator itt;
        uint cur_out, cur_in;   // Counter for in and out dependencies
        uint n_dep;             // First dependency and number of dependencies

        // Initialize task struckture
        task = __kmpc_omp_task_alloc(NULL, 0, 0, sizeof(kmp_task) + 8, 0,
                                     (kmp_routine_entry)ptask_f);

        // Compute number of dependencies for current task
        params[cur_task].in_s = 0;
        params[cur_task].out_s = it->successors.size();
        n_dep = params[cur_task].out_s;

        for (itt = it->predecessors.begin(); itt != it->predecessors.end(); 
             ++itt) {
            params[cur_task].in_s += 1;
            if (itt->type == Type::INOUT)
                params[cur_task].out_s += 1;
        }

        n_dep += params[cur_task].in_s;

        params[cur_task].tID = cur_task;
        params[cur_task].exec = it->exec;
        params[cur_task].in_var = new bool*[params[cur_task].in_s];
        params[cur_task].out_var = new bool*[params[cur_task].out_s];
        dep_list = new kmp_depend_info[n_dep+1];

        // Prepare task dependencies
        cur_in = cur_out = 0;
        uint i = 1; // Index for dep_list
        dep_list[0].base_addr = (kmp_intptr) &(params[cur_task]);
        dep_list[0].len = sizeof(params[cur_task]);
        dep_list[0].flags.in = false;
        dep_list[0].flags.out = false;

        for (itt = it->successors.begin(); itt != it->successors.end(); ++itt) {

            params[cur_task].out_var[cur_out] = &dep_vars[itt->dID];

            dep_list[i].base_addr = (kmp_intptr) &dep_vars[itt->dID];
            dep_list[i].len = sizeof(dep_vars[itt->dID]);
            dep_list[i].flags.in = false;
            dep_list[i].flags.out = true;

            i++;
            cur_out++;
        }

        for (itt = it->predecessors.begin(); itt != it->predecessors.end(); ++itt) {
            params[cur_task].in_var[cur_in] = &dep_vars[itt->dID];

            dep_list[i].base_addr = (kmp_intptr) &dep_vars[itt->dID];
            dep_list[i].len = sizeof(dep_vars[itt->dID]);
            dep_list[i].flags.in = true;
            dep_list[i].flags.out = false;

            if (itt->type == Type::INOUT) {
                params[cur_task].out_var[cur_out] = &dep_vars[itt->dID];
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

    // Clean the mess
    delete dep_vars;
    delete params;
}

/* ************************
 * Graph management
 * ************************ */
void TaskLab::save(const char* filename) {
    // save graph into internal representation
    const char* filename_ = add_extension(filename, ".dat");

    // make an archive
    std::ofstream ofs(filename_);

    if (ofs.is_open()) {
        boost::archive::text_oarchive oa(ofs);

        oa << tg;
    } else {
        fprintf(stderr, "Invalid filename.\n");

        exit(1);
    }

    delete filename_;
}

void TaskLab::restore(const char* filename) {
    // restore by default internal representation
    const char* filename_ = add_extension(filename, ".dat");

    // allocate memory
    tg = new TaskGraph(0, DEFAULT_DEP_RANGE,
                          DEFAULT_EXECUTION_TIME, 
                          DEFAULT_EXECUTION_RANGE);
    // open the archive
    std::ifstream ifs(filename_);

    if (ifs.is_open()) {
        boost::archive::text_iarchive ia(ifs);

        // restore the schedule from the archive
        tg->tasks.clear();
        ia >> tg;
    } else {
        fprintf(stderr, "Invalid filename.\n");

        exit(1);
    }

    delete filename_;
}

void TaskLab::plot(const char* filename) {
    // restore by default dot file
    const char* filename_ = add_extension(filename, ".dot");

    std::ofstream ofs (filename_, std::ofstream::out);

    ofs << "digraph taskgraph {\n";

    for (uint32_t i = 0; i < tg->ntasks; i++) {
        ofs << "\tT" << i << " [label= \"T" << i << "\\n load: " 
            << tg->tasks[i].exec << "\"];\n";

        // if there are successors on the following task
        if (!tg->tasks[i].successors.empty()) {
            std::list<dep>::const_iterator it;

            for (it = tg->tasks[i].successors.begin(); 
                it != tg->tasks[i].successors.end(); ++it) {
                ofs << "\tT" << i << " -> T" << it->task << "[label=" 
                    << it->dID << "];\n";
            }
        }
    }

    ofs << "}";

    ofs.close();

    delete filename_;
}

/* ************************
 * Default constructor&destructor
 * ************************ */
TaskLab::TaskLab() {
    tg = NULL;
}

TaskLab::~TaskLab() {
    if (tg != NULL)
        delete tg;
}

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

/* ************************
 * Dispatcher handler
 * ************************ */
void TaskLab::f(tparam_t param) {
    bool cur = true;

    // Load time to be executed on the current task
    int load = (param.exec * tg_t->exec_t) + tg_t->exec_t;

    // Waiting!
    std::this_thread::sleep_for(std::chrono::milliseconds(load));

    // -- Compute if task execution is valid --

    // Check if all input dependencies are true (if in_s is 0, then cur will
    // be true as expected)
    for (uint i = 0; cur && i < param.in_s; i++)
        cur = cur && *(param.in_var[i]);

    // Print error message if execution is incorrect
    if (!cur) {
        std::string err_str;

        err_str = "invalid execution of task " + std::to_string(param.tID) + "\n";
        std::cerr << err_str;
    }

    // Propagate whether current task execution is valid
    for (uint i = 0; i < param.out_s; i++)
        *(param.out_var[i]) = cur;
}

void TaskLab::ptask_f(kmp_int32 gtid, void* param) {
    kmp_task* t = (kmp_task*) param;
    mtsp_task_metadata* md = t->metadata;

    tparam_t* p = (tparam_t*) md->dep_list[0].base_addr;

    f(*p);
}

TaskGraph* TaskLab::tg_t = NULL;

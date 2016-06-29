/**
 * Tasklab.h
 *   API that enables the simulation of task parallelism applications without 
 *   relying on multiple benchmarks.
 *     -- generate and dispatch directed acyclic graphs, also known as
 *     task graphs.
 *     -- trace applications as high or low level tasks
 *     -- (de)serialize task graphs
 *     -- visualize information regarding task graph as .dot or .info
 *
 * */

#pragma once
#ifndef TASKLAB
#define TASKLAB

#include <stdlib.h>
#include <vector>
#include <list>
#include <map>

#include <fstream>
#include <string.h>

/* Serialization */
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/serialization/vector.hpp> // vectors
#include <boost/serialization/list.hpp>   // list
#include <boost/serialization/map.hpp>    // map

/* Dispatcher */
#include <kmp.h>
#include <thread>

/* ***************
 * Default definitions
 * *************** */
#define DEFAULT_DEP_RANGE       (uint32_t)10       // standard value for how far a predecessor may be
#define DEFAULT_EXECUTION_SIZE  (uint32_t)1000000  // standard execution size (i.e. in a iteration)
#define DEFAULT_EXECUTION_RANGE (float)0.25        // max. range from standard load time (0 to 1)
#define DEFAULT_NAME            (char*)"taskgraph" // default name for the graph

#define MAX_DOT_P               100                // max. no. of tasks by a plotted dot file

#define NONE                    -1

#define EVENT_S                  3                 // total events+1: HTASK LTASK

/// Uncomment to enter DEBUG mode
// #define DEBUG                    1

/// Uncomment to compile TASKLAB to run with TIOGA
// #define TIOGA                    1

/* ***************
 * Trace definitions
 * *************** */
#define TMPDIR                  "/tmp/"
#define EVT_VAR                 "TL_EVT"

/* Watchable events */
typedef enum Event   { HTASK = 1, LTASK = 2 } Evt;

/* Plotting task graph options */
typedef enum Plot    { DOT = 1, LL = 2, INFO = 3 } Plot;

/* Runtime definition */
typedef enum Runtime { MTSP = 1 } RT;

/* Type of a dependency */
typedef enum Type    { IN = 1, OUT = 2, INOUT = 3 } Type;

/* ***************
 * Task graph PUBLIC structure
 *   -- users should these structures
 * *************** */

#ifndef __TIOGLIB_H__
/**
 * dep describes a dependency between tasks
 */
typedef struct dep_s {
public:
    uint64_t     varptr;     // address of the dependency variable
    uint8_t      mode;       // mode of the variable 
} dep;

/**
 * task describes a task
 */
typedef struct task_s {
public:
    uint16_t    tID;         // hardware internal task ID
    uint64_t    WDPtr;       // address pointing to the task function
    int         ndeps;       // number of dependencies of the task
    dep*        deparr;      // list of dependencies of the task
} task;
#endif

/* ***************
 * Task graph INTERNAL structure
 *   -- for tasklab own validation
 * *************** */
/**
 * _dep describes a dependency between tasks
 */
typedef struct dep_i {
public:
    uint32_t task;  // task that the dependency is heading towards to

    uint8_t  type;  // type of dependency
    uint32_t dID;   // index of dependency

    uint32_t var;   // var that relies on

private:
    /* Serialization */
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &f, const uint version) {
        f & task;
        f & type;
        f & dID;
        f & var;
    }
} _dep;

/**
 * _task describes a task
 */
typedef struct task_i {
public:
    std::list<_dep> predecessors;  // predecessors tasks
    std::list<_dep> successors;    // successors tasks

    uint32_t tID;                  // index of task

    uint32_t npred;                // total number of predecessors
    float    exec;                 // how long should the task remain executing

    /* ***************
     * Task structure handler
     * *************** */
    /**
     * Check if a given dep. is already present on the task
     * return:  true if dep. is present, otherwise false
     * */
    bool hasdep(uint32_t ID);

private:
    /* Serialization */
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &f, const uint version) {
        f & predecessors;
        f & successors;
        f & tID;
        f & npred;
        f & exec;
    }
} _task;

/**
 * TaskGraph describes a complete graph of tasks
 */
class TaskGraph {
public:
    /* ***************
     * Task graph builders
     * *************** */
    /**
     * Arbitrary constructor, which sets everything to default values
     */
    TaskGraph();

    /**
     * Default and complete constructor
     *  n: number of tasks
     *  d: dep. range
     *  l: execution load time
     *  r: max. range from execution load time
     */
    TaskGraph(const uint32_t n, const uint32_t d, const uint32_t t, float r);

    /**
     * Feed the graph with tasks
     *  max_dep: max. number of dependencies per task
     * */
    void create_tasks(const uint32_t max_dep);

    /**
     * Describe dependency between tasks of the graph 
     *  task_id: id of the current task
     *  dep_id:  array of id of dependencies
     *  min:     min. predecessor id of the graph
     *  max:     max. predecessor id of the graph
     * */
    void describe_deps(const uint32_t tID, uint32_t* dep_id, 
                       const uint32_t min, const uint32_t max);

    /* ***************
     * Trace handlers
     * *************** */
    /**
     * Add a given task to the graph, solving dependencies
     *  t: task to be added
     * */
    void add_task(task t);

private:
    /* ***************
     * Members of task graph basic structure
     * *************** */
    friend class TaskLab;

    std::vector<_task> tasks;  // tasks structure
    uint32_t  ntasks;          // total number of tasks
    uint32_t  ndeps;           // total number of dependencies between tasks
    uint32_t  nvar;            // total number of variables shared between tasks

    uint32_t  dep_r;           // max range of how far a predecessor may be

    uint32_t  exec_t;          // standard execution time per task (loop cycles)
    float     max_r;           // max. range from standard execution time (0 to 1)

    /* Main dependency map with all variable addresses:
     *   each of them maps to a vector (or a single structure) with values 
     *   sufficient to describe a parent-children relationship.
     * */
    std::map< uint64_t, std::vector<_dep> > in_map;
    std::map< uint64_t, _dep >              out_map;

    /* Keep in track with low level task graph */
    std::vector<uint64_t> ll;

    /* ***************
     * Serialization
     * *************** */
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &f, const uint version) {
        f & tasks;
        f & ntasks;
        f & ndeps;
        f & nvar;
        f & dep_r;
        f & exec_t;
        f & max_r;
        f & in_map;
        f & out_map;
        f & ll;
    }
};

/* ***************
 * TaskLab
 * *************** */
class TaskLab {
public:
    /* ***************
     * Main functions regarding tasklab API
     * *************** */
    /* 
     * Generates a directed acyclic graph to be taken as a task graph 
     *  n     is the number of tasks to be generated;
     *  m     is the maximum number of IN/INOUT dependencies that
     *        has to be created on each task (1 to max.);
     *  d     is how far a predecessor may be from a parent (OPTIONAL);
     *  t     is the standard execution time per task (ms) (OPTIONAL);
     *  r     is the max. range from standard load time (0-1) (OPTIONAL).
     * */
    void generate(const uint32_t n, const uint32_t m,
                  const uint32_t d, const uint32_t t,
                  const float    r);

    /**
     * Dispatch a graph to the runtime
     *  rt      is the runtime that will be used for dispatching
     *
     *  returns if execution was successful
     * */
    bool run(const uint8_t rt);

    /* ***************
     * Helper functions regarding simulation
     * *************** */
    /*
     * Generates multiple random task graphs and dispatch them to runtime
     * of choice.
     *  nruns   is the number of graphs to be generated
     *  max_t   is the max. no. of tasks that a graph may obtain
     *  rt      is the runtime that will be used for dispatching
     */
    void burnin(const uint32_t nruns, const uint32_t max_t, const uint8_t rt);

    /*
     * Restores multiple task graphs from .dag files in a given folder and
     * dispatch them to runtime of choice.
     *  path  is the (full) path to the directory
     *  n     is how many times should we run each task graph
     */
    void burnin(const char* path, uint16_t n, const uint8_t rt); 

    /* ***************
     * Trace functions
     * *************** */
    /* Check if a given event is being recorded by the TaskLab
     *  event   is the type of the event
     *
     *  returns if the event is being recorded
     * */ 
    bool hasEvent(const uint8_t event);

    /* Notifies that a given event is now watchable
     *  event   is the type of the event
     * */
    void watchEvent(const uint8_t event);

    /* Notifies TaskLab that the event occurred
     *   -- if event is of type task:
     *     establish information as dep and task structure
     *
     *  event   is the type of the event
     *  t       is the information regarding the event
     * */
    void eventOccurred(const uint8_t event, const void* t);

    /* ***************
     * Graph management
     * *************** */
    /**
     * Save a graph as a .dat file, by serializing it
     *  filename: name of the file
     *  return:   true if it succeeds, else false
     */
    bool save(const char* filename);

    /**
     * Restore a graph from a .dat file, by deserializing it
     *  filename: name of the serialized graph file
     *  return:   true if it succeeds, else false
     */
    bool restore(const char* filename);

    /**
     * Save a graph as a .dot, .tsk or .info file
     *  filename: name of the file
     *  info:     format to be printed (dot for high level,
     *            ll for low level or info for graph information)
     *  return:   true if it succeeds, else false
     */
    bool plot(const char* filename, const uint8_t info);

    /* ***************
     * Helper functions regarding verification
     * *************** */
    /**
     * Simply check if TaskLab is empty, i.e. has a task graph structure in
     * evt format.
     *
     *  return: true if is task graph is empty, otherwise false
     */
    bool empty(uint8_t evt = NONE);

    /* ***************
     * Default constructor&destructor
     * *************** */
    TaskLab();
    ~TaskLab();

private:
    TaskGraph*              tg;

    /* Watchable trace events */
    bool                    t_e[EVENT_S] {0};

    /* Structures useful when dealing with despatching and
     * errors */
    static TaskGraph*       tg_t;
    static bool             r_error;

    /* ***************
     * Dispatcher handlers
     * *************** */
    /**
     * Task parameter data structure regarding the despatching
     */
    typedef struct task_parameter {
    public:
        uint32_t tID;       // task dep_id
        bool**   pred;
        bool**   succ;
        uint32_t pred_s;    // size of predecessors dep.
        uint32_t succ_s;    // size of successors dep.
        float    exec;      // default load time of task
    } tparam_t;

    /**
     * Initialize functions that will in order to dispatch functions
     *  rt:         runtime to be establish communication
     *
     *  return:     if it initialization was successful 
     */
    bool init_run(const uint8_t rt);

    /**
     * Main dispatcher according to mtsp runtime signature, manage dependencies
     * between tasks, set dependency checker and dispatch them.
     */
    static void microtask(int gid, int tid, void* param);

    /**
     * Default function called by each task when executed
     * */
    static void ptask_f(kmp_int32 gtid, void* param);

    /**
     * Function to be executed by each task
     * */
    static void f(tparam_t param);

    /* ***************
     * Helpers
     * *************** */
    /**
     * Helper function to add a extension to a string
     *  filename:  original string
     *  extension: extension to be added to the string
     *
     *  return:    concatenation of string with extension
     */
    const char* add_extension(const char* filename, const char* extension);
};

#endif

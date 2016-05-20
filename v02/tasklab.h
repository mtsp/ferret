/**
 * Tasklab.h
 *   API that enables the simulation of task parallelism applications without 
 * relying on multiple benchmarks.
 *   Is able to generate and dispatch directed acyclic graphs, taken as
 * task graphs.
 *
 * */

#ifndef TASKLAB
#define TASKLAB 1
#pragma once

#include <stdlib.h>
#include <vector>
#include <list>

#include <fstream>
#include <string.h>

/* Serialization */
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/serialization/vector.hpp> // vectors
#include <boost/serialization/list.hpp>   // list

/* Dispatcher */
#include <kmp.h>
#include <thread>

/* Defaults */
#define DEFAULT_DEP_RANGE       10 // standard value for how far a predecessor may be
#define DEFAULT_EXECUTION_TIME  1000 // standard load time (in milisseconds)
#define DEFAULT_EXECUTION_RANGE 0.25 // max. range from standard load time (0 to 1)
#define DEFAULT_NAME            (char*)"taskgraph" // default name for the graph

/* Runtime definition */
typedef enum Runtime { MTSP } RT;

/* Type of a dependency */
typedef enum Type { IN, INOUT, OUT } Type;

/* ***************
 * Dispatcher handler
 * *************** */
/**
 * Task parameter data structure regarding the despatching
 */
typedef struct task_parameter {
public:
    uint32_t tID;       // task dep_id
    bool** in_var;
    bool** out_var;
    uint32_t in_s;      // size of in var.
    uint32_t out_s;     // size of out var.
    float exec;         // default load time of task
} tparam_t;

/* ***************
 * Task graph structure
 * *************** */
/**
 * Structure to describe a dependency between tasks
 */
typedef struct dep_s {
public:
    uint32_t task;  // task that the dependency is heading towards to

    uint8_t  type;  // type of dependency
    uint32_t dID;   // index of dependency

private:
    /* Serialization */
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &f, const uint version) {
        f & task;
        f & type;
        f & dID;
    }
} dep;

/**
 * Structure to describe a task
 */
typedef struct task_s {
public:
    std::list<dep> predecessors;  // predecessors tasks
    std::list<dep> successors;    // by default, all successor tasks are OUT

    uint32_t tID;                 // index of task

    uint32_t npred;               // total number of predecessors
    float    exec;                // how long should the task remain executing

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
} task;

/**
 * Structure to describe a complete graph of tasks
 */
class TaskGraph {
private:
    friend class TaskLab;

    std::vector<task> tasks;        // tasks structure
    uint32_t  ntasks;               // total number of tasks
    uint32_t  ndeps{0};             // total number of dependencies between tasks

    uint32_t  dep_r;                // max range of how far a predecessor may be

    uint32_t  exec_t;          // standard execution time per task (ms)
    float     max_r;                // max. range from standard execution time (0 to 1)

    /* ***************
     * Task graph builders
     * *************** */
    /**
     * Arbitrary constructor, which sets everything to zero (none)
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

    /* Serialization */
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &f, const uint version) {
        f & tasks;
        f & ntasks;
        f & ndeps;
        f & dep_r;
        f & exec_t;
        f & max_r;
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
     *  d     is how far a predecessor may be from a parent;
     *  t     is the standard execution time per task (ms);
     *  r     is the max. range from standard load time (0-1).
     * */
    void generate(const uint32_t n, const uint32_t m,
                  const uint32_t d = DEFAULT_DEP_RANGE,
                  const uint32_t t = DEFAULT_EXECUTION_TIME, 
                  const float    r = DEFAULT_EXECUTION_RANGE);

    /**
     * Dispatch a graph to the mtsp runtime
     * */
    void dispatch(const uint8_t rt);

    /* ***************
     * Graph management
     * *************** */
    /**
     * Save a graph as a .dat file, by serializing it
     *  filename: name of the file
     */
    void save(const char* filename);

    /**
     * Restore a graph from a .dat file, by deserializing it
     *  filename: name of the serialized graph file
     */
    void restore(const char* filename);

    /**
     * Save a graph as a .dot file
     *  filename: name of the file
     */
    void plot(const char* filename);

    /* ***************
     * Default constructor&destructor
     * *************** */
    TaskLab();
    ~TaskLab();

private:
    TaskGraph* tg;

    /* Task graph structure using when dealing with despatching */
    static TaskGraph* tg_t;

    /* ***************
     * Dispatcher handlers
     * *************** */
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

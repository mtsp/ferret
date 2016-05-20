/**
 * Common.h
 * Description: General structures regarding the task graph.
 *  - Basic relationship would be Graph -> Task -> Dep
 *  - Also contains methods for file management (.dat, .dot) both storing
 *    and restoring
 * */

#include <stdlib.h>
#include <vector>
#include <list>

#include <fstream>
#include <string.h>

// headers that implement a archive in simple text format
#include "boost/archive/text_iarchive.hpp"
#include "boost/archive/text_oarchive.hpp"

// serialize vectors
#include "boost/serialization/vector.hpp"

// serialize list
#include "boost/serialization/list.hpp"

#ifndef COMMON
#define COMMON

typedef unsigned int uint;
typedef enum Type { IN, INOUT, OUT } Type;

/**
 * Structure to describe a dependency between tasks
 */
typedef struct Dep {
    uint task;      // task that the dependency is heading towards to
    Type type;      // type of dependency
    uint index;     // the index of the dependency

private:
    /* Serialization */
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &ar, const uint version) {
        ar & task;
        ar & type;
        ar & index;
    }
} Dep;

/**
 * Structure to describe a task
 */
typedef struct Task {
    std::list<Dep> predecessors;    // predecessors tasks
    std::list<Dep> successors;      // by default, all successor tasks are OUT

    uint C_dep_tasks;               // total number of predecessors
    float load;                     // how long should the task remain on load

private:
    /* Serialization */
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &ar, const uint version) {
        ar & predecessors;
        ar & successors;
        ar & C_dep_tasks;
        ar & load;
    }
} Task;

/**
 * Structure to describe a complete graph of tasks
 */
class Graph {
public:
    std::vector<Task> tasks;        // tasks structure
    uint  total_tasks;              // total number of tasks
    uint  total_deps;               // total number of dependencies between tasks

    uint  dep_range;                // max range of how far a predecessor may be

    uint  load_time;                // describes the standard load time per task (ms)
    float max_range;                // max. range from standard load time (0 to 1)

    /**
     * Default constructor
     */
    Graph() {
        Graph(0, 0, 0, 0);          // standard param.
    }

    /**
     * Complete constructor
     *  n: number of tasks
     *  d: dep. range
     *  l: standard load time
     *  r: max. range from standard load time
     */
    Graph(uint n, uint d, uint t, float r) {
        tasks.resize(n);

        total_tasks = n;
        total_deps = 0;

        dep_range = d;

        load_time = t;
        max_range = r;
    }

    /**
     * Save a graph as a .dat file, by serializing it
     *  g: graph to be serialized
     *  filename: name of the file
     */
    static void save(const Graph &g, const char* filename) {
        // save graph into internal representation
        char* filename_ = Graph::add_extension(filename, ".dat");

        // make an archive
        std::ofstream ofs(filename_);

        if (ofs.is_open()) {
            boost::archive::text_oarchive oa(ofs);

            oa << g;
        } else {
            fprintf(stderr, "Invalid filename.\n");

            exit(1);
        }

        delete filename_;
    }

    /**
     * Restore a graph from a .dat file, by deserializing it
     *  g: where the graph should be restored
     *  filename: name of the serialized graph file
     */
    static void restore(Graph **g, const char* filename) {
        // restore by default internal representation
        char* filename_ = Graph::add_extension(filename, ".dat");

        // allocate memory
        *g = new Graph();

        // open the archive
        std::ifstream ifs(filename_);

        if (ifs.is_open()) {
            boost::archive::text_iarchive ia(ifs);

            // restore the schedule from the archive
            (*g)->tasks.clear();
            ia >> **g;
        } else {
            fprintf(stderr, "Invalid filename.\n");

            exit(1);
        }

        delete filename_;
    }

    /**
     * Save a graph as a .dot file
     *  g: graph to be saved
     *  filename: name of the file
     */
    static void show(const Graph &g, const char* filename) {
        // restore by default dot file
        char* filename_ = Graph::add_extension(filename, ".dot");

        std::ofstream ofs (filename_, std::ofstream::out);

        ofs << "digraph taskgraph {\n";

        for (uint i = 0; i < g.total_tasks; i++) {
            ofs << "\tT" << i << " [label= \"T" << i << "\\n load: " << g.tasks[i].load << "\"];\n";

            // if there are successors on the following task
            if (!g.tasks[i].successors.empty()) {
                std::list<Dep>::const_iterator it;

                for (it = g.tasks[i].successors.begin(); it != g.tasks[i].successors.end(); ++it) {
                    ofs << "\tT" << i << " -> T" << it->task << "[label=" << it->index << "];\n";
                }
            }
        }

        ofs << "}";

        ofs.close();

        delete filename_;
    }

private:
    /* Serialization */
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & f, const uint version) {
        f & tasks;
        f & total_tasks;
        f & total_deps;
        f & dep_range;
        f & load_time;
        f & max_range;
    }

    /**
     * Helper function to add a extension to a string
     *  filename:  original string
     *  extension: extension to be added to the string
     *
     *  return:    concatenation of string with extension
     */
    static char* add_extension(const char* filename, const char* extension) {
        // save graph into internal representation
        char* filename_ = new char[strlen(filename) + strlen(extension) + 1];

        // add extension
        strcpy(filename_, filename);
        strcat(filename_, extension);

        return filename_;
    }
};

#endif
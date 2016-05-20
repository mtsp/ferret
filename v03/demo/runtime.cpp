#include <iostream>
#include <cstdlib>
#include <dlfcn.h>
using namespace std;

typedef void (*fun_type1)();
typedef void (*fun_type2)(int param);

bool isDebugging = false;
fun_type1 init_task_lab = NULL;
fun_type2 event_ocurred = NULL;

void tasks_begin() {
	cout << "runtime: tasks_begin" << endl;

    if (const char* path_task_lab = std::getenv("MTSP_DEBUGGING")) {
		cout << "We are debugging." << endl;
		isDebugging = true;

		// carrega a library do task lab
		void *myso = dlopen(path_task_lab, RTLD_NOW);

		if (myso == NULL) 
			cout << "Error loading shared library." << endl;

		// obtem ponteiros para as funcoes do task lab
		init_task_lab = (fun_type1) dlsym(myso, "init_task_lab");
		event_ocurred = (fun_type2) dlsym(myso, "event_ocurred");

		if (init_task_lab == NULL)
			cout << "era null" << endl;

		// chama a funcao de inicializacao do tasklab
		init_task_lab();
    }
	else {
		isDebugging = false;
		cout << "We are NOT debugging." << endl;
	}
}

void watch_event(int event) {
	cout << "runtime: tasklab asked to watch event " << event << endl;
}

void task_submit(int param) {
	cout << "runtime: task_submit com param " << param << endl;

	if (isDebugging) event_ocurred(123);
	if (isDebugging) event_ocurred(123);
	if (isDebugging) event_ocurred(456);
	if (isDebugging) event_ocurred(789);
}

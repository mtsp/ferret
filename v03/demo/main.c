#include <iostream>
using namespace std;

extern void tasks_begin();
extern void task_submit(int);

int main() {

	// simula inicializando o sistema de tasks
	tasks_begin();

	// simula adicionando uma task na submission queue, 
	// o "10" eh apenas para ilustrar passando parametro
	task_submit(10);


	return 0;
}

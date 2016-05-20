#include <iostream>
using namespace std;

extern void watch_event(int);

extern "C" {

void init_task_lab() {
	cout << "tasklab: registrando evento 123" << endl;
	watch_event(123);

	cout << "tasklab: registrando evento 456" << endl;
	watch_event(456);

	cout << "tasklab: registrando evento 789" << endl;
	watch_event(789);
}

void event_ocurred(int event) {
	cout << "tasklab: notificacao que o evento " << event << " ocorreu." << endl;	
}

}

#ifdef STANDALONE
	int main() {

		// se o task lab esta sendo compilado standalone entao ele tem um metodo
		// main e se comportaria como se fosse o programa main do usuario, ou seja
		// fazendo requests para o mtsp.

	}
#endif

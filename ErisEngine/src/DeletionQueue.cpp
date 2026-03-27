#include "DeletionQueue.h"

void DeletionQueue::push_function(std::function<void()>&& function) {
	deletors.push_back(function);
}

void DeletionQueue::flush() {
	for (auto it = deletors.rbegin(); it != deletors.rend(); ++it){
		(*it)();
	}
	deletors.clear();
}
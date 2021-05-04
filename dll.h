// struct for holding pids of non-finished background child processes
struct dllNode {
	int* pid;
	struct dllNode* prev;
	struct dllNode* next;
};
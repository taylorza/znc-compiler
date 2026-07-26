#ifndef CALLGRAPH_H_
#define CALLGRAPH_H_

#define CALLGRAPH_MAX_FUNCS 256

void callgraph_init(void) MYCC;
uint16_t callgraph_add_func(int name_id) MYCC;
void callgraph_add_edge(uint16_t caller_id, uint16_t callee_id) MYCC;
void callgraph_mark_reachable(void) MYCC;

#endif // CALLGRAPH_H_
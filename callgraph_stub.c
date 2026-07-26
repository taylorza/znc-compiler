#include "znc.h"
#include "callgraph.h"

extern void far_callgraph_init(void) MYCC;
extern uint16_t far_callgraph_add_func(int name_id) MYCC;
extern void far_callgraph_add_edge(uint16_t caller_id, uint16_t callee_id) MYCC;
extern void far_callgraph_mark_reachable(void) MYCC;

void callgraph_init(void) MYCC {
    PROLOG(48)
    far_callgraph_init();
    EPILOG
}
uint16_t callgraph_add_func(int name_id) MYCC {
    uint16_t id;
    PROLOG(48)
        id = far_callgraph_add_func(name_id);
    EPILOG_RETURN(id);
}
void callgraph_add_edge(uint16_t caller_id, uint16_t callee_id) MYCC {
    PROLOG(48)
        far_callgraph_add_edge(caller_id, callee_id);
    EPILOG
}
void callgraph_mark_reachable(void) MYCC {
    PROLOG(48)
        far_callgraph_mark_reachable();
    EPILOG
}
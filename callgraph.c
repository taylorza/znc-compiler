#include "znc.h"
#include "callgraph.h"

typedef struct {
    uint8_t bits[CALLGRAPH_MAX_FUNCS / 8];
} Bitset;
Bitset callgraph[CALLGRAPH_MAX_FUNCS]; // 0 = top level

static inline void bitset_set(Bitset* set, uint16_t id) {
    set->bits[id >> 3] |= (uint8_t)(1 << (id & 7));
}

static inline uint8_t bitset_test(Bitset* set, uint16_t id) {
    return (set->bits[id >> 3] & (uint8_t)(1 << (id & 7))) != 0;
}

typedef struct {
    int func_id;
    int name_id;
} FuncId;

FuncId func_ids[CALLGRAPH_MAX_FUNCS];

typedef struct {
    uint16_t data[CALLGRAPH_MAX_FUNCS];
    uint16_t head, tail;
} Queue;

static void queue_init(Queue* q) {
    q->head = 0;
    q->tail = 0;
}

static uint8_t queue_is_empty(Queue* q) {
    return q->head == q->tail;
}

static void queue_push(Queue* q, uint16_t value) {
    q->data[q->tail] = value;
    q->tail = (q->tail + 1) % CALLGRAPH_MAX_FUNCS;
}

static uint16_t queue_pop(Queue* q) {
    uint16_t value = q->data[q->head];
    q->head = (q->head + 1) % CALLGRAPH_MAX_FUNCS;
    return value;
}

void far_callgraph_init(void) MYCC {
    memset(callgraph, 0, sizeof(callgraph));
    memset(func_ids, -1, sizeof(func_ids)); // Initialize func_ids to -1
}

SYMBOL callee_sym;
uint16_t far_callgraph_add_func(int name_id) MYCC {
    for (int i = 1; i < CALLGRAPH_MAX_FUNCS; ++i) {
        if (func_ids[i].name_id == name_id) {
            return i; // Function already exists
        }
        
        if (func_ids[i].func_id == -1) {
            func_ids[i].func_id = i;
            func_ids[i].name_id = name_id;
            return i;
        }
    }
    return 0xffff; // No available slot
}

void far_callgraph_add_edge(uint16_t caller_id, uint16_t callee_id) MYCC {
    if (caller_id >= CALLGRAPH_MAX_FUNCS || callee_id >= CALLGRAPH_MAX_FUNCS) {
        return; // Out of bounds
    }

    bitset_set(&callgraph[caller_id], callee_id);
}

Queue q;
SYMBOL caller_sym;
SYMBOL callee_sym;
void far_callgraph_mark_reachable(void) MYCC {
    queue_init(&q);

    queue_push(&q, 0);
    while (!queue_is_empty(&q)) {
        uint16_t current = queue_pop(&q);
        for (int i = 1; i < CALLGRAPH_MAX_FUNCS; ++i) {
            if (i == current) continue; // Skip self

            if (bitset_test(&callgraph[current], i)) {
                if (current != 0) {
                    // Mark the callee function as reachable
                    callee_sym = lookupFuncByNameId(func_ids[i].name_id);
                    if (is_defined(&callee_sym)) {
                        callee_sym.flags |= SYM_FLAG_USED;
                        updatesym(&callee_sym);
                    }
                }
                if (func_ids[i].func_id != -1) {
                    queue_push(&q, i);
                    func_ids[i].func_id = -1;
                }
            }
        }
    }
}



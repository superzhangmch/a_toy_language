#ifndef GC_H
#define GC_H

#include <stddef.h>
#include "runtime.h"

// GC object header - prepended to every heap-allocated object
typedef struct GCObject {
    int type;                   // Object type (TYPE_ARRAY, TYPE_DICT, etc.)
    int marked;                 // Mark bit for GC
    size_t size;                // Size of the object data
    struct GCObject *next;      // Linked list of all objects
} GCObject;

// Root stack for tracking Value* on stack
#define MAX_ROOTS 1024

typedef struct {
    Value **roots[MAX_ROOTS];   // Stack of pointers to Value variables
    int root_count;             // Current number of roots

    GCObject *all_objects;      // Linked list of all allocated objects
    int num_objects;            // Current number of objects
    int max_objects;            // Threshold to trigger GC

    size_t heap_size;           // Current heap size in bytes
    size_t max_heap_size;       // Heap size threshold

    void *stack_bottom;         // Bottom of stack for conservative scanning

    // Heap address range for fast filtering
    void *heap_start;           // Lowest heap address seen
    void *heap_end;             // Highest heap address seen
} GC;

// Global GC instance
extern GC gc;

// GC API
void gc_init(void);
void gc_set_stack_bottom(void *bottom);  // Set stack bottom for scanning
void* gc_alloc(int type, size_t size);
void gc_collect(void);

// Root management - called by generated code
void gc_push_root(Value *v);
void gc_pop_root(void);

// Manual marking (for global variables)
void gc_mark_value(Value *v);

// Statistics
void gc_print_stats(void);

#endif // GC_H

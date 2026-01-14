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
    struct GCObject *hash_next; // Linked list in hash bucket
} GCObject;

// Root stack for tracking Value* on stack
#define MAX_ROOTS 1024

// Hash table for fast object lookup
#define GC_HASH_SIZE 1024

typedef struct {
    Value *roots[MAX_ROOTS];    // Stack of pointers to Value structs
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

    // Hash table for O(1) object lookup during stack scanning
    GCObject *hash_table[GC_HASH_SIZE];

    // Cumulative statistics
    int total_collections;      // Total number of GC runs
    int total_objects_freed;    // Total objects freed across all collections
    size_t total_bytes_freed;   // Total bytes freed across all collections
} GC;

// Global GC instance
extern GC gc;

// GC API
void gc_init(void);
void gc_set_stack_bottom(void *bottom);  // Set stack bottom for scanning
void* gc_alloc(int type, size_t size);
void* gc_realloc(void *old_ptr, int type, size_t old_size, size_t new_size);
void gc_collect(void);

// Root management - called by generated code
void gc_push_root(Value *v);
void gc_pop_root(void);

// Manual marking (for global variables)
void gc_mark_value(Value *v);

// Statistics
void gc_print_stats(void);

#endif // GC_H

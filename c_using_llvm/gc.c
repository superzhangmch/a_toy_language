#include "gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Global GC instance
GC gc;

// Initialize GC
void gc_init(void) {
    gc.root_count = 0;
    gc.all_objects = NULL;
    gc.num_objects = 0;
    gc.max_objects = 100;  // Initial threshold
    gc.heap_size = 0;
    gc.max_heap_size = 1024 * 1024;  // 1MB initial
    gc.stack_bottom = NULL;
    gc.heap_start = (void*)~(size_t)0;  // Max address
    gc.heap_end = NULL;                  // Min address
    gc.total_collections = 0;
    gc.total_objects_freed = 0;
    gc.total_bytes_freed = 0;

    // Initialize hash table
    for (int i = 0; i < GC_HASH_SIZE; i++) {
        gc.hash_table[i] = NULL;
    }

    printf("GC: Initialized (threshold: %d objects)\n", gc.max_objects);
}

// Set stack bottom for conservative scanning
void gc_set_stack_bottom(void *bottom) {
    gc.stack_bottom = bottom;
}

// Convert user pointer to GC object header
static GCObject* ptr_to_gcobject(void *ptr) {
    return ((GCObject*)ptr) - 1;
}

// Convert GC object header to user pointer
static void* gcobject_to_ptr(GCObject *obj) {
    return (void*)(obj + 1);
}

// Forward declarations
static void mark_value(Value *v);
static GCObject* find_gc_object(void *ptr);

// Mark an array's elements
static void mark_array(Array *a) {
    if (!a) return;

    // Mark the data buffer itself (it's also a GC object)
    if (a->data) {
        // Validate that a->data points to a valid GC object
        GCObject *data_obj = find_gc_object(a->data);
        if (data_obj && !data_obj->marked) {
            data_obj->marked = 1;

            // Mark the element values
            Value *elements = (Value*)a->data;
            for (int i = 0; i < a->size; i++) {
                mark_value(&elements[i]);
            }
        }
    }
}

// Mark a dict's values
static void mark_dict(Dict *d) {
    if (!d) return;

    for (int i = 0; i < 256; i++) {  // HASH_SIZE = 256
        DictEntry *entry = d->buckets[i];
        while (entry) {
            mark_value(&entry->value);
            entry = entry->next;
        }
    }
}

// Mark a class instance's fields
static void mark_instance(Instance *inst) {
    if (!inst) return;

    // Mark the fields dict
    mark_value(&inst->fields);
}

// Recursively mark a Value and its children
static void mark_value(Value *v) {
    if (!v) return;

    // Only heap-allocated types need marking
    if (v->type != TYPE_ARRAY && v->type != TYPE_DICT &&
        v->type != TYPE_STRING && v->type != TYPE_INSTANCE &&
        v->type != TYPE_CLASS) {
        return;  // Primitives (int, float, bool, null) - no marking needed
    }

    if (!v->data) return;  // Null pointer

    // Find the GC object header
    GCObject *obj = ptr_to_gcobject((void*)v->data);

    // Already marked? Avoid infinite recursion
    if (obj->marked) return;

    obj->marked = 1;

    // Recursively mark children based on type
    switch (v->type) {
        case TYPE_ARRAY:
            mark_array((Array*)v->data);
            break;
        case TYPE_DICT:
            mark_dict((Dict*)v->data);
            break;
        case TYPE_INSTANCE:
            mark_instance((Instance*)v->data);
            break;
        case TYPE_STRING:
            // Strings have no children
            break;
        case TYPE_CLASS:
            // Classes are static, no children to mark
            break;
    }
}

// Hash function for pointer addresses
static inline size_t hash_ptr(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    // Simple hash: use middle bits of address
    return (addr >> 3) % GC_HASH_SIZE;
}

// Check if a pointer points to a GC object (optimized with hash table)
static GCObject* find_gc_object(void *ptr) {
    size_t hash = hash_ptr(ptr);

    // Search in hash bucket
    for (GCObject *obj = gc.hash_table[hash]; obj; obj = obj->hash_next) {
        void *obj_start = gcobject_to_ptr(obj);
        void *obj_end = (char*)obj_start + obj->size;

        // Check if ptr points to this object (or interior pointer)
        if (ptr >= obj_start && ptr < obj_end) {
            return obj;
        }
    }
    return NULL;
}

// Conservative stack scanning
static void scan_stack(void) {
    if (!gc.stack_bottom) {
        // Stack bottom not set, skip scanning
        return;
    }

    void *stack_top;
    volatile int dummy;
    stack_top = (void*)&dummy;

    // Stack grows downward, scan from current position to bottom
    void *start = stack_top;
    void *end = gc.stack_bottom;

    // Ensure correct order
    if (start > end) {
        void *tmp = start;
        start = end;
        end = tmp;
    }

    // Scan stack with 8-byte alignment (word-aligned)
    // More efficient than byte-by-byte scanning while still conservative
    size_t word_size = sizeof(void*);

    // Align start pointer to word boundary
    char *aligned_start = (char*)((((uintptr_t)start + word_size - 1) / word_size) * word_size);

    for (char *p = aligned_start; p + word_size <= (char*)end; p += word_size) {
        void *potential_ptr = *(void**)p;

        // Skip null pointers
        if (!potential_ptr) continue;

        // Fast filter: check if pointer is in heap address range
        if (potential_ptr < gc.heap_start || potential_ptr >= gc.heap_end) {
            continue;
        }

        // Check if this looks like a heap pointer
        GCObject *obj = find_gc_object(potential_ptr);
        if (obj && !obj->marked) {
            // Recursively mark based on object type
            // IMPORTANT: Use the correct object start pointer, not potential_ptr
            // (which might be an interior pointer)
            // DON'T set marked=1 here - let mark_value() do it!
            Value v;
            v.data = (long)gcobject_to_ptr(obj);
            v.type = obj->type;
            mark_value(&v);
        }
    }
}

// Mark phase: mark all reachable objects from roots
static void mark_from_roots(void) {
    // Conservative stack scanning (like Boehm GC)
    scan_stack();

    // Also mark from explicit roots (if any)
    for (int i = 0; i < gc.root_count; i++) {
        if (gc.roots[i]) {
            mark_value(gc.roots[i]);
        }
    }
}

// Sweep phase: free unmarked objects
static void sweep(void) {
    GCObject **obj_ptr = &gc.all_objects;

    while (*obj_ptr) {
        GCObject *obj = *obj_ptr;

        if (!obj->marked) {
            // Unmarked - remove from list and free
            *obj_ptr = obj->next;

            // Remove from hash table
            void *ptr = gcobject_to_ptr(obj);
            size_t hash = hash_ptr(ptr);
            GCObject **hash_ptr = &gc.hash_table[hash];
            while (*hash_ptr) {
                if (*hash_ptr == obj) {
                    *hash_ptr = obj->hash_next;
                    break;
                }
                hash_ptr = &(*hash_ptr)->hash_next;
            }

            gc.heap_size -= obj->size;
            gc.num_objects--;

            free(obj);
        } else {
            // Marked - clear mark for next GC cycle
            obj->marked = 0;
            obj_ptr = &obj->next;
        }
    }
}

// Main GC collection function
void gc_collect(void) {
    int before = gc.num_objects;
    size_t before_size = gc.heap_size;

    // Mark phase
    mark_from_roots();

    // Sweep phase
    sweep();

    int after = gc.num_objects;
    size_t after_size = gc.heap_size;

    // Update cumulative statistics
    int freed_objects = before - after;
    size_t freed_bytes = before_size - after_size;
    gc.total_collections++;
    gc.total_objects_freed += freed_objects;
    gc.total_bytes_freed += freed_bytes;

    // Adjust threshold based on collection results
    if (after > 0) {
        gc.max_objects = after * 2;
    } else {
        gc.max_objects = 100;  // Minimum threshold
    }

    // Uncomment for debugging:
    // printf("GC: Collected %d objects (%zu bytes), %d objects (%zu bytes) remain, next threshold: %d\n",
    //        freed_objects, freed_bytes,
    //        after, after_size, gc.max_objects);
}

// Reallocate GC-managed memory (like realloc but for GC)
void* gc_realloc(void *old_ptr, int type, size_t old_size, size_t new_size) {
    if (!old_ptr) {
        return gc_alloc(type, new_size);
    }

    if (new_size <= old_size) {
        // No need to reallocate if shrinking or same size
        return old_ptr;
    }

    // Allocate new memory
    void *new_ptr = gc_alloc(type, new_size);

    // Copy old data
    memcpy(new_ptr, old_ptr, old_size);

    // Old memory will be collected by GC
    return new_ptr;
}

// Allocate object with GC
void* gc_alloc(int type, size_t size) {
    // Check if GC should run
    if (gc.num_objects >= gc.max_objects) {
        gc_collect();
    }

    // Allocate object with header
    GCObject *obj = (GCObject*)malloc(sizeof(GCObject) + size);

    if (!obj) {
        // Out of memory, try GC and retry
        printf("GC: malloc failed, running emergency GC\n");
        gc_collect();

        obj = (GCObject*)malloc(sizeof(GCObject) + size);
        if (!obj) {
            fprintf(stderr, "GC: Fatal - out of memory\n");
            exit(1);
        }
    }

    // Initialize header
    obj->type = type;
    obj->marked = 0;
    obj->size = size;

    // Add to global object list
    obj->next = gc.all_objects;
    gc.all_objects = obj;

    // Add to hash table for fast lookup
    void *ptr = gcobject_to_ptr(obj);
    size_t hash = hash_ptr(ptr);
    obj->hash_next = gc.hash_table[hash];
    gc.hash_table[hash] = obj;

    gc.num_objects++;
    gc.heap_size += size;

    // Update heap address range for fast filtering in stack scan
    void *obj_start = (void*)obj;
    void *obj_end = (char*)ptr + size;
    if (obj_start < gc.heap_start) gc.heap_start = obj_start;
    if (obj_end > gc.heap_end) gc.heap_end = obj_end;

    // Zero-initialize the memory
    memset(ptr, 0, size);

    return ptr;
}

// Push a root onto the root stack
void gc_push_root(Value *v) {
    if (gc.root_count >= MAX_ROOTS) {
        fprintf(stderr, "GC: Root stack overflow\n");
        exit(1);
    }

    gc.roots[gc.root_count++] = v;
}

// Pop a root from the root stack
void gc_pop_root(void) {
    if (gc.root_count <= 0) {
        fprintf(stderr, "GC: Root stack underflow\n");
        exit(1);
    }

    gc.root_count--;
}

// Manually mark a value (for global variables)
void gc_mark_value(Value *v) {
    mark_value(v);
}

// Print GC statistics
void gc_print_stats(void) {
    printf("\n=== GC Statistics ===\n");
    printf("Current objects: %d (threshold: %d)\n", gc.num_objects, gc.max_objects);
    printf("Current heap size: %zu bytes (max: %zu)\n", gc.heap_size, gc.max_heap_size);
    printf("Root stack: %d / %d\n", gc.root_count, MAX_ROOTS);
    printf("\n");
    printf("Total collections: %d\n", gc.total_collections);
    printf("Total objects freed: %d\n", gc.total_objects_freed);
    printf("Total bytes freed: %zu\n", gc.total_bytes_freed);
    printf("====================\n\n");
}

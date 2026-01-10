#include "runtime.h"

// Array structure
typedef struct {
    int size;
    int capacity;
    void *data;
} Array;

// Helper to create array
static Array* new_array() {
    Array *a = malloc(sizeof(Array));
    a->size = 0;
    a->capacity = 8;
    a->data = malloc(8 * sizeof(Value));
    return a;
}

// Create empty array
Value make_array(void) {
    Array *a = new_array();
    Value result = {TYPE_ARRAY, (long)a};
    return result;
}

// Append value to array
Value append(Value arr, Value val) {
    Array *a = (Array*)(arr.data);
    if (a->size >= a->capacity) {
        a->capacity *= 2;
        a->data = realloc(a->data, a->capacity * sizeof(Value));
    }
    ((Value*)a->data)[a->size++] = val;
    Value result = {TYPE_INT, 0};
    return result;
}

// Get array element
Value array_get(Value arr, Value index) {
    Array *a = (Array*)(arr.data);
    long idx = index.data;
    if (idx >= 0 && idx < a->size) {
        return ((Value*)a->data)[idx];
    }
    Value result = {TYPE_INT, 0};
    return result;
}

// Set array element
Value array_set(Value arr, Value index, Value val) {
    Array *a = (Array*)(arr.data);
    long idx = index.data;
    if (idx >= 0 && idx < a->size) {
        ((Value*)a->data)[idx] = val;
    }
    return val;
}

// Get length
Value len(Value v) {
    if (v.type == TYPE_ARRAY) {
        Array *a = (Array*)(v.data);
        Value result = {TYPE_INT, a->size};
        return result;
    } else if (v.type == TYPE_STRING) {
        char *s = (char*)(v.data);
        Value result = {TYPE_INT, strlen(s)};
        return result;
    }
    Value result = {TYPE_INT, 0};
    return result;
}

// Type conversion functions
Value to_int(Value v) {
    if (v.type == TYPE_INT) return v;
    if (v.type == TYPE_FLOAT) {
        double f = *(double*)&v.data;
        Value result = {TYPE_INT, (long)f};
        return result;
    }
    Value result = {TYPE_INT, 0};
    return result;
}

Value to_float(Value v) {
    if (v.type == TYPE_FLOAT) return v;
    if (v.type == TYPE_INT) {
        double f = (double)v.data;
        Value result = {TYPE_FLOAT, *(long*)&f};
        return result;
    }
    double f = 0.0;
    Value result = {TYPE_FLOAT, *(long*)&f};
    return result;
}

Value to_string(Value v) {
    char buf[64];
    if (v.type == TYPE_INT) {
        snprintf(buf, sizeof(buf), "%ld", v.data);
        char *s = strdup(buf);
        Value result = {TYPE_STRING, (long)s};
        return result;
    } else if (v.type == TYPE_FLOAT) {
        double f = *(double*)&v.data;
        snprintf(buf, sizeof(buf), "%g", f);
        char *s = strdup(buf);
        Value result = {TYPE_STRING, (long)s};
        return result;
    } else if (v.type == TYPE_STRING) {
        return v;
    }
    Value result = {TYPE_STRING, (long)""};
    return result;
}

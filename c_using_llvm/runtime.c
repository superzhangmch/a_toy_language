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

// Convert value to string representation (like str() in Python)
Value str(Value v) {
    return to_string(v);
}

// Get type name as string
Value type(Value v) {
    const char *type_name;
    if (v.type == TYPE_INT) {
        type_name = "int";
    } else if (v.type == TYPE_FLOAT) {
        type_name = "float";
    } else if (v.type == TYPE_STRING) {
        type_name = "string";
    } else if (v.type == TYPE_ARRAY) {
        type_name = "array";
    } else {
        type_name = "unknown";
    }
    char *s = strdup(type_name);
    Value result = {TYPE_STRING, (long)s};
    return result;
}

// Slice array or string
Value slice_access(Value obj, Value start_v, Value end_v) {
    long start = start_v.data;
    long end = end_v.data;

    if (obj.type == TYPE_ARRAY) {
        Array *a = (Array*)(obj.data);
        long size = a->size;

        // Handle negative indices
        if (start < 0) start += size;
        if (end < 0) end += size;

        // Clamp to bounds
        if (start < 0) start = 0;
        if (end > size) end = size;
        if (start > end) start = end;

        // Create new array with slice
        Array *new_a = new_array();
        for (long i = start; i < end; i++) {
            Value elem = ((Value*)a->data)[i];
            if (new_a->size >= new_a->capacity) {
                new_a->capacity *= 2;
                new_a->data = realloc(new_a->data, new_a->capacity * sizeof(Value));
            }
            ((Value*)new_a->data)[new_a->size++] = elem;
        }

        Value result = {TYPE_ARRAY, (long)new_a};
        return result;
    } else if (obj.type == TYPE_STRING) {
        char *s = (char*)(obj.data);
        long size = strlen(s);

        // Handle negative indices
        if (start < 0) start += size;
        if (end < 0) end += size;

        // Clamp to bounds
        if (start < 0) start = 0;
        if (end > size) end = size;
        if (start > end) start = end;

        // Create new string with slice
        long len = end - start;
        char *new_s = malloc(len + 1);
        strncpy(new_s, s + start, len);
        new_s[len] = '\0';

        Value result = {TYPE_STRING, (long)new_s};
        return result;
    }

    Value result = {TYPE_STRING, (long)strdup("")};
    return result;
}

// Read input from user
Value input(Value prompt) {
    if (prompt.type == TYPE_STRING) {
        char *s = (char*)(prompt.data);
        printf("%s", s);
        fflush(stdout);
    }

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        char *s = strdup(buffer);
        Value result = {TYPE_STRING, (long)s};
        return result;
    }

    Value result = {TYPE_STRING, (long)strdup("")};
    return result;
}

// Read file contents
Value read(Value filename) {
    char *fname = (char*)(filename.data);
    FILE *f = fopen(fname, "rb");

    if (f == NULL) {
        Value result = {TYPE_STRING, (long)strdup("")};
        return result;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    Value result = {TYPE_STRING, (long)content};
    return result;
}

// Write content to file
Value write(Value content, Value filename) {
    char *fname = (char*)(filename.data);
    FILE *f = fopen(fname, "w");

    if (f == NULL) {
        Value result = {TYPE_INT, 0};
        return result;
    }

    if (content.type == TYPE_STRING) {
        char *s = (char*)(content.data);
        fprintf(f, "%s", s);
    } else if (content.type == TYPE_INT) {
        fprintf(f, "%ld", content.data);
    } else if (content.type == TYPE_FLOAT) {
        double d = *(double*)&content.data;
        fprintf(f, "%g", d);
    }

    fclose(f);
    Value result = {TYPE_INT, 1};
    return result;
}

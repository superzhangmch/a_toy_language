# Pi calculation using spigot algorithm
# Carefully translated from C implementation

# Initialize array with 8401 elements
var f = []
var size = 8401
var i = 0
while (i < size) {
    append(f, 2000)  # a/5 where a=10000
    i = i + 1
}

var a = 10000
var c = size - 1
var d = 0
var e = 0
var g = 0

# Main calculation loop
while (c > 0) {
    g = c * 2
    d = 0
    var b = c

    # Inner loop processes digits from index c down to 1 (not 0)
    while (b > 0) {
        d = d + f[b] * a
        g = g - 1
        f[b] = d % g
        b = b - 1

        # Only divide and multiply if b is still positive
        if (b > 0) {
            d = d / g
            g = g - 1
            d = d * b
        }
    }

    # Print 4-digit group
    var result = e + d / a
    print(result)

    e = d % a
    c = c - 14
}

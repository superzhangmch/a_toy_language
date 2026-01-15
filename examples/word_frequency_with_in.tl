# Word Frequency Counter - Using IN operator with dict
# Much cleaner and faster than parallel arrays!

var file_path = cmd_args()[0]
var content = read(file_path)

println("=== Word Frequency Counter (With IN operator) ===")
println("File: interpreter.c")
println("File size: ", len(content), " characters")
println("")

# Now we can use a DICT instead of parallel arrays!
var freq_dict = {}
var current_word = ""
var total_words = 0

# Iterate through file content
var i = 0
while (i < len(content)) {
    var ch = content[i]
    var is_alpha = 0

    if (ch >= "a" and ch <= "z") { is_alpha = 1 }
    if (ch >= "A" and ch <= "Z") { is_alpha = 1 }
    if (ch >= "0" and ch <= "9") { is_alpha = 1 }
    if (ch == "_") { is_alpha = 1 }

    if (is_alpha == 1) {
        current_word = str(current_word) + str(ch)
    }

    if (is_alpha == 0 and len(current_word) > 0) {
        total_words += 1

        # Use IN operator to check if word exists in dict!
        if (current_word in freq_dict) {
            freq_dict[current_word] = freq_dict[current_word] + 1
        } else {
            freq_dict[current_word] = 1
        }

        current_word = ""
    }

    i += 1
}

# Last word
if (len(current_word) > 0) {
    total_words += 1
    if (current_word in freq_dict) {
        freq_dict[current_word] = freq_dict[current_word] + 1
    } else {
        freq_dict[current_word] = 1
    }
}

println("Total words: ", total_words)

# Convert dict to arrays for sorting
var word_list = []
var freq_list = []

var keys = keys(freq_dict)
for(i => word in keys) {
    append(word_list, word)
    append(freq_list, freq_dict[word])
}

println("Unique words: ", len(word_list))
println("")

# Bubble sort by frequency (descending)
println("Sorting...")
var n = len(word_list)
var pass = 0

while (pass < n - 1) {
    var m = 0
    while (m < n - 1 - pass) {
        if (freq_list[m] < freq_list[m + 1]) {
            var temp_f = freq_list[m]
            freq_list[m] = freq_list[m + 1]
            freq_list[m + 1] = temp_f

            var temp_w = word_list[m]
            word_list[m] = word_list[m + 1]
            word_list[m + 1] = temp_w
        }
        m += 1
    }
    pass += 1
}

println("")
println("=== Top 30 Most Frequent Words ===")
println("")

var idx = 0
while (idx < 30 and idx < len(word_list)) {
    println(idx + 1, ". ", word_list[idx], ": ", freq_list[idx])
    idx += 1
}

println("")
println("Done!")
gc_stat()

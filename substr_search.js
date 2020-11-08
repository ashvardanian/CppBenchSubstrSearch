
/**
 * Source: https://stackoverflow.com/a/7924240
 * 
 * @param {*} haystack 
 * @param {*} needle 
 * @param {*} allow_overlap 
 */
function count_occurrences(haystack, needle, allow_overlap) {

    if (needle.length > haystack.length) 
        return 0;

    var n = 0,
        pos = 0,
        step = allow_overlap ? 1 : needle.length;

    while (true) {
        // https://www.w3schools.com/jsref/jsref_indexof.asp
        pos = haystack.indexOf(needle, pos);
        if (pos >= 0) {
            n++;
            pos += step;
        } else break;
    }
    return n;
}

function random_string(length, chars) {
    var result = '';
    for (var i = 0; i < length; i++) 
        result += chars[Math.floor(Math.random() * chars.length)];
    return result;
}

let haystack_size = 1024 * 1024 * 64;
let allowed_chars = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
let haystack = random_string(haystack_size, allowed_chars);
let needles = new Array(1024).fill(undefined).map(()=> random_string(6, allowed_chars));

var total_matches = 0;
let start_time = new Date();
for (needle in needles) {
    total_matches += count_occurrences(haystack, needle, true);
}
let secs = (new Date().getTime() - start_time.getTime()) / 1000.0;

console.info('Execution time: %d s', secs);
console.info('Bytes/sec: %d', Math.floor(haystack_size * needles.length / secs))
console.info('Total matches: %d', total_matches);

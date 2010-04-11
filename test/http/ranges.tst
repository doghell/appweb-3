/*
 *  ranges.tst - Test http with ranges
 */

load("http/support.es")

//  Ranges
assert(run("--range 0-4 /numbers.html") == "01234")
assert(run("--range -5 /numbers.html") == "5678")

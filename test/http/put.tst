/*
 *  put.tst - Test the put command
 */

load("http/support.es")

//  PUT file
cleanDir("web/tmp")
run("http/test.dat /tmp/day.tmp")
assert(exists("web/tmp/day.tmp"))

//  PUT files
cleanDir("web/tmp")
run("http/*.tst /tmp/")
assert(exists("web/tmp/basic.tst"))
cleanDir("web/tmp")

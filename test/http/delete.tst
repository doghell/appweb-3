/*
 *  delete.tst - Test http delete
 */

load("http/support.es")

//  DELETE
run("http/test.dat /tmp/test.dat")
assert(exists("web/tmp/test.dat"))
run("--method DELETE /tmp/test.dat")
assert(!exists("web/tmp/test.dat"))


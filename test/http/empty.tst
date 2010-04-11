/*
 *  empty.tst - Empty response
 */

load("http/support.es")

//  Empty get
data = run("/empty.html")
assert(data == "")


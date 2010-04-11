/*
 *  chunked.tst - Test chunked transfers
 */

load("http/support.es")

//  Chunked get
data = run("--chunk 256 /big.txt")
assert(data.startsWith("012345678"))
assert(data.endsWith("END"))

//  Chunked empty get
data = run("--chunk 100 /empty.html")
assert(data == "")


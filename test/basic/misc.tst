/*
 *  misc.tst - Misc. Http tests
 */

assert(Http.mimeType("a.txt") == "text/plain")
assert(Http.mimeType("a.html") == "text/html")

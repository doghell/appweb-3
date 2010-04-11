/*
 *  http10.tst - Test using HTTP 1.0
 */

load("http/support.es")

//  HTTP/1.0
run("--http 0 /index.html")
run("-i 10 --http 0 /index.html")

//  Explicit HTTP/1.1
run("--http 1 /index.html")
run("-i 20 --http 0 /index.html")
run("-i 20 --http 1 /index.html")


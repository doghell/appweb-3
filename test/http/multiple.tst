/*
 *  multiple.tst - Test multiple get commands
 */

load("http/support.es")

//  Multiple requests to test keep-alive
run("-i 300 /index.html")

//  Multiple requests to test keep-alive
run("--chunk 100 -i 300 /index.html")


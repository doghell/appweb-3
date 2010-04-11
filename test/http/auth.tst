/*
 *  auth.tst - Test authentication
 */

load("http/support.es")

//  Auth
run("--user 'joshua:pass1' /basic/basic.html")
run("--user 'joshua' --password 'pass1' /basic/basic.html")


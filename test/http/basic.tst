/*
 *  basic.tst - Basic http tests
 */

load("http/support.es")

assert(sh(env() + command + "/index.html").match("Hello /index.html"))

/*
 *  auth.tst - Authentication http tests
 */

const HTTP = session["main"]
let http: Http = new Http

http.get(HTTP + "/basic/basic.html")
assert(http.code == 401)
http.setCredentials("joshua", "pass1")
http.get(HTTP + "/basic/basic.html")
assert(http.code == 200)

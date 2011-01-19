/*
 *  badUrl.tst - Stress test malformed URLs 
 */

const HTTP = session["main"]
let http: Http = new Http

http.get(HTTP + "/index\x01.html")
assert(http.code == 404)
assert(http.response.contains("Not Found"))

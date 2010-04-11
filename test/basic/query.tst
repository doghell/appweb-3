/*
 *  query.tst - Http query tests
 */

const HTTP = session["main"]
let http: Http = new Http

http.get(HTTP + "/form.ejs?a&b&c")
assert(http.code == 200)
assert(http.response.contains('"a": ""'))
assert(http.response.contains('"b": ""'))
assert(http.response.contains('"c": ""'))

http.get(HTTP + "/form.ejs?a=x&b=y&c=z")
assert(http.code == 200)
assert(http.response.contains('"a": "x"'))
assert(http.response.contains('"b": "y"'))
assert(http.response.contains('"c": "z"'))

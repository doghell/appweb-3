/*
 *  post.tst - Post method tests
 */

const HTTP = session["main"]
const URL = HTTP + "/form.ejs"

let http: Http = new Http

http.post(URL, "Some data")
assert(http.code == 200)

http.form(URL, {name: "John", address: "700 Park Ave"})
assert(http.response.contains('"name": "John"'))
assert(http.response.contains('"address": "700 Park Ave"'))

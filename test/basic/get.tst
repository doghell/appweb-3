/*
 *  get.tst - Http GET tests
 */

const HTTP = session["main"]
const URL = HTTP + "/index.html"
let http: Http = new Http

//  Basic get. Validate response code and contents
http.get(URL)
assert(http.code == 200)
// assert(http.readString().contains("Hello"))

//  Validate get contents
http.get(URL)
assert(http.readString(12) == "<html><head>")
assert(http.readString(7) == "<title>")

//  Validate get contents
http.get(URL)
assert(http.response.endsWith("</html>\n"))
assert(http.response.endsWith("</html>\n"))

//  Test Get with a body. Yes this is valid Http, although unusual.
http.get(URL, {name: "John", address: "700 Park Ave"})
assert(http.code == 200)

if (test.os == "WIN") {
    http.get(HTTP + "/inDEX.htML")
    assert(http.code == 200)
}

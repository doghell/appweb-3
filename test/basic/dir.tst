/*
 *  dir.tst - Directory GET tests
 */

const HTTP = session["main"]
const URL = HTTP + "/index.html"
let http: Http = new Http

/*  DIRECTORY LISTINGS NEEDED 
http.get(HTTP + "/dir/")
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
*/

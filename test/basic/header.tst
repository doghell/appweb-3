/*
 *  header.tst - Http response header tests
 */

const HTTP = session["main"]
const URL = HTTP + "/index.html"
let http: Http = new Http

http.get(URL)
connection = http.header("Connection")
assert(connection == "keep-alive")

http.get(URL)
assert(http.codeString == "OK")
assert(http.contentType == "text/html")
assert(http.date != "")
assert(http.lastModified != "")

http.post(URL)
assert(http.code == 200)

//  Request headers
http.addHeader("key", "value")
http.get(URL)
assert(http.code == 200)

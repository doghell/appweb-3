/*
 *  alias.tst - Alias http tests
 */

const HTTP = session["main"]
let http: Http = new Http

http.get(HTTP + "/aliasDir/index.html")
assert(http.code == 200)
assert(http.response.contains("alias/index.html"))

http.get(HTTP + "/aliasFile/")
assert(http.code == 200)
assert(http.response.contains("alias/index.html"))

http.get(HTTP + "/AliasForMyDocuments/index.html")
assert(http.code == 200)
assert(http.response.contains("My Documents/index.html"))

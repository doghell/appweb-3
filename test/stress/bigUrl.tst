/*
 *  bigUrl.tst - Stress test very long URLs 
 */

const HTTP = session["main"]
const URL = HTTP + "/index.html"
const EURL = HTTP + "/index.ejs"

let http: Http = new Http

/*
 *  Create a very long query
 */
let queryPart = ""
for (i in 100) {
    queryPart += + "key" + i + "=" + 1234567890 + "&"
}

/*
 *  Vary up the query length based on the depth
 */
for (iter in test.depth) {
    let query = ""
    for (i in 5 * (iter + 3)) {
        query += queryPart + "&"
    }
    query = query.trim("&")

    //  Test /index.html
    http.get(URL + "?" + query)
    assert(http.code == 200)
    assert(http.response.contains("Hello /index.html"))

    //  Test /index.ejs
    http.get(EURL + "?" + query)
    assert(http.code == 200)
    assert(http.response.contains("Hello /index.ejs"))
}

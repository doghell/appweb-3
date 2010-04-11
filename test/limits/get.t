/*
 *  get.tst - Extreme GET tests
 */

require ejs.test

module test.http.getmethod {

    const HTTP = session["http"]
    const URL = HTTP + "/index.html"
    const BIG = HTTP + "/big.ejs"

    let http: Http = new Http

print("DEPTH " + test.depth)
    for (iter in test.depth) {
        url = URL + "?"
        for (i in 2000 * (iter + 1)) {
            url += + "key" + i + "=" + Date().now() + "&"
        }
        url = url.trim("&")
        http = new Http
    print(url.length)
        http.get(url)
        print(http.code)
        print(http.response)
        assert(http.code == 200)
        assert(http.response.contains("Hello"))
break
    }
}

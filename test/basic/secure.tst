/*
 *  secure.tst - SSL http tests
 */

if (test.config["ssl"] == 1) {
    const URL = session["main"] + "/index.html"
    const URLS = session["ssl"] + "/index.html"

    let http: Http = new Http

    http.get(URL)
    assert(!http.isSecure)
    http.close()

    http.get(URLS)
    assert(http.isSecure)
    http.close()

    http.get(URLS)
    assert(http.readString(12) == "<html><head>")
    assert(http.readString(7) == "<title>")
    http.close()

    //  Validate get contents
    http.get(URLS)
    assert(http.response.endsWith("</html>\n"))
    assert(http.response.endsWith("</html>\n"))
    http.close()

    http.post(URLS, "Some data")
    assert(http.code == 200)
    http.close()

} else {
    test.skip("SSL not enabled")
}

/*
 *  redirect.tst - Redirection tests
 */

const HTTP = session["main"]
let http: Http = new Http

//  First just test a normal get
http.get(HTTP + "/dir/index.html")
assert(http.code == 200)

http.followRedirects = false
http.get(HTTP + "/dir")
assert(http.code == 301)

http.followRedirects = true
http.get(HTTP + "/dir")
assert(http.code == 200)

http.followRedirects = true
http.get(HTTP + "/dir/")
assert(http.code == 200)
assert(http.response.contains("Hello /dir/index.html"))

/*
 *  gzip.tst - Compressed content
 */

const HTTP = session["main"]
const URL = HTTP + "/compressed.txt"
let http: Http = new Http

http.addHeader("Accept-Encoding", "gzip")
http.get(URL)
assert(http.code == 200)

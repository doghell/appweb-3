/*
 *  chunk.tst - Test chunked transfer encoding for response data
 */

let HTTP = session["main"] || ":4100"
let http: Http = new Http

/*
http.chunked = true
http.post(HTTP + "/index.html")
http.wait()
// print("CODE " + http.code)
assert(http.code == 200)
*/

http.chunked = false
http.post(HTTP + "/index.html")
http.wait()
// print("CODE " + http.code)
assert(http.code == 200)

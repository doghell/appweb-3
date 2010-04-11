/*
 *  put.tst - Put method tests
 */

const HTTP = session["main"]
let http: Http = new Http

/*
//  Test http.put with content data
data = Path("basic/test.dat").readString()
http.put(HTTP + "/tmp/test.dat", data)
assert(http.code == 201 || http.code == 204)
*/

//  This request should hang because we don't write any data. Wait with a timeout
http.contentLength = 1000
http.put(HTTP + "/a.tmp")
assert(http.wait(250) == false)

path = Path("basic/test.dat")
http.contentLength = path.size
http.put(HTTP + "/test.dat")
file = File(path).open()
buf = new ByteArray
while (file.read(buf)) {
    http.write(buf)
    buf.flush()
}
http.write()
http.wait()
assert(http.code == 204)

/*
 *  methods.tst - Test misc Http methods
 */

const HTTP = session["main"]
const URL = HTTP + "/index.html"
let http: Http = new Http

//  Put a file
data = Path("basic/test.dat").readString()
http.put(HTTP + "/tmp/test.dat", data)
assert(http.code == 201 || http.code == 204)

//  Delete
http.del(HTTP + "/tmp/test.dat")
assert(http.code == 204)

//  Post
http.post(URL, "Some data")
assert(http.code == 200)

//  Options
http.options(URL)
assert(http.header("Allow") == "OPTIONS,TRACE,GET,HEAD,POST,PUT,DELETE")

//  Trace - should be disabled
http.trace(URL)
assert(http.code == 406)

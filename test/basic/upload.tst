/*
 *  upload.tst - File upload tests
 */

const HTTP = session["main"]
let http: Http = new Http

http.upload(HTTP + "/upload.ejs", { myfile: "basic/test.dat"} )
assert(http.code == 200)
assert(http.response.contains('"clientFilename": "test.dat"'))
assert(http.response.contains('Uploaded'))
http.wait()

http.upload(HTTP + "/upload.ejs", { myfile: "basic/test.dat"}, {name: "John Smith", address: "100 Mayfair"} )
assert(http.code == 200)
assert(http.response.contains('"clientFilename": "test.dat"'))
assert(http.response.contains('Uploaded'))
assert(http.response.contains('"address": "100 Mayfair"'))

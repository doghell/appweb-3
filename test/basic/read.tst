/*
 *  read.tst - Various Http read tests
 */

const HTTP = session["main"]
let http: Http = new Http

//  Validate readXml()
http.get(HTTP + "/test.xml")
assert(http.readXml().customer.name == "Joe Green")

//  Test http.read() into a byte array
http.get(HTTP + "/big.ejs")
buf = new ByteArray
count = 0
while (http.read(buf) > 0) {
    count += buf.available
}
if (count != 63201) {
    print("COUNT IS " + count + " code " + http.code)
}
assert(count == 63201)

http.get(HTTP + "/lines.txt")
lines = http.readLines()
for (l in lines) {
    line = lines[l]
    assert(line.contains("LINE") && line.contains(l.toString()))
}
assert(http.code == 200)

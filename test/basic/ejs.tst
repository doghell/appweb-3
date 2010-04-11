/*
 *  ejs.tst - EJS basic tests
 */

const HTTP = session["main"]
let http: Http = new Http

/* Suport routines */

function contains(pat): Void {
    assert(http.response.contains(pat))
}

function keyword(pat: String): String {
    pat.replace(/\//, "\\/").replace(/\[/, "\\[")
    let reg = RegExp('.*"' + pat + '": "*([^",]*).*', 's')
    return http.response.replace(reg, "$1")
}

function match(key: String, value: String): Void {
    assert(keyword(key) == value)
}


/* Tests */

function basic() {
    http.get(HTTP + "/ejsProgram.ejs")
    assert(http.code == 200)
    data = deserialize(http.response)
    assert(data.response)
}

function forms() {
    http.get(HTTP + "/ejsProgram.ejs")
    assert(http.code == 200)
    data = deserialize(http.response)
    assert(data.response)

    if (false && test.os == "WIN") {
        http.get(HTTP + "/ejsProgRAM.eJS")
        assert(http.code == 200)
    }
}

function alias() {
    http.get(HTTP + "/SimpleAlias/ejsProgram.ejs")
    assert(http.code == 200)
    request = deserialize(http.response).request
    assert(request.url == "/SimpleAlias/ejsProgram.ejs")
    assert(request.query == null)
}

function query() {
    http.get(HTTP + "/ejsProgram.ejs?a=b&c=d&e=f")
    data = deserialize(http.response)
    request = data.request
    params = data.params
    assert(request.url, "/ejsProgram.ejs")
    assert(params.a == "b")
    assert(params.c == "d")

    //
    //  Query string vars should not be turned into variables for GETs
    //
    http.get(HTTP + "/ejsProgram.ejs?var1=a+a&var2=b%20b&var3=c")
    data = deserialize(http.response)
    request = data.request
    params = data.params
    assert(request.url == "/ejsProgram.ejs")
    assert(request.query == "var1=a+a&var2=b%20b&var3=c")
    assert(params.var1 == "a a")
    assert(params.var2 == "b b")
    assert(params.var3 == "c")

    //
    //  Post data should be turned into variables
    //
    http.form(HTTP + "/ejsProgram.ejs?var1=a+a&var2=b%20b&var3=c", 
        { name: "Peter", address: "777+Mulberry+Lane" })
    data = deserialize(http.response)
    request = data.request
    params = data.params
    assert(request.query == "var1=a+a&var2=b%20b&var3=c")
    assert(params.var1 == "a a")
    assert(params.var2 == "b b")
    assert(params.var3 == "c")
    assert(params.name == "Peter")
    assert(params.address == "777 Mulberry Lane")
}

function encoding() {
    http.get(HTTP + "/ejsProgram.ejs?var%201=value%201")
    data = deserialize(http.response)
    request = data.request
    params = data.params
    assert(request.query == "var%201=value%201")
    assert(request.url == "/ejsProgram.ejs")
    assert(params["var 1"] == "value 1")
}

function status() {
    http.get(HTTP + "/ejsProgram.ejs?code=711")
    assert(http.code == 711)
}

function location() {
    let http = new Http
    http.followRedirects = false
    http.get(HTTP + "/ejsProgram.ejs?redirect=http://www.redhat.com/")
    assert(http.code == 302)
    http.close()
}

function quoting() {
    http.get(HTTP + "/ejsProgram.ejs?a+b+c")
    data = deserialize(http.response)
    request = data.request
    params = data.params
    assert(request.query == "a+b+c")
    assert(params["a b c"] == "")

    http.get(HTTP + "/ejsProgram.ejs?a=1&b=2&c=3")
    data = deserialize(http.response)
    request = data.request
    params = data.params
    assert(request.query == "a=1&b=2&c=3")
    assert(params.a == "1")
    assert(params.b == "2")
    assert(params.c == "3")

    http.get(HTTP + "/ejsProgram.ejs?a%20a=1%201+b%20b=2%202")
    data = deserialize(http.response)
    request = data.request
    params = data.params
    assert(request.query == "a%20a=1%201+b%20b=2%202")
    assert(params["a a"] == "1 1 b b=2 2")

    http.get(HTTP + "/ejsProgram.ejs?a%20a=1%201&b%20b=2%202")
    data = deserialize(http.response)
    request = data.request
    params = data.params
    assert(request.query, "a%20a=1%201&b%20b=2%202")
    assert(params["a a"] == "1 1")
    assert(params["b b"] == "2 2")

    http.get(HTTP + "/ejsProgram.ejs?a|b+c>d+e?f+g>h+i'j+k\"l+m%20n=1234")
    data = deserialize(http.response)
    request = data.request
    params = data.params
    assert(params["a|b c>d e?f g>h i'j k\"l m n"] == 1234)
    assert(request.query == "a|b+c>d+e?f+g>h+i'j+k\"l+m%20n=1234")
}

basic()
forms()
alias()
query()
encoding()
status()
location()
quoting()

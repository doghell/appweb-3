/*
 *  vhost.tst - Virtual Host tests
 */

const MAIN = session["main"]
const VHOST = session["vhost"]
const IPHOST = session["iphost"]
let http: Http = new Http

function mainHost() {
    http.get(MAIN + "/main.html")
    http.response.contains("MAIN SERVER")
    assert(http.code == 200)

    //  These should fail
    http.get(MAIN + "/iphost.html")
    assert(http.code == 404)
    http.get(MAIN + "/vhost1.html")
    assert(http.code == 404)
    http.get(MAIN + "/vhost2.html")
    assert(http.code == 404)
}

//
//  The first virtual host listens to "localhost", the second listens to "127.0.0.1". Both on the same port.
//
function namedHost() {
    let port = session["vhostPort"]
    let vhost = "http://localhost:" + port
    http.get(vhost + "/vhost1.html")
    assert(http.code == 200)

    //  These should fail
    http.get(vhost + "/main.html")
    assert(http.code == 404)
    http.get(vhost + "/iphost.html")
    assert(http.code == 404)
    http.get(vhost + "/vhost2.html")
    assert(http.code == 404)

    //  Now try the second vhost on 127.0.0.1
    vhost = "http://127.0.0.1:" + port
    http.get(vhost + "/vhost2.html")
    assert(http.code == 200)

    //  These should fail
    http.get(vhost + "/main.html")
    assert(http.code == 404)
    http.get(vhost + "/iphost.html")
    assert(http.code == 404)
    http.get(vhost + "/vhost1.html")
    assert(http.code == 404)
}

function ipHost() {
    let http: Http = new Http
    http.setCredentials("mary", "pass2")
    http.get(IPHOST + "/private.html")
    assert(http.code == 200)
}

mainHost()
namedHost()
ipHost()

/*
 *  http.tst - Test the http command
 */

if (test.config["http_client"] == 1 && session["main"] && test.depth > 1) {

    let command = "/usr/bin/env http --host " + session["main"] + " "
    if (Config.OS == "WIN") {
        command = "/bin/env http --host " + session["main"] + " "
    }
    if (test.verbosity > 2) {
        command += "-v "
    }

    function run(args): String {
        test.log(2, "[TestRun]", command + args)
        try {
            result = Cmd.run(command + args)
            assert(true)
        } catch (e) {
            assert(false, e)
        }
        return result
    }

    //  Empty get
    data = run("/empty.html")
    assert(data == "")

    //  Basic get
    data = run("/numbers.txt")
    assert(data.startsWith("012345678"))
    assert(data.trim().endsWith("END"))

    //  Chunked get
    data = run("--chunk 256 /big.txt")
    assert(data.startsWith("012345678"))
    assert(data.trim().endsWith("END"))

    //  Chunked empty get
    data = run("--chunk 100 /empty.html")
    assert(data == "")

    //  Multiple requests to test keep-alive
    run("-i 300 /index.html")

    //  Multiple requests to test keep-alive
    run("--chunk 100 -i 300 /index.html")

    //  HTTP/1.0
    run("--http 0 /index.html")
    run("-i 10 --http 0 /index.html")

    //  Explicit HTTP/1.1
    run("--http 1 /index.html")
    run("-i 20 --http 0 /index.html")
    run("-i 20 --http 1 /index.html")

    //  Auth
    run("--user joshua:pass1 /basic/basic.html")
    run("--user joshua --password pass1 /basic/basic.html")

    //  Form data
    data = run("--form 'name=John+Smith&address=300+Park+Avenue' /form.ejs")
    assert(data.contains('"address": "300 Park Avenue"'))
    assert(data.contains('"name": "John Smith"'))

    //  PUT file
    run("cmd/test.dat /tmp/day.tmp")
    if (test.threads == 1) {
        assert(exists("web/tmp/day.tmp"))
    }

    files = Path("basic").files().join(" ")
    run(files + " /tmp/")
    if (test.threads == 1) {
        assert(exists("web/tmp/methods.tst"))
    }
    //  DELETE
    run("cmd/test.dat /tmp/test.dat")
    assert(exists("web/tmp/test.dat"))
    run("--method DELETE /tmp/test.dat")
    assert(!exists("web/tmp/test.dat"))

    //  Options/Trace with showCode
    run("--method OPTIONS /index.html")
    data = run("--showCode -q --method TRACE /index.html")
    assert(data.trim() == "406")

    //  Show headers
    data = run("--showHeaders /index.html")
    assert(data.contains('Content-Type'))

    //  Upload
    files = Path("basic").files().join(" ")
    data = run("--upload " + files + " /upload.ejs")
    assert(data.contains('"clientFilename": "methods.tst"'))
    if (test.threads == 1) {
        assert(exists("web/tmp/methods.tst"))
    }

    files = Path("basic").files().join(" ")
    data = run("--upload --form 'name=John+Smith&address=300+Park+Avenue' " + files + " /upload.ejs")
    assert(data.contains('"address": "300 Park Avenue"'))
    assert(data.contains('"clientFilename": "methods.tst"'))

    data = run("--cookie 'test-id=12341234; $domain=site.com; $path=/dir/' /form.ejs")
    assert(data.contains('"test-id": '))
    assert(data.contains('"name": "test-id"'))
    assert(data.contains('"domain": "site.com"'))
    assert(data.contains('"path": "/dir/"'))

    //  Ranges
    assert(run("--range 0-4 /numbers.html").trim() == "01234")
    assert(run("--range -5 /numbers.html").trim() == "5678")

    //  Load test
    if (test.depth > 2) {
        run("-i 2000 /index.html")
        run("-i 2000 /big.txt")
    }

    //  Cleanup
    for each (f in Path("web/tmp").files()) {
        Path(f).remove()
    }

} else {
    test.skip("Test runs at depth 2 or higher with Http client enabled")
}

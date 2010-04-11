/*
 *  01000-chunk.tst - This tests uploading 3 files using chunked encoding as well.
 *  Was failing due to the trailing "\r\n" in the upload content
 */

if (test.config["http_client"] == 1 && session["main"] && test.depth > 0 && 
        (test.os == "MACOSX" || test.os == "LINUX")) {

    let nc
    try { nc = sh("which nc"); } catch {}
    if (nc) {
        sh("cc -o regress/tcp regress/tcp.c")
        testCmd("cat regress/01000-chunk.dat | nc 127.0.0.1 " + session["port"])
        testCmd("regress/tcp 127.0.0.1 " + session["port"] + " regress/01000-chunk.dat")
    } else {
        test.skip("nc not installed")
    }

} else {
    test.skip("Http client not enabled")
}

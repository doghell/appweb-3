/*
 *  php.tst - PHP tests
 */

if (test.config["php"] == 1) {
    const HTTP = session["main"]
    let http: Http = new Http

    //  Simple Get 
    http.get(HTTP + "/index.php")
    assert(http.code == 200)
    assert(http.response.contains("Hello PHP World"))

    //  Get 
    http.get(HTTP + "/form.php")
    assert(http.code == 200)
    assert(http.response.contains("form.php"))

    //  Form
    http.form(HTTP + "/form.php?a=b&c=d", { name: "John Smith", address: "777 Mulberry Lane" })
    assert(http.code == 200)
    assert(http.response.contains("name is John Smith"))
    assert(http.response.contains("address is 777 Mulberry Lane"))

    //  Big output
    http.get(HTTP + "/big.php")
    assert(http.code == 200)
    data = new ByteArray
    while (http.read(data)) {
        assert(data.toString().contains("aaaabbbb"))
    }

} else {
    test.skip("PHP not enabled")
}

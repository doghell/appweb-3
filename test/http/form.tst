/*
 *  form.tst - Test forms
 */

load("http/support.es")

//  Form data
data = run("--form 'name=John+Smith&address=300+Park+Avenue' /form.ejs")
params = deserialize(data).params
assert(params.address == "300 Park Avenue")
assert(params.name == "John Smith")

//  Form data with a cookie
data = run("--cookie 'test-id=12341234; $domain=site.com; $path=/dir/' /form.ejs")
request = deserialize(data).request
cookie = request.cookies["test-id"]
assert(cookie.name == "test-id")
assert(cookie.domain == "site.com")
assert(cookie.path == "/dir/")

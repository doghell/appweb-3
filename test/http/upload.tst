/*
 *  upload.tst - Test http upload
 */

load("http/support.es")

//  Upload files
data = run("--upload http/*.tst /upload.ejs")
assert(data.contains('"clientFilename": "basic.tst"'))
assert(data.contains('"clientFilename": "methods.tst"'))

//  Upload with form
data = run("--upload --form 'name=John+Smith&address=300+Park+Avenue' http/*.tst /upload.ejs")
assert(data.contains('"address": "300 Park Avenue"'))
assert(data.contains('"clientFilename": "basic.tst"'))

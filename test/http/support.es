/*
    Support functions for the Http unit tests
 */

use default namespace public

var command = env() + locate("http") + " --host " + session["http"] + " "
if (test.verbose > 2) {
    command += "-v "
}

function run(args): String {
    if (test.verbose > 1) {
        test.logTag("[TestRun]", command + args)
    }
    // print(command + args)
    let result = sh(command + args)
    return result.trim()
}

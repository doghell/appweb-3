#!/bin/sh
set -x
export PATH=${BUILT_PRODUCTS_DIR}:$PATH
export DYLD_LIBRARY_PATH=${BUILT_PRODUCTS_DIR}:$DYLD_LIBRARY_PATH

cd $PROJECT_DIR/../../../src/ejs

FILES="ejs.es"
TARGET=$BUILT_PRODUCTS_DIR/ajs.mod

ajsc  --debug --optimize 9 --bind --empty --out $TARGET $FILES
ajsmod --showDebug --empty --showBuiltin --listing --out ../include/ejs.slots.h $TARGET

set +x
exit 0

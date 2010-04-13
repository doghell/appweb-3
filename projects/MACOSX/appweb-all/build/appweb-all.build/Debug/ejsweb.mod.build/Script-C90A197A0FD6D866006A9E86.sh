#!/bin/sh
set -x
export PATH=${BUILT_PRODUCTS_DIR}:$PATH
export DYLD_LIBRARY_PATH=${BUILT_PRODUCTS_DIR}:$DYLD_LIBRARY_PATH

cd $PROJECT_DIR/../../../src/ejs

FILES="ejsweb.es"
TARGET=$BUILT_PRODUCTS_DIR/ajsweb.mod

ajsc --debug --optimize 9 --bind --out $TARGET $FILES
mkdir -p ../../lib/default-web
chmod -R +w ../../lib/default-web
cp -r default-web/* ../../lib/default-web
chmod -R +w ../../lib/default-web
cp appweb.conf mime.types ../../lib

set +x
exit 0


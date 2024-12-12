#!/bin/bash

set -e

DIR="linux-build"

if [ ! -d $DIR ]; then
    mkdir $DIR
    cmake -B $DIR $@
fi

cmake --build $DIR -j 8
cp $DIR/compile_commands.json .

ctest --test-dir $DIR --output-on-failure

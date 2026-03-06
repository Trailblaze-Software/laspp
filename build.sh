#!/bin/bash

# SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
# SPDX-License-Identifier: MIT

set -e

DIR="linux-build"

if [ ! -d $DIR ]; then
    mkdir $DIR
    cmake -B $DIR "$@"
fi

cmake --build $DIR -j 8
cp $DIR/compile_commands.json .

ctest --test-dir $DIR --output-on-failure

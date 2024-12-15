#!/bin/bash

# SPDX-FileCopyrightText: (c) 2024 Trailblaze Software, all rights reserved
# SPDX-License-Identifier: LGPL-2.1
#
# This library is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; version 2.1.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License along
# with this library; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-2024 USA
#
# For LGPL2 incompatible licensing or development requests, please contact
# trailblaze.software@gmail.com

set -e

DIR="linux-build"

if [ ! -d $DIR ]; then
    mkdir $DIR
    cmake -B $DIR $@
fi

cmake --build $DIR -j 8
cp $DIR/compile_commands.json .

ctest --test-dir $DIR --output-on-failure

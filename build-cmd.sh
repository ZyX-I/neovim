#!/bin/sh

clang -DCOMPILE_TEST_VERSION -ldl -o src/cmd src/nvim/viml/testhelpers/progs/cmd-main.c

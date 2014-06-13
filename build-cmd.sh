#!/bin/sh

clang -DCOMPILE_TEST_VERSION -ldl -o src/cmd src/viml/parser/cmd-main.c

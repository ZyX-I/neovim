#!/bin/sh

clang -DCOMPILE_TEST_VERSION -ldl -o src/cmd src/translator/parser/cmd-main.c

#!/bin/sh

clang -Isrc -DCOMPILE_TEST_VERSION -ldl -o src/exe src/nvim/viml/testhelpers/progs/exe-main.c

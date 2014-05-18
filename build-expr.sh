if test -d expr ; then
  rm -rf expr
fi
mkdir -p expr
cd expr

CFLAGS="-I../src -DCOMPILE_TEST_VERSION -g -c"
KLEE_CFLAGS="${CFLAGS} -emit-llvm -I$HOME/tmp/image/klee/include -DCOMPILE_KLEE"

LINK_FLAGS="-o expr.lo"

for src in ../src/nvim/garray.c ../src/nvim/translator/{parser,printer}/expr*.c ../src/nvim/translator/testhelpers/{parser,fgetline}.c ; do
  out=$(basename $src)
  while test -e "${out}.lo" ; do
    out="${out}.2"
  done
  out="${out}.lo"
  clang ${KLEE_CFLAGS} -o "${out}" $src
  LINK_FLAGS="${LINK_FLAGS} ${out}"
done

llvm-link ${LINK_FLAGS}

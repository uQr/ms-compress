FLAGS="-DMSCOMP_API_EXPORT -DMSCOMP_WITHOUT_LZX -O3 -fno-tree-vectorize -march=core2 -Wall -s"
FILES="src/*.cpp"
OUT="MSCompression"

echo Compiling dynamic shared library...
g++ ${FLAGS} -DMSCOMP_API_DLL -fPIC -shared ${FILES} -o lib${OUT}.so

echo Compiling static shared library...
g++ ${FLAGS} -c ${FILES}
ar -rcs lib${OUT}.a *.o
rm -f *.o
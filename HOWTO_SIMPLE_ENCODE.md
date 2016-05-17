```bash
git clone -b simple-encode https://github.com/wtakase/libgibraltar.git
cd libgibraltar
. gibraltar_setenv.sh 
make clean && make cuda=1
cd examples/simple_encode/
./simple_encode 10 2 infile_1mb out
```

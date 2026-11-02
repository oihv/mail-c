# gcc -g src/test.c -o build/test
rm *.log
rm .log
rm server.log.*
rm client.log

flags="-g -I./src -lsqlite3 -Wall -Werror -Wno-unused-function -lssl -lcrypto"
# gcc $flags src/server.c -o build/server
gcc $flags src/client.c -o build/client
gcc $flags src/test.c -o build/test
# gcc -g -I./src src/base/test.c -o build/base/test

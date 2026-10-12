# gcc -g src/test.c -o build/test
rm *.log
rm server.log.*
rm client.log
gcc -g -I./src src/server.c -o build/server
gcc -g -I./src src/client.c -o build/client
gcc -g -I./src src/test.c -o build/test
gcc -g -I./src src/base/test.c -o build/base/test

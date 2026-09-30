# gcc -g src/test.c -o build/test
gcc -g -I./src src/server.c -o build/server
gcc -g -I./src src/client.c -o build/client
gcc -g -I./src src/test.c -o build/test

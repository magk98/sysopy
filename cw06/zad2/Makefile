all:
	
	gcc -c -Wall ipc_util.c
	ar crs ipc_util.a ipc_util.o
	gcc -c -Wall server_posix.c
	gcc server_posix.o ipc_util.a -o server -lrt
	gcc -c -Wall client_posix.c
	gcc client_posix.o ipc_util.a -o client -lrt
    

clean:
	rm -r *.o
	rm -r *.a
	rm -r client server
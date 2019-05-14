all:
	
	gcc -c -Wall ipc_util.c
	ar crs ipc_util.a ipc_util.o
	gcc -c -Wall server_sysv.c
	gcc server_sysv.o ipc_util.a -o server
	gcc -c -Wall client_sysv.c
	gcc client_sysv.o ipc_util.a -o client
    

clean:
	rm -r *.o
	rm -r *.a
	rm -r client server
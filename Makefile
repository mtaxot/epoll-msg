all:
	@gcc -Wall -o server2 server2.c message.c connection.c packet.c threads.c -lpthread
#	@gcc -Wall -o client client.c message.c packet.c
	@gcc -Wall -o client2 client2.c message.c connection.c packet.c threads.c -lpthread
clean:
	@rm -rf server server2 client client2

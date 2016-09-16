#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <time.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define LISTEN_QUEUE 20

#define BUFLEN 1024

#define MAXITERATION 2
  
/*
	usage:
			./server serverPort
*/
int main(int argc, char* argv[])
{
	int ret, val, status, i;
	
	int listen_sock, cl_sock;
	struct sockaddr_in my_addr, cl_addr;
	socklen_t cl_addr_len = sizeof(cl_addr);
	
	int pid;
	
	char buffer[BUFLEN];
	char str_addr[INET_ADDRSTRLEN];
	
	/*checking syntax & parameters*/
	if(argc != 2)
	{
		printf("\n\nSyntax Error: incorrect number of arguments!\n\n");
		exit(1);	
	}
	
	printf("\n\nStarting Server on port %d\n\n", atoi(argv[1]));
	
	/*creating listening socket*/
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock < 0)
	{
		printf("\n\nSocket Error: creating listen socket!\n\n");
		exit(1);
	}
	
	bzero(&my_addr, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(atoi(argv[1]));
	my_addr.sin_addr.s_addr = INADDR_ANY;
	
	/*binding socket and server address*/
	ret = bind(listen_sock, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if(ret < 0)
	{
		printf("\n\nBind Error: binding listen socket!\n\n");
		exit(1);
	}
	
	/*listening connection requestes*/
	ret = listen(listen_sock, LISTEN_QUEUE);
	if(ret < 0)
	{
		printf("\n\nListen Error: listening on listen socket!\n\n");
		exit(1);
	}
	
	/*editing socket options: REUSING IP ADDRESSES and PORTS*/
	val=1;
	#ifdef SO_REUSEADDR
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	#endif
	#ifdef SO_REUSEPORT
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
	#endif
	
	int avl_connections = 0;
	/*loop*/
	while(1)
	{
		printf("\n\nWaiting for a request ...\n");
		
		/*accept a new request*/
		cl_sock = accept(listen_sock, (struct sockaddr *)&cl_addr, &cl_addr_len);
		
		/*forking server*/
		pid = fork();
		if(pid == 0)	/*child*/
		{
			close(listen_sock);
			
			/*getting client IP address*/
			if(inet_ntop(AF_INET, &cl_addr.sin_addr, str_addr, INET_ADDRSTRLEN) == NULL)
			{
				printf("\n\nInet_ntop Error: converting client address to string!\n\n");
				exit(1);
			}
			
			ret = recv(cl_sock, buffer, BUFLEN, 0);
			if(ret == 0)
			{
				printf("\n\nRecv Error: receiving data on client socket!\n\n");
				exit(1);
			}

			printf("\nRECEIVED:\n\t%s\nFROM:\n\t%s:%d\n", buffer, str_addr, cl_addr.sin_port);

//sleep(6);

			ret = send(cl_sock, buffer, BUFLEN, 0);
			if(ret < 0)
			{
				printf("\n\nSend Error: receiving data on client socket!\n\n");
				exit(1);
			}

			printf("\nSENT:\n\t%s\nTO:\n\t%s:%d\n", buffer, str_addr, cl_addr.sin_port);
			
			close(cl_sock);
			exit(0);
		}
		else			/*father*/
		{
			close(cl_sock);
			
			for(i=0; i<MAXITERATION; i++)	/*loop to terminate more than 1 zombie-child*/
			{
				pid = wait(&status);
				if(pid == -1)
				{
					printf("\n\nWait Error: %s\n\n", strerror(errno));
					//exit(1);
				}
				if(pid == 0)
					break;
			}
		}
	}
	return 0;
}


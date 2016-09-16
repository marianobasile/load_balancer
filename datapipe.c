/*
 * Datapipe - Create a listen socket to pipe connections to another
 * machine/port. 'localport' accepts connections on the machine running    
 * datapipe, which will connect to 'remoteport' on 'remotehost'.
 * It will fork itself into the background on non-Windows machines.
 *
 * This implementation of the traditional "datapipe" does not depend on
 * forking to handle multiple simultaneous clients, and instead is able
 * to do all processing from within a single process, making it ideal
 * for low-memory environments.  The elimination of the fork also
 * allows it to be used in environments without fork, such as Win32.
 *
 * This implementation also differs from most others in that it allows
 * the specific IP address of the interface to listen on to be specified.
 * This is useful for machines that have multiple IP addresses.  The
 * specified listening address will also be used for making the outgoing
 * connections on.
 *
 * Note that select() is not used to perform writability testing on the
 * outgoing sockets, so conceivably other connections might have delayed
 * responses if any of the connected clients or the connection to the
 * target machine is slow enough to allow its outgoing buffer to fill
 * to capacity.
 *
 * Compile with:
 *     cc -O -o datapipe datapipe.c
 * On Solaris/SunOS, compile with:
 *     gcc -Wall datapipe.c -lsocket -lnsl -o datapipe
 * On Windows compile with:
 *     bcc32 /w datapipe.c                (Borland C++)
 *     cl /W3 datapipe.c wsock32.lib      (Microsoft Visual C++)
 *
 * Run as:
 *   datapipe localhost localport remotehost remoteport
 *
 *
 * written by Jeff Lawson <jlawson@bovine.net>
 * inspired by code originally by Todd Vierling, 1995.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <winsock.h>
  #define bzero(p, l) memset(p, 0, l)
  #define bcopy(s, t, l) memmove(t, s, l)
#else
  #include <sys/time.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/wait.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <netdb.h>
  #include <strings.h>
 //====================================
 #include <inttypes.h>
 //=====================================
  #define recv(x,y,z,a) read(x,y,z)
  #define send(x,y,z,a) write(x,y,z)
  #define closesocket(s) close(s)
  typedef int SOCKET;
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif


struct client_t
{
	int inuse;
	SOCKET csock, osock;
	time_t activity;
	/*to handle the servers connection counter*/
	uint8_t server_index;
	/*to handle the communication verse: 0 --> from client to server; 1 --> from server to client*/
	uint8_t direction;
};

#define MAXCLIENTS 20
#define IDLETIMEOUT 30


const char ident[] = "$Id: datapipe.c,v 1.8 1999/01/29 01:21:54 jlawson Exp $";

 //============================================================================================

struct server {
	struct sockaddr_in addr;     //ip_addr & port 
	uint64_t connection;        //# of active connections
};


int validate_server_address(char *address) {

	int count = 0;
	char * addressPart = strtok(address,".");
	
	if (addressPart == NULL)
		return -1;
	
	while(addressPart != NULL) {
		count++;
		if( (atoi(addressPart) < 0) || (atoi(addressPart) > 255) ) 
			return -1;
		addressPart = strtok(NULL,".");
	}
	if(count != 4)
		return -1;
	return 0;
}

int validate_server_port(char *port) 
{
	uint64_t dot_position = strcspn(port,".");
	
	if (dot_position != strlen(port))
		return -1;
		
	if( atoi(port) >= 65536) 
	{
		errno = EINVAL;
		return -1;
	}	
	return 0;
}

void init_server_sockaddr(struct server s[],int n_server,char *argv[]) 
{

	int i;
	int index = 3;	/*starting from the first server*/
	
	for(i=0; i<n_server;i++)
	{
		bzero(&s[i].addr, sizeof(struct sockaddr_in));
		s[i].addr.sin_family = AF_INET;
		s[i].addr.sin_addr.s_addr = inet_addr(argv[index++]);
		s[i].addr.sin_port = htons( (unsigned short) atol(argv[index++]) );
		s[i].connection = 0;
	}
}

/*forwarding policy: RR +  less busy server*/
uint8_t get_server_index(struct server s[], int rr_counter, int n_server) 
{
	int index = rr_counter;
	int i, current = rr_counter;
	
	for(i=1; i<n_server; i++)
	{
		if(s[index].connection == 0)
			break;
	
		current = (current+i)%n_server;
	
		if(s[index].connection > s[current].connection)
			index = current;
	}
	return index;
}

void help() 
{ 
	printf("\nCorrect Usage:\n");
	printf("./datapipe LOCAL_ADDR LOCAL_PORT SERVER_ADDR1 SERVER_PORT1 SERVER_ADDRN SERVER_PORTN\n\n");
}

int check_args(char *argv[],int argc) 
{
	int i;
	int avl_server=0;
	char serv_addr[INET_ADDRSTRLEN];
	
	for(i=1; i<argc;i=i+2) 
	{
		strcpy(serv_addr,argv[i]);          //since strtok modifies argv[]
		if(argc <= i || validate_server_address(serv_addr) == -1 )
		{ 
			errno = EFAULT;
			printf("Error during address validation: %s\n\n", strerror( errno ) );
			printf("./lz78: invalid syntax\n");
			help();
			exit(EXIT_FAILURE);
		}
		else if(argc <= (i+1) ||  validate_server_port(argv[i+1]) == -1)
		{ 
			errno = EINVAL;
			printf("Error during port validation: %s\n\n", strerror( errno ) );  
			printf("./lz78: invalid syntax\n");
			help();
			exit(EXIT_FAILURE);
		}
		else
			avl_server++;
	}
	
	if(avl_server == 0)
		return -1;

	return avl_server-1;		/*returning the number of server (-1 because balancer has to be not considered!)*/
}

void print_avl_server(struct server s[],int n_server)
{
	int i;
	char str[INET_ADDRSTRLEN];
	
	for(i=0; i<n_server;i++)
	{
		printf("\nSERVER: %d",i);
		
		inet_ntop(AF_INET, &(s[i].addr.sin_addr), str, INET_ADDRSTRLEN);
		printf("\tIP_ADDRESS: %s",str);
		
		printf("\tPORT: ");
		printf("%hu",ntohs(s[i].addr.sin_port));
		
		printf("\tCONNECTIONS: "); 
		printf("%"PRIu64,s[i].connection); 
	}
	printf("\n\n");
}
//============================================================================================



int main(int argc, char *argv[])
{ 
	SOCKET lsock;
	char buf[1024];
	struct sockaddr_in laddr;
	
	int ret, nbyt, closeneeded;

  ret= nbyt = 0;
	
	//============================================================================================
	int N_SERVER;         		//# of available servers
	uint8_t index = 0;        //index for RR scheduling
	uint8_t sv_index = 0;			//index for forwarding policy (RR + less busy server)
	//============================================================================================
	int i;
	struct client_t clients[MAXCLIENTS];
	
	
	#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32)
		/* Winsock needs additional startup activities */
		WSADATA wsadata;
		WSAStartup(MAKEWORD(1,1), &wsadata);
	#endif

//============================================================================================

	/*Args check */
	
	if((N_SERVER = check_args(argv,argc)) == -1)
	{
		printf("./lz78: invalid syntax\n");
		help();
		exit(EXIT_FAILURE);
	}
	
	struct server s[N_SERVER];    //available servers

//============================================================================================

	/* reset all of the client structures */
	for (i = 0; i < MAXCLIENTS; i++)
	{
		clients[i].inuse = 0;
		clients[i].direction = 0;			//client starts the communication
	}
	
	/* determine the listener address and port */
	bzero(&laddr, sizeof(struct sockaddr_in));
	laddr.sin_family = AF_INET;
	laddr.sin_port = htons((unsigned short) atol(argv[2]));
	laddr.sin_addr.s_addr = inet_addr(argv[1]);
	if (!laddr.sin_port) {
		fprintf(stderr, "invalid listener port\n");
		return 20;
	}
	if (laddr.sin_addr.s_addr == INADDR_NONE) {
		struct hostent *n;
		if ((n = gethostbyname(argv[1])) == NULL) {
			perror("gethostbyname");
			return 20;
		}    
		bcopy(n->h_addr, (char *) &laddr.sin_addr, n->h_length);
	}

//============================================================================================
	
	init_server_sockaddr(s,N_SERVER,argv);
	
	print_avl_server(s,N_SERVER);

//============================================================================================

	/* create the listener socket */

	if ((lsock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return 20;
	}
	if (bind(lsock, (struct sockaddr *)&laddr, sizeof(laddr))) {
		perror("bind");
		return 20;
	}
	if (listen(lsock, 5)) {
		perror("listen");
		return 20;
	}
	
	/* change the port in the listener struct to zero, since we will
	* use it for binding to outgoing local sockets in the future. */
	
	laddr.sin_port = htons(0);
	
	/* fork off into the background. */
	/*
	#if !defined(__WIN32__) && !defined(WIN32) && !defined(_WIN32)
		if ((i = fork()) == -1) {
			perror("fork");
			return 20;
		}
		if (i > 0)
			return 0;
		setsid();
	#endif
	*/
	
	/* main polling loop. */

	while (1)
	{
		fd_set fdsr;
		int maxsock;
		time_t now = time(NULL);
		
		/* build the list of sockets to check. */
		
		FD_ZERO(&fdsr);
		FD_SET(lsock, &fdsr);
		maxsock = (int) lsock;
		
		for (i = 0; i < MAXCLIENTS; i++)
			if (clients[i].inuse)
			{
				FD_SET(clients[i].csock, &fdsr);
				if ((int) clients[i].csock > maxsock)
					maxsock = (int) clients[i].csock;
				FD_SET(clients[i].osock, &fdsr);
				if ((int) clients[i].osock > maxsock)
					maxsock = (int) clients[i].osock;
			}
			    
		if (select(maxsock + 1, &fdsr, NULL, NULL, NULL) < 0)
			return 30;

		/* check if there are new connections to accept. */
		if (FD_ISSET(lsock, &fdsr))
		{
			SOCKET csock = accept(lsock, NULL, 0);
			
			for (i = 0; i < MAXCLIENTS; i++)
				if (!clients[i].inuse) break;
		
			if (i < MAXCLIENTS)
			{
				/* connect a socket to the outgoing host/port */
				
				/*getting the server index where to forward the request*/
				sv_index = get_server_index(s, index, N_SERVER);
			
				SOCKET osock;
				if ((osock = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
				{
					perror("socket");
					closesocket(csock);
				}
				else if (bind(osock, (struct sockaddr *)&laddr, sizeof(laddr))) 
				{
					perror("bind");
					closesocket(csock);
					closesocket(osock);
				}
				else if (connect(osock, (struct sockaddr *)&s[sv_index].addr, sizeof(s[i++].addr))) 
				{
					perror("connect");
					closesocket(csock);
					closesocket(osock);
				}
				else
				{
					clients[i].osock = osock;
					clients[i].csock = csock;
					clients[i].activity = now;
					clients[i].inuse = 1;
					clients[i].server_index = sv_index;
					index = ((index+1)%N_SERVER);
					/*incrementing the server connection counter*/
					s[sv_index].connection++;

          print_avl_server(s,N_SERVER);
				}
			} 
			else 
			{
				fprintf(stderr, "too many clients\n");
				closesocket(csock);
			}
		}
	
//============================================================================================
		/* service any client connections that have waiting data. */
		for (i = 0; i < MAXCLIENTS; i++)
		{
			closeneeded = 0;
			if (!clients[i].inuse)
			{
				continue;
			}
			if (FD_ISSET(clients[i].csock, &fdsr) && clients[i].direction == 0)
			{
				nbyt = recv(clients[i].csock, buf, sizeof(buf), 0);
				if (nbyt == -1)
				{
					printf("\n\nError recv() from client: %s\n", strerror(errno));
					closeneeded = 1;
				}	
				ret = send(clients[i].osock, buf, nbyt, 0);
				if (ret == -1)
				{
					printf("\n\nError send() to server: %s\n", strerror(errno));
					closeneeded = 1;
				}
				if (nbyt != -1 && ret != -1)
					clients[i].activity = now;
				/*updating the communication direction*/	
				clients[i].direction = 1;
				goto timeout;
			} 
			if (FD_ISSET(clients[i].osock, &fdsr) && clients[i].direction == 1)
			{
				nbyt = recv(clients[i].osock, buf, sizeof(buf), 0);
				if (nbyt == -1)
				{
					printf("\n\nError recv() from server: %s\n", strerror(errno));
					closeneeded = 1;
				}	
				ret = send(clients[i].csock, buf, nbyt, 0);
				if (ret == -1)
				{
					printf("\n\nError send() to client: %s\n", strerror(errno));
					closeneeded = 1;
				}
				if (nbyt != -1 && ret != -1)
					clients[i].activity = now;
				/*updating the communication direction*/	
				clients[i].direction = 0;
			}

timeout:

			if (now - clients[i].activity > IDLETIMEOUT)
			{
				closeneeded = 1;
			}
			if (closeneeded)
			{
				closesocket(clients[i].csock);
				closesocket(clients[i].osock);
				clients[i].inuse = 0;
				clients[i].direction = 0;				
				/*decrementing the server connection counter*/
        clients[i].direction = 0;
				s[clients[i].server_index].connection--;
				 print_avl_server(s,N_SERVER);
			}      
		}//end for
	}//end while
//============================================================================================
	
	return 0;
}





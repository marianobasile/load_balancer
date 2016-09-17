#include <stdlib.h>
#include <stdio.h>																							
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#define DIM_TCP_MSG 1024
#define IP_SIZE 16

/* Print the help */
void help() 
{	
	printf("\nCorrect Usage:\n");
	printf("./datapipe_client SERVER_IP_ADDR SERVER_PORT\n\n");
}

void print_error_and_exit() 
{
	printf("./datapipe_client: invalid syntax\n");
	help();
	exit(EXIT_FAILURE);
}

void check_argc_size(int argc) 
{
	if(argc != 3) 
		print_error_and_exit();
}

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

    if( atoi(port) >= 65536) 
    {
    	errno = EINVAL;
    	return -1;
    }

    return 0;
}

void fill_sockaddr(struct sockaddr_in* addr,char * argv[])
{
	bzero(addr, sizeof(*addr));
 	addr->sin_family = AF_INET;
  addr->sin_port = htons(atoi(argv[2]));
  inet_pton(AF_INET, argv[1], &addr->sin_addr);
}

int main(int argc, char * argv[]) {
  	
  int  tcpSocket;
	char tcpBuffer[DIM_TCP_MSG];
 	struct sockaddr_in server_addr;  
 	int ret, nbytes, len,i;
  char serv_addr[INET_ADDRSTRLEN];

  //since strtok modifies argv[]
  strcpy(serv_addr,argv[1]);

 	
 	/*Argc size check */
	check_argc_size(argc);

	if(validate_server_address(argv[1]) == -1)
	{	
		errno = EFAULT;
		printf("Error during server address validation: %s\n", strerror( errno ) );  
    	exit(EXIT_FAILURE);
	}

	if(validate_server_port(argv[2]) == -1)
	{	
		errno = EFAULT;
		printf("Error during server port validation: %s\n", strerror( errno ) );  
    	exit(EXIT_FAILURE);
	}

  fill_sockaddr(&server_addr,argv);

  for(i=0;i<5;i++) 
  {
    if(i==1)
      sleep(5);

    if( (tcpSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
      printf("Error while creating socket: %s\n", strerror( errno ) );  
      exit(EXIT_FAILURE);
    }

    ret = connect(tcpSocket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(ret == -1) 
    {
      printf("Error while performing CONNECT(): %s\n", strerror( errno ) );  
      close(tcpSocket);
      exit(EXIT_FAILURE);
    }
    printf("\nConnessione al server %s (porta: %d) effettuata con successo: %d",serv_addr,atoi(argv[2]),i);
    printf("\n");

    bzero(&tcpBuffer,DIM_TCP_MSG);
    strcpy(tcpBuffer,"MESSAGGIO DAL CLIENT");
    len = sizeof(tcpBuffer);

    if( (nbytes = write(tcpSocket,tcpBuffer,len)) != len)
    {
      printf("Error while performing WRITE(): %s\n", strerror( errno ) );  
      close(tcpSocket);
      exit(EXIT_FAILURE);
    }   

    if( (nbytes = recv(tcpSocket,tcpBuffer,len,MSG_WAITALL)) != len)
    {
      printf("Error while performing RECV(): %s\n", strerror( errno ) );  
      close(tcpSocket);
      exit(EXIT_FAILURE);
    } 
    
    close(tcpSocket);
  }
  
  return 0;
}





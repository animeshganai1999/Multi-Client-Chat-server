#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int sockfd;
pid_t child_pid,parent_pid;

void error(char *msg)
{
  perror(msg);
  exit(0);
}

/*Function to handle ^C and ^Z*/
void sigHandler(int signum)
{ 
	//WRITE YOUR CODE HERE
    printf("\n");
    // fflush(stdout);

    // When client press xtrl+c or ctrl+z then client should QUIT
	char buffer[256];
    bzero(buffer,256);
    sprintf(buffer,"/quit");
    send(sockfd,buffer,sizeof(buffer),0);

    exit(signum);

    // kill(parent_pid, SIGKILL);
    // kill(child_pid,SIGKILL);
    
} 


int main(int argc,char *argv[])
{
    int portno=atoi(argv[1]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent *server;
    server = gethostbyname("127.0.0.1");

    if (server == NULL){
        fprintf(stderr,"ERROR, no such host");
        exit(0);
    }

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr)); // initializes buffer
    serv_addr.sin_family = AF_INET; // for IPv4 family
    bcopy((char *)server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno); //defining port number

    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
         error("ERROR connecting");
    }
    
    

    printf("Enter the commands and type '/quit' to quit\n");
    printf("Enter '/active' to see all available active users\n");
    printf("Enter '/send' <client id> <message> to send the message to the particular client\n");
    printf("Enter '/broadcast' <message> to broadcast a message\n");
    printf("Enter '/quit' to disconnect\n");
    printf("\nconnecting...\n\n");

    int fd[2];
    pipe(fd);

    signal(SIGINT, sigHandler);  // handles ^C
	signal(SIGTSTP , sigHandler);    //handles ^Z

    if((child_pid = fork()) == 0){ // read command from console and send the message to the server
        close(fd[1]); // client will not write anything in stdout
        parent_pid = getpid();
        char buffer[256];
        while(1){
            bzero(buffer,256);
            fgets(buffer,256,stdin);
            send(sockfd,buffer,sizeof(buffer),0);
        }
        
        }else{ // get the message from server and print it to the console
        close(fd[0]); // server will not read anything from stdin
        char buffer[256];
        while(1){
            bzero(buffer,256);

            recv(sockfd,buffer,sizeof(buffer),0);
            if(strncmp(buffer,"CONNECTION TERMINATED",21)==0){
                printf("***SUCCESSFUL TERMINATION***\n");
                // kill(getpid(),0);
                // kill(getpid(),0);
                // kill(parent_pid, SIGKILL);
                kill(child_pid,SIGKILL);
                break;
            }
            printf("%s\n",buffer);
        }
    }
    close(sockfd);

    return 0;
}

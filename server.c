#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>	/* Include this to use semaphores */
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <errno.h>
#include <ctype.h>

// Maximum number of clients can be connected
#define MAX_LIMIT 10
#define MAX_GROUP_LIMIT 5 // each group can have maximum 5 clients
#define MAX_GROUP_NUM 20 // maximum 20 group formation is possible
#define MAX_ABUSIVE_MESSAGE 5 // Number of abusive message allowed

#define P(s) semop(s, &pop, 1)  /* pop is the structure we pass for doing the P(s) operation */
#define V(s) semop(s, &vop, 1)  /* vop is the structure we pass for doing the V(s) operation */

/*Define all the desired data structures as asked in the question*/

typedef struct{
	int sockfd;
	int isActive; // whether the client corresponding to the key is active or not
	int isInitialized; // whether the key ever initialized
	int groupIds[MAX_GROUP_NUM][2]; // index = 0 -> whether the client belongs to that group or not
									// index = 1 -> whether the clinet is admin in that group or not
    int numAbusiveMessage;
} uniqueClientKeys;

// Structure for group
typedef struct{
	int isGroupAvailable; // whether the group is available or not
	int groupCreator; // client id who creats the group
	int clientId[MAX_GROUP_LIMIT]; // all clients ID which belong to the group, if any index is 0 means there is a vacent place to add client
	int numAdmin; // number of admins in the group
	int groupSize; // number of people in the group
	int isBroadCastGroup; // whether the group is a broadcast group or not
} uniqueGroupKey;

// structure to store which clients are requestd to join a group
typedef struct{
	int clientId[MAX_LIMIT][2]; // each index will store the client id who has been requested and also their decision status
	int clientIdAccepted[MAX_LIMIT]; // client id's who has accepted the request
	int numRequest; // number of client has been requested to join the group
	int numReplied; // number of clients replied to the request
	int numAccepted; // number of clients accept the request
	int isInitiated; // is the group id is initiated
	int isActive; // if the group is activated, after joining all the clients
	int adminId; // who initiate the request
} groupIdRequest;

// array of structure which will maintain details of all the clients
uniqueClientKeys clientKeys[100000];
// Array of structure which will maintain all the clients in the group
uniqueGroupKey groupKeys[MAX_GROUP_NUM];
// Array if structure which will store the clients id's are requested to join the group
groupIdRequest joinReq[MAX_GROUP_NUM];

// Number of clients connected to the server, MAX LIMIT 5
int numClients = 0;
// Number of groups in the server
int numGroups = 0;

// This function send the message to a particular client using socket file descriptor
void sendMessageClient(int newsockfd,char *msg){
	char buffer[256];
	strcpy(buffer,msg);
	send(newsockfd,buffer,sizeof(buffer),0);
	return;
}

///////////////////////////////// SOME STUPID ERROR CHECKING //////////////////////////////////////

// Check whether group ID and client ID is valid or not --> Valid interms of whether any alphabetic char is given 
int checkNumberValid(char *id){
	if(id[strlen(id)-1] == '\n'){
		id[strlen(id)-1] = '\0';
	}
	for(int i=0;i<strlen(id);i++){
		if(isdigit(id[i]) == 0){ // not a digit
			return 0; // Not valid ID
		}
	}
	return 1;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ SOME STUPID ERROR CHECKING ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

// Get the whole buffer and extract the COMMAND from that and return it
char* getCommand(char *buffer){
    const char delimiter[] = " ";
    char *tmp;
    tmp = strtok(buffer, delimiter);
    return tmp;
}

/*Function to handle ^C*/
void sigCHandler(int signum) 
{ 
	//WRITE YOUR CODE HERE
	printf("\nServer Terminated\n");
	// When the server terminates all the clients also should terminates
	for(int i=10000;i<=99999;i++){
		if(clientKeys[i].isActive){
			sendMessageClient(clientKeys[i].sockfd,"\nServer has been closed\n");
			sendMessageClient(clientKeys[i].sockfd,"CONNECTION TERMINATED");
		}
	}
	exit(signum);
} 

/*Function to handle ^Z*/
void sigZhandler(int signum) 
{ 
	//WRITE YOUR CODE HERE
	printf("\nServer Terminated\n");
	// When the server terminates all the clients also should terminates
	for(int i=10000;i<=99999;i++){
		if(clientKeys[i].isActive){
			sendMessageClient(clientKeys[i].sockfd,"Server has been closed\n");
			sendMessageClient(clientKeys[i].sockfd,"CONNECTION TERMINATED");
		}
	}
	exit(signum);
}

/*Function to handle errors*/
void error(char *msg)
{
	//WRITE YOUR CODE HERE
	printf("%s\n",msg);
	
}


/*function to add a new client entry to Shared Memory Segment for storing list of active clients*/
void addRecClient(int client_id,int newsockfd){
	//WRITE YOUR CODE HERE
	// When server has reaches to it's maximum limit of connecting client thwn don't connect new client
	if(numClients >= MAX_LIMIT){
		sendMessageClient(newsockfd,"$-> Server has reached to it's mamimum service limit\nPlease try again leter\n");
		sendMessageClient(newsockfd,"CONNECTION TERMINATED");
		return;
	}
	// Establish the connection
	clientKeys[client_id].sockfd = newsockfd;
	clientKeys[client_id].isActive = 1; // activate the client
	clientKeys[client_id].isInitialized = 1; // initialized the client

	// send unique key to the client and a welcome message
	char buffer[256];
	bzero(buffer,256);
	sprintf(buffer,"> %sYour ID is : %d\n","Welcome to Chat Hub!\n",client_id);
	send(newsockfd,buffer,sizeof(buffer),0);
	printf("New client added with id : %d\n",client_id);
	numClients++;
	return;
}

// Remove client from a group, when client QUIT
void removeQuitClient(int keyInt,int newGroupId){

	// Remove from individual client list
	clientKeys[keyInt].groupIds[newGroupId][0] = 0; // remove it from the group
	int isAdmin = 0;
	if(clientKeys[keyInt].groupIds[newGroupId][1]){
		clientKeys[keyInt].groupIds[newGroupId][1] = 0; // remove it's administrative power
		isAdmin = 1;
	}
	
	// Remove from group list
	groupKeys[newGroupId].groupSize--;
	if(isAdmin)
		groupKeys[newGroupId].numAdmin--;
	for(int i=0;i<groupKeys[newGroupId].groupSize;i++){
		if(groupKeys[newGroupId].clientId[i] == keyInt){
			groupKeys[newGroupId].clientId[i] = groupKeys[newGroupId].clientId[groupKeys[newGroupId].groupSize];
			break;
		}
	}
	// remove this client entry form the group
	groupKeys[newGroupId].clientId[groupKeys[newGroupId].groupSize] = 0;

	// If there is no ADMIN in the group then remove that group from server
	if(groupKeys[newGroupId].numAdmin == 0){
		uniqueGroupKey groupKey = groupKeys[newGroupId];

		char message[256];
		sprintf(message,"> There are no Admin in the group %d, so the group has been destroyed\n",newGroupId);

		// From each client field remove that group, and from grouplist remove all clients
		for(int cli=0;cli<MAX_GROUP_LIMIT;cli++){
			if(groupKey.clientId[cli] != 0){
				int clientID = groupKey.clientId[cli];
				clientKeys[clientID].groupIds[newGroupId][0] = 0;
				clientKeys[clientID].groupIds[newGroupId][1] = 0;
				groupKeys[newGroupId].clientId[cli] = 0;
				sendMessageClient(clientKeys[clientID].sockfd,message);
			}
		}

		// reset each parameters of the group
		groupKey.isGroupAvailable = 0; // Make the group available for others
		groupKey.groupCreator = 0;
		groupKey.numAdmin = 0;
		groupKey.groupSize = 0;
		groupKey.isBroadCastGroup = 0;
	}

}


/*function to remove a disconnected client entry from Shared Memory Segment for storing list of active clients*/
void removeRecClient(int key, int newsockfd){
	//WRITE YOUR CODE HERE	
	clientKeys[key].isActive = 0; // make the client deactivate
    clientKeys[key].numAbusiveMessage = 0; // reset to 0

	printf("%d has been disconnected\n",key);

	// Inform all the others clients that a particular client has been disconnected
	for(int i=10000;i<=99999;i++){
		if(i != key && clientKeys[i].isActive){
			char buffer[256];
			sprintf(buffer,"> %d is disconnected\n",key);
			sendMessageClient(clientKeys[i].sockfd,buffer);
		}
	}
	// remove it from all the groups
	for(int gid=0;gid<MAX_GROUP_NUM;gid++){
		if(clientKeys[key].groupIds[gid][0] == 1){ // client key belongs to the gid group
			removeQuitClient(key,gid);
		}
	}
	// Send a termination message to the terminated cleint
	sendMessageClient(newsockfd,"CONNECTION TERMINATED");
	numClients--;
}

/*function to get unique client id mapped to sockfd from the Shared Memory Segment for storing list of active clients*/
int getClientId(){
	//WRITE YOUR CODE HERE
	int lower = 10000;
	int upper = 99999;
	// generate a 5 digit random key
	int uid = (rand() %(upper - lower + 1)) + lower;
	// If the generated key already assigned to a client then regenerate a new key
	while(1){
		if(clientKeys[uid].isActive == 1){
			uid = (rand() %(upper - lower + 1)) + lower;
		}else{
			break;
		}
	}
	// return the unique key
	return uid;
}


/*FUNCTIONS BELOW HANLES ALL SORT OF MESSAGE QUERIES FROM CLIENT*/
// check if the message is abusive or not if abusive then block it
int isTheMessageAbusive(char *message,int key){
    // run the python code to test whether the message is abusive or not
    pid_t pid = 0;
    int status;
    pid = fork();
    if(pid == 0){ // child process handels the KNN part
        // Close STDOUT file descriptor for child
        close(1); // close STDOUT
        execlp("python","python","tester.py",message,NULL);
        printf("Never execute\n");
    }else{
        wait(&status); // wait child process to finish
    }
	// system(msgcheck);

	// Reading the fle
	FILE* fp = fopen("temp.txt","r");
    printf("Message : %s\n",message);
	int isAbusive;
	fscanf(fp,"%d",&isAbusive);

    // inclrease number of accepted abusive message
    if(isAbusive){
        clientKeys[key].numAbusiveMessage++;
    }
    return isAbusive;
}

/* function to processs messages from clients*/
void sendMsg(int newsockfd, int key, char buffer[]){
	//WRITE YOUR CODE HERE

	const char delimiter[] = " ";
    char *tmp;
    tmp = strtok(buffer, delimiter);
	// when destination id is not given
	if(!checkNumberValid(tmp)){
		sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		return;
	}
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> Destination ID is not given!");
		return;
	}
	char destKey[20];
	strcpy(destKey,tmp);

	// No message body or dest id may not valid
	if(destKey[strlen(destKey)-1] == '\n'){
		destKey[strlen(destKey)-1] = '\0';
		// destination ID not valid
		if(strlen(destKey) != 5){
			sendMessageClient(newsockfd,"$-> Destination ID is not valid!");
		}else{ // No message body
			sendMessageClient(newsockfd,"No message to send!");
		}
		return;
	}
	if(strlen(destKey) != 5){
		sendMessageClient(newsockfd,"$-> Destination ID is not valid!");
		return;
	}
	// convert destination client ID to integer
	int destKeyInt = atoi(destKey);

	// client is not connected to server
	if(clientKeys[destKeyInt].isInitialized == 0){
		sendMessageClient(newsockfd,"$-> Client correspoendence to the given ID never connected to the server!");
		return;
	}

	// client is not connected to server
	if(clientKeys[destKeyInt].isActive == 0){
		sendMessageClient(newsockfd,"$-> Client correspoendence to the given ID is not connected to the server now!");
		return;
	}

	int destSockfd = clientKeys[destKeyInt].sockfd;

	// No message body
	tmp = strtok(NULL, "\n");
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> No message to send!");
		return;
	}
	// No message body, all are whitespace
	int allWS = 1;
	for(int i=0;i<strlen(tmp);i++){
		if(!isspace(tmp[i])){
			allWS = 0;
			break;
		}
	}
	if(allWS){
		sendMessageClient(newsockfd,"$-> No message to broadcast!");
		return;
	}
	char messageConetent[256];

	sprintf(messageConetent,"Message from - %d\n-> ",key);
	strcpy(messageConetent+strlen(messageConetent),tmp);

    // Send the text message to check if it is abusive or not
	int isAbusive = isTheMessageAbusive(tmp,key);

	if(isAbusive == 1){
		sendMessageClient(newsockfd,"Sent abusive message. !!MSG Blocked!!");
        // remove that client from the server if number of abusive message exceeds
        if(clientKeys[key].numAbusiveMessage == MAX_ABUSIVE_MESSAGE){
            sendMessageClient(newsockfd,"You are kicked-out form the server, due to abusive languages!\n");
            removeRecClient(key,newsockfd); // remove the client
        }
	}else{
		send(destSockfd,messageConetent,sizeof(messageConetent),0);
		sendMessageClient(newsockfd,"Message delivered to the client successfully!");
	}
	// printf("Dest key = %d\n",destKeyInt);
	// printf("Message content = %s\n",tmp);
	return;
}

/*FUCTION TO HANDLE BROADCAST REQUEST*/
void broadcast(int newsockfd, int key, char *buffer){
	//WRITE YOUR CODE HERE

	const char delimiter[] = "\n";
    char *tmp;
    tmp = strtok(buffer, delimiter);

	// No messgae Body
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> No message to broadcast!");
		return;
	}

	// No message body, all are whitespace
	int allWS = 1;
	for(int i=0;i<strlen(tmp);i++){
		if(!isspace(tmp[i])){
			allWS = 0;
			break;
		}
	}
	if(allWS){
		sendMessageClient(newsockfd,"$-> No message to broadcast!");
		return;
	}

	char messageConetent[256];
	sprintf(messageConetent,"Message from - %d\n-> ",key);
	strcpy(messageConetent+strlen(messageConetent),tmp);
	// printf("buffer = %s",buffer);
	// printf("Message = %s\n",messageConetent);

	// Broadcast the message to all other clients
    int isAbusive = isTheMessageAbusive(tmp,key);

    if(isAbusive == 1){
		sendMessageClient(newsockfd,"Sent abusive message. !!MSG Blocked!!");

        // remove that client from the server if number of abusive message exceeds
        if(clientKeys[key].numAbusiveMessage == MAX_ABUSIVE_MESSAGE){
            sendMessageClient(newsockfd,"You are kicked-out form the server, due to abusive languages!\n");
            removeRecClient(key,newsockfd); // remove the client
        }
	}else{
		for(int i=10000;i<=99999;i++){
            if(i != key && clientKeys[i].isActive){
                send(clientKeys[i].sockfd,messageConetent,sizeof(messageConetent),0);
            }
        }
        sendMessageClient(newsockfd,"Message broadcasted successfully!\n");
	}

	return;
}

// send list of clients which are currenctly connected to the server
void sendClientList(int newsockfd,int key){
	char buffer[256];
	bzero(buffer,256);
	sprintf(buffer,"\nActive client ID's : \n");

	// collecting the clients which are connected to the server
	int cnt = 0;
	for(int i=10000;i<=99999;i++){
		if(clientKeys[i].isActive == 1 && i !=key ){
			sprintf(buffer+strlen(buffer),"%d\n",i);
			cnt++;
		}
	}
	if(cnt == 0){ //no other client available 
		bzero(buffer,256);
		sprintf(buffer,"No other clients available, except You!\n");
	}
	send(newsockfd,buffer,sizeof(buffer),0);
	return;
}
// =============================================== PART - 2 ==============================================================

// Generate a available groupID
int getGroupID(){
	for(int i=0;i<MAX_GROUP_NUM;i++){
		// group is available and no clients send group request with that group id
		if(groupKeys[i].isGroupAvailable == 0 && joinReq[i].isInitiated == 0){ 
			return i;
		}
	}
	return -1; // no group id available
}

int checkPoperClientID(char *key,int newsockfd){
	if(key[strlen(key)-1] == '\n'){
		key[strlen(key)-1] = '\0';
	}

	if(!checkNumberValid(key)){
		sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		return -1;
	}

	int keyInt = atoi(key);

	if(strlen(key) != 5){ // whether the client id is valid or not, 5 digit ID
		char message[256];
		sprintf(message,"$-> %s is Invalid Client ID!\n",key);
		sendMessageClient(newsockfd,message);
		return -1;
	}else if(clientKeys[keyInt].isActive == 0){ // check if the client is connected to the server or not
		char message[256];
		sprintf(message,"$-> %s is not connected to the server!\n",key);
		sendMessageClient(newsockfd,message);
		return -1;
	}
	return keyInt;
}

// Insert client in the group
int insertClientInGroup(int newGroupId, char *key,int newsockfd,int adminKey,int permssionNeed){ // newsockfd -> socked id of the client who is creating the group
	// Check the client ID -> (whether the ke is valid, whether the client is active or not)
	int keyInt = checkPoperClientID(key,newsockfd);
	if(keyInt == -1){
		return -1; //error
	}

	if(adminKey == keyInt){
		sendMessageClient(newsockfd,"You are already in the group, no need to specify!\n");
		return -1;
	}
	// Permission is needed to insert a client, then just send requests to the client, and add them in a spatial buffer
	if(permssionNeed == 1){
		
		char buffer[256];
		joinReq[newGroupId].clientId[joinReq[newGroupId].numRequest][0] = keyInt;
		sprintf(buffer,"> %d has created a group with group id : %d\nYou are invited to join that\n",adminKey,newGroupId);
		sendMessageClient(clientKeys[keyInt].sockfd,buffer);
		return 1;
	}

	// got the client key
	// whether group has reached its max limit or not
	if(groupKeys[newGroupId].groupSize >= MAX_GROUP_LIMIT){
		char message[256];
		sprintf(message,"$-> %s can not be added in the group, group has reached it's max limit!",key);
		sendMessageClient(newsockfd,message);
		return -1;
	}

	// check whether the client already in the group or not
	int alreadyInGroup = 0;
	for(int i=0;i<MAX_GROUP_LIMIT;i++){ //check each possible group
		if(groupKeys[newGroupId].clientId[i] == keyInt){
			char message[256];
			sprintf(message,"$-> %d already in the group!",keyInt);
			sendMessageClient(newsockfd,message);
			return -1;
		}
	}
	// Everything is fine
	// Find the empty place and insert the client
	int emptyPlace = -1;
	for(int i=0;i<MAX_GROUP_LIMIT;i++){
		if(groupKeys[newGroupId].clientId[i] == 0){
			emptyPlace = i;
			break;
		}
	}
	// increase the group size
	groupKeys[newGroupId].groupSize++;
	// Insert thet client in the group
	groupKeys[newGroupId].clientId[emptyPlace] = keyInt;
	// Insert the group details in the client array also
	clientKeys[keyInt].groupIds[newGroupId][0] = 1;
	clientKeys[keyInt].groupIds[newGroupId][1] = 0; // Initially added clients are not Admin

	char message[256];
	sprintf(message,"%s has successfully added in the group!",key);
	sendMessageClient(newsockfd,message);

	// Send the message to the respective client who has added
	bzero(message,256);
	sprintf(message,"> %d is added you in the group %d\n",adminKey,newGroupId);
	// sprintf(message,"%d created a group, with group id : %d\n",adminKey,newGroupId);
	// sprintf(message+strlen(message),"You are added in the group!");
	sendMessageClient(clientKeys[keyInt].sockfd,message);

	return 1;
}

// Make the group without permission
void makeGroupWOPermission(int adminKey,int newsockfd,char* buffer){
	int newGroupId = getGroupID(); // get a new group ID

	printf("Buffer = %s\ngroup id = %d\n",buffer,newGroupId);
	
	char delimiter[] = " ";
    char *tmp;
	tmp = strtok(buffer, delimiter);

    if (tmp == NULL){
		sendMessageClient(newsockfd,"$-> Group can not be formed with only one client!");
		return;
	}
	// Atleast 2 people should be needed to make the group, after making group only 1 people can be in the group
    groupKeys[newGroupId].isGroupAvailable = 1; // make the group unavailable for others
	printf("Before admin insertion\n");

	char message[256];
	sprintf(message,"New Group has been created with group id : %d\n",newGroupId);
	sendMessageClient(newsockfd,message);

	// insert the admin first in the group
	clientKeys[adminKey].groupIds[newGroupId][0] = 1; // client belongs to the group
	clientKeys[adminKey].groupIds[newGroupId][1] = 1; // client is the admin of that group
	groupKeys[newGroupId].clientId[0] = adminKey;
	groupKeys[newGroupId].groupSize = 1; // as admin added in the group
	groupKeys[newGroupId].groupCreator = adminKey;
	groupKeys[newGroupId].numAdmin = 1;
	printf("Before insertin clients\n");

	// Insert rest of the client
	while(tmp){
		if(!checkNumberValid(tmp)){
			sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		}else{
			insertClientInGroup(newGroupId,tmp,newsockfd,adminKey,0);
		}
		tmp = strtok(NULL, delimiter);
	}
	printf("insertion completed\n");
}

// Make the group with permission
void makeGroupWithPermission(int adminKey,int newsockfd,char* buffer){
	int newGroupId = getGroupID(); // get a new group ID

	printf("Buffer = %s\ngroup id = %d\n",buffer,newGroupId);
	
	char delimiter[] = " ";
    char *tmp;
	tmp = strtok(buffer, delimiter);
    if (tmp == NULL){
		sendMessageClient(newsockfd,"$-> Group can not be formed with only one client!");
		return;
	}
	// alote newGroupId to the specific group
	joinReq[newGroupId].isInitiated = 1; // Make the group ID reserved, by initiating it
	// Insert client to the joinReq[newGroupId]
	joinReq[newGroupId].clientId[0][0] = adminKey;
	joinReq[newGroupId].clientIdAccepted[0] = adminKey;
	joinReq[newGroupId].numRequest = 1;
	joinReq[newGroupId].numAccepted = 1;
	joinReq[newGroupId].numReplied = 1;
	joinReq[newGroupId].adminId = adminKey; // initiate the request

	printf("Before insertion clients\n");

	// Insert rest of the client
	while(tmp){
		if(!checkNumberValid(tmp)){
			sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		}else{
			if(insertClientInGroup(newGroupId,tmp,newsockfd,adminKey,1) == 1){
				// insert the client in the requested list
				joinReq[newGroupId].numRequest++;
			}
		}
		tmp = strtok(NULL, delimiter);
	}
	sendMessageClient(newsockfd,"Group invitation has been sent\n");
	printf("Number of request = %d\n",joinReq[newGroupId].numRequest);
	printf("insertion completed\n");
}

// when all clients Replied then create the group, and insert those client who accepts the invitation
void createGroupAndInsertClients(int groupId){
	// Set the group size
	groupKeys[groupId].groupSize = joinReq[groupId].numAccepted;
	// set other parameters
	groupKeys[groupId].isGroupAvailable = 1;
	groupKeys[groupId].groupCreator = joinReq[groupId].adminId;
	groupKeys[groupId].numAdmin = 1;
	// Insert clients to the group
	for(int i=0;i<MAX_GROUP_LIMIT;i++){
		groupKeys[groupId].clientId[i] = joinReq[groupId].clientIdAccepted[i];
	}
	// for each of the client set the group description also
	for(int i=0;i<joinReq[groupId].numAccepted;i++){
		int clientID = joinReq[groupId].clientIdAccepted[i];
		clientKeys[clientID].groupIds[groupId][0] = 1; // indicates that client is part of that group
	}
	// set the admin
	clientKeys[joinReq[groupId].adminId].groupIds[groupId][1] = 1;
}

// validate group id before join, whether that client requested with that group id or not
int validateGroupId(char *gid,int key, int newsockfd){

	if(!checkNumberValid(gid)){
		sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		return -1;
	}
	int groupId = atoi(gid);
	// check whether the group has been initiated with the ID
	if(joinReq[groupId].isInitiated == 0){
		sendMessageClient(newsockfd,"$-> The group has never been initiated!\n");
		return -1;
	}

	printf("Requested List :\n");
	for(int i=0;i<MAX_LIMIT;i++){
		if(joinReq[groupId].clientId[i][0] != 0){
			printf("%d\n",joinReq[groupId].clientId[i][0]);
		}
	}
	printf("Accepted List :\n");
	for(int i=0;i<MAX_LIMIT;i++){
		if(joinReq[groupId].clientIdAccepted[i] != 0){
			printf("%d\n",joinReq[groupId].clientIdAccepted[i]);
		}
	}

	// check whether the client has been requested to join that group or not
	int requested = 0;
	for(int i=0;i<MAX_LIMIT;i++){
		if(joinReq[groupId].clientId[i][0] == key){
			// check whether the cleint make the decision
			if(joinReq[groupId].clientId[i][1] == 1){
				sendMessageClient(newsockfd,"$-> You already made your decision, can't change now!\n");
				return -1;
			}else{
				joinReq[groupId].clientId[i][1] = 1; // client has made the decision
			}
			requested = 1;
			break;
		}
	}
	if(requested == 0){
		sendMessageClient(newsockfd,"$-> You are not invited to join that group!\n");
		return -1;
	}
	return groupId;
}

// Join the group with group ID
void joinGroup(int key, int newsockfd, char *gid){
	int groupId = validateGroupId(gid,key,newsockfd);
	if(groupId == -1){
		return;
	}
	// when number of accepts reaches max limit
	if(joinReq[groupId].numAccepted >= MAX_GROUP_LIMIT){
		sendMessageClient(newsockfd,"$-> Group has reached It's maximum limit, you can not join now anymore!\n");
		return;
	}

	joinReq[groupId].clientIdAccepted[joinReq[groupId].numAccepted] = key;

	joinReq[groupId].numAccepted++;
	joinReq[groupId].numReplied++;

	sendMessageClient(newsockfd,"Request Accepted!\n");

	printf("Accepted List :\n");
	printf("num Accepted = %d\n",joinReq[groupId].numAccepted);
	for(int i=0;i<MAX_LIMIT;i++){
		if(joinReq[groupId].clientIdAccepted[i] != 0){
			printf("%d\n",joinReq[groupId].clientIdAccepted[i]);
		}
	}

	// If all the clients approve the group request then create the group
	if(joinReq[groupId].numReplied == joinReq[groupId].numRequest){
		createGroupAndInsertClients(groupId);
		printf("Group with id = %d, Has been created\n",groupId);
	}
}

// Decline the group request with group ID
void declinegroup(int key,int newsockfd,char* gid){
	int groupId = validateGroupId(gid,key,newsockfd);
	if(groupId == -1){
		return;
	}

	joinReq[groupId].numReplied++;
	sendMessageClient(newsockfd,"$-> Request Accepted!\n");
	// If all the clients approve the group request then create the group
	if(joinReq[groupId].numReplied == joinReq[groupId].numRequest){
		createGroupAndInsertClients(groupId);
		printf("Group with id = %d, Has been created\n",groupId);
	}

}

// Send message to a group
void sendMessagetoGroup(int key,int newsockfd,char* buffer){
	const char delimiter[] = " ";
    char *tmp;
    tmp = strtok(buffer, delimiter);
	if(!checkNumberValid(tmp)){
		sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		return;
	}
	// when destination id is not given
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> Group ID is not given!");
		return;
	}
	char message[256];
	int groupId = atoi(tmp);
	tmp = strtok(NULL,"\n");
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> No message body!");
		return;
	}
	// check whether the client is a member of the group or not
	if(clientKeys[key].groupIds[groupId][0] == 0){ //client is not the group memeber
		sendMessageClient(newsockfd,"$-> You are not a member of that group!");
		return;
	}
	// Check whether the group is a broadcast group or not, if it is a broadcast group then admins only can send the message
	if(groupKeys[groupId].isBroadCastGroup == 1 && clientKeys[key].groupIds[groupId][1] == 0){
		// the group is broadcast group and client is not an admin of that group
		bzero(message,256);
		sprintf(message,"$-> %d is a broadcast group and you are not an Admin, so you can not send any message in the group\n",groupId);
		sendMessageClient(newsockfd,message);
		return;
	}

	// Send the message to all group members, except itself
	bzero(message,256);
	sprintf(message,"\nGroup Message from : %d  ",key);
	for(int i=0;i<groupKeys[groupId].groupSize;i++){
		if(key != groupKeys[groupId].clientId[i]){
			int clientID = groupKeys[groupId].clientId[i];
			int sockfd = clientKeys[clientID].sockfd;
			sendMessageClient(sockfd,message);
			sendMessageClient(sockfd,tmp);
		}
	}
	sendMessageClient(newsockfd,"Message delivered successfully\n");
}

// Make a client the admin of a group
void makeAdmin(int key,int newsockfd,char* buffer){
	char message[256];
	const char delimiter[] = " ";
    char *tmp;
    tmp = strtok(buffer, delimiter);

	if(!checkNumberValid(tmp)){
		sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		return;
	}

	// when destination id is not given
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> Group ID is not given!");
		return;
	}
	int groupId = atoi(tmp);
	tmp = strtok(NULL,"\n");
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> client ID not given!");
		return;
	}
	int clientID = atoi(tmp);
	// check whether the client is a member of the group or not
	if(clientKeys[key].groupIds[groupId][0] == 0){ //client is not the group memeber
		sendMessageClient(newsockfd,"$-> You are not a member of that group!");
		return;
	}
	// check whether the client is an admin or not of that group
	if(clientKeys[key].groupIds[groupId][1] == 0){ //client is not the group admin
		sendMessageClient(newsockfd,"$-> You are not an admin of the group!");
		return;
	}
	// check whether the client (to whom admin wants to make admin) is a member of that group or not
	if(clientKeys[clientID].groupIds[groupId][0] == 0){ //client is not the group memeber
		sendMessageClient(newsockfd,"$-> Given client is not member of that group!");
		return;
	}
	// check whether the client is already an admin for that group or not
	if(clientKeys[clientID].groupIds[groupId][1] == 1){
		bzero(message,256);
		sprintf(message,"$-> %d is already an Admin of the group %d\n",clientID,groupId);
		sendMessageClient(newsockfd,message);
		return;
	}
	// Make the client admin
	clientKeys[clientID].groupIds[groupId][1] = 1;
	groupKeys[groupId].numAdmin++;

	// Notify both the client
	bzero(message,256);
	sprintf(message,"%d successfully become the admin of the group %d\n",clientID,groupId);
	sendMessageClient(newsockfd,message);

	bzero(message,256);
	sprintf(message,"> You become the admin of the group %d\n",groupId);
	sendMessageClient(clientKeys[clientID].sockfd,message);
}

// Add new clients in the group
void addToGroup(int key,int newsockfd,char* buffer){
	const char delimiter[] = " ";
    char *tmp;
    tmp = strtok(buffer, delimiter);

	if(!checkNumberValid(tmp)){
		sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		return;
	}

	// when destination id is not given
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> Group ID is not given!");
		return;
	}
	int groupId = atoi(tmp);
	// check whether the client is a member of the group or not
	if(clientKeys[key].groupIds[groupId][0] == 0){ //client is not the group memeber
		sendMessageClient(newsockfd,"$-> You are not a member of that group!");
		return;
	}
	// check whether the client is an admin or not of that group
	if(clientKeys[key].groupIds[groupId][1] == 0){ //client is not the group admin
		sendMessageClient(newsockfd,"$-> You are not an admin of the group!");
		return;
	}
	// Insert rest of the client
	tmp = strtok(NULL, delimiter);
	while(tmp){
		if(!checkNumberValid(tmp)){
			sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		}else{
			insertClientInGroup(groupId,tmp,newsockfd,key,0);
		}
		tmp = strtok(NULL, delimiter);
	}

}

// Remove a client from the group
int removeClientFromTheGroup(int newGroupId, char *key,int newsockfd,int adminKey){
	// Check the client ID -> (whether the ke is valid, whether the client is active or not)
	int keyInt = checkPoperClientID(key,newsockfd);
	if(keyInt == -1){
		return -1; //error
	}
	char message[256];
	// check whether the clientKey belongs to the given group
	if(clientKeys[keyInt].groupIds[newGroupId][0] == 0){ //client does not belong to the group
		sprintf(message,"$-> %d does not belong to the group %d\n",keyInt,newGroupId);
		sendMessageClient(newsockfd,message);
		return -1;
	}
	// If admin wants to remove itself then not possible
	if(adminKey == keyInt){
		sendMessageClient(newsockfd,"$-> You can not remove yourself from the group!\n");
		return -1;
	}
	// Means client belongs to the group
	// Remove from individual client list
	clientKeys[keyInt].groupIds[newGroupId][0] = 0; // remove it from the group
	int isAdmin = 0;
	if(clientKeys[keyInt].groupIds[newGroupId][1]){
		clientKeys[keyInt].groupIds[newGroupId][1] = 0; // remove it's administrative power
		isAdmin = 1;
	}
	
	// Remove from group list
	groupKeys[newGroupId].groupSize--;
	if(isAdmin)
		groupKeys[newGroupId].numAdmin--;
	for(int i=0;i<groupKeys[newGroupId].groupSize;i++){
		if(groupKeys[newGroupId].clientId[i] == keyInt){
			groupKeys[newGroupId].clientId[i] = groupKeys[newGroupId].clientId[groupKeys[newGroupId].groupSize];
			break;
		}
	}
	groupKeys[newGroupId].clientId[groupKeys[newGroupId].groupSize] = 0;

	// Notify the client who has been removed
	bzero(message,256);
	sprintf(message,"> You are removed by %d, from the group %d\n",adminKey,newGroupId);
	sendMessageClient(clientKeys[keyInt].sockfd,message);

	// Notify the admin
	bzero(message,256);
	sprintf(message,"%d has been removed from the group %d\n",keyInt,newGroupId);
	sendMessageClient(newsockfd,message);

}
// Remove client from group
void removeFromGroup(int key,int newsockfd,char* buffer){
	const char delimiter[] = " ";
    char *tmp;
    tmp = strtok(buffer, delimiter);

	if(!checkNumberValid(tmp)){
		sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		return;
	}

	// when destination id is not given
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> Group ID is not given!");
		return;
	}
	int groupId = atoi(tmp);
	// check whether the client is a member of the group or not
	if(clientKeys[key].groupIds[groupId][0] == 0){ //client is not the group memeber
		sendMessageClient(newsockfd,"$-> You are not a member of that group!");
		return;
	}
	// check whether the client is an admin or not of that group
	if(clientKeys[key].groupIds[groupId][1] == 0){ //client is not the group admin
		sendMessageClient(newsockfd,"$-> You are not an admin of the group!");
		return;
	}
	// Remove rest of the client
	tmp = strtok(NULL, delimiter);
	while(tmp){
		if(!checkNumberValid(tmp)){
			sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		}else{
			removeClientFromTheGroup(groupId,tmp,newsockfd,key);
		}
		tmp = strtok(NULL, delimiter);
	}

}

// Make a group broadcast group
void makeGroupBroadcast(int key,int newsockfd,char* buffer){
	const char delimiter[] = " ";
    char *tmp;
    tmp = strtok(buffer, delimiter);

	if(!checkNumberValid(tmp)){
		sendMessageClient(newsockfd,"$-> Alphabetic characters are not valid ID!\n");
		return;
	}
	// when destination id is not given
	if(tmp == NULL){
		sendMessageClient(newsockfd,"$-> Group ID is not given!");
		return;
	}
	int groupId = atoi(tmp);
	// check whether the client is a member of the group or not
	if(clientKeys[key].groupIds[groupId][0] == 0){ //client is not the group memeber
		sendMessageClient(newsockfd,"$-> You are not a member of that group!");
		return;
	}
	// check whether the client is an admin or not of that group
	if(clientKeys[key].groupIds[groupId][1] == 0){ //client is not the group admin
		sendMessageClient(newsockfd,"$-> You are not an admin of the group!");
		return;
	}
	char message[256];
	// make the group broadcast
	if(groupKeys[groupId].isBroadCastGroup == 1){
		sprintf(message,"%d is already a broadcast group\n",groupId);
		sendMessageClient(newsockfd,message);
	}else{
		groupKeys[groupId].isBroadCastGroup = 1;
		sprintf(message,"%d becomes a broadcast group successfully\n",groupId);
		sendMessageClient(newsockfd,message);
	}

}

// List of all active groups, and the sender is also a part of that group
void activeGroups(int key,int newsockfd){
	char message[256];
	uniqueClientKeys clientContent = clientKeys[key];
	int isAnyGroup = 0;

	for(int i=0;i<MAX_GROUP_NUM;i++){
		// check if the client is a part of any group
		if(clientContent.groupIds[i][0] == 1){
			isAnyGroup = 1;
			sprintf(message+strlen(message),"\nGroup ID : %d\n",i);

			sprintf(message+strlen(message),"Group Admins id's : \n");
			for(int j=0;j<MAX_GROUP_LIMIT;j++){
				if(groupKeys[i].clientId[j] != 0){
					int clientID = groupKeys[i].clientId[j];
					if(clientKeys[clientID].groupIds[i][1] == 1){
						sprintf(message+strlen(message),"%d\n",clientID);
					}
				}
			}

			sprintf(message+strlen(message),"Client id's associated with the group : \n");
			for(int j=0;j<MAX_GROUP_LIMIT;j++){
				if(groupKeys[i].clientId[j] != 0){
					sprintf(message+strlen(message),"%d\n",groupKeys[i].clientId[j]);
				}
			}
		}
	}
	if(isAnyGroup == 0){
		sprintf(message,"No groups are assiciated with you\n");
	}
	sendMessageClient(newsockfd,message);
}

// =======================================================================================================================
void viewGroupDetails(char *gid,int newsockfd){
	int id = atoi(gid);
	char buffer[256];
	sprintf(buffer,"\nClients are - \n");
	for(int i=0;i<MAX_GROUP_LIMIT;i++){
		if(groupKeys[id].clientId[i] != 0){
			sprintf(buffer+strlen(buffer),"%d\n",groupKeys[id].clientId[i]);
		}
	}
	sprintf(buffer+strlen(buffer),"Admins are -\n");
	printf("Admins are : \n");
	for(int i=0;i<MAX_GROUP_LIMIT;i++){
		if(groupKeys[id].clientId[i] != 0){
			int clientID = groupKeys[id].clientId[i];
			printf("%d, %d\n",clientKeys[clientID].groupIds[id][0],clientKeys[clientID].groupIds[id][1]);
			if(clientKeys[clientID].groupIds[id][1] == 1){
				sprintf(buffer+strlen(buffer),"%d\n",clientID);
			}
		}
	}
	sendMessageClient(newsockfd,buffer);
}

// =======================================================================================================================

/*Function to handle all the commands as entered by the client*/ 
int performAction(int key){
	/*You are instructed to implement the following utilities:
	  -/active
	  -/send <client id> <Message>
	  -/broadcast <Message>
	  -/quit
	*/
	//WRITE YOUR CODE HERE
	int newsockfd = clientKeys[key].sockfd;

	char buffer[256];
	bzero(buffer,256);
	int sts = recv(newsockfd,buffer,sizeof(buffer),0);

	// printf("Status : %d\n",sts);

	if(clientKeys[key].isActive == 0){ // means connection is closed
		// removeRecClient(key,newsockfd);
		return 0;
	}

	printf("Getting from client : %s\n",buffer);
	char command[30];
	strcpy(command,getCommand(buffer));

	if(strncmp(command,"/activegroups",13) == 0){ // see all the groups details in which the requested client a member

		activeGroups(key,newsockfd);

	}else if(strncmp(command,"/active",7) == 0){ // display all the available active client connected to the server

		printf("Sending List of Active clients\n");
		sendClientList(newsockfd,key);

	}else if(strncmp(command,"/sendgroup",10) == 0){ // send message to a particular group
		
		char tbuffer[256];
		strcpy(tbuffer,buffer+11);
		sendMessagetoGroup(key,newsockfd,tbuffer);

	}else if(strncmp(command,"/send",5) == 0){ // send the message to the perticular client

		char tbuffer[256];
		strcpy(tbuffer,buffer+6);
		sendMsg(newsockfd,key,tbuffer);

	}else if(strncmp(command,"/broadcast",10) == 0){ // send the message to all other clients connected to the server

		char tbuffer[256];
		strcpy(tbuffer,buffer+11);
		broadcast(newsockfd,key,tbuffer);

	}else if(strncmp(command,"/quit",5) == 0){ // remove the connection

		removeRecClient(key,newsockfd);

	}else if(strncmp(command,"/makegroupbroadcast",19) == 0){ // make the group broadcast group
		
		char tbuffer[256];
		strcpy(tbuffer,buffer+20);
		makeGroupBroadcast(key,newsockfd,tbuffer);

	}else if(strncmp(command,"/makegroupreq",13) == 0){ // make the group with permission

		char tbuffer[256];
		strcpy(tbuffer,buffer+14);
		makeGroupWithPermission(key,newsockfd,tbuffer);

	}else if(strncmp(command,"/makegroup",10) == 0){ // make the group without permission

		char tbuffer[256];
		strcpy(tbuffer,buffer+11);
		makeGroupWOPermission(key,newsockfd,tbuffer);

	}else if(strncmp(command,"/group",6) == 0){ // see details for a particular group <--- Not needed for the assignment

		char tbuffer[256];
		strcpy(tbuffer,buffer+7);
		viewGroupDetails(tbuffer,newsockfd);

	}else if(strncmp(command,"/joingroup",10) == 0){ // join a group for which you had requested before
		
		char tbuffer[256];
		strcpy(tbuffer,buffer+11);
		joinGroup(key,newsockfd,tbuffer);

	}else if(strncmp(command,"/declinegroup",13) == 0){ // decline a group for which you had requested before
		
		char tbuffer[256];
		strcpy(tbuffer,buffer+14);
		declinegroup(key,newsockfd,tbuffer);

	}else if(strncmp(command,"/makeadmin",10) == 0){ // make one client an admin
		
		char tbuffer[256];
		strcpy(tbuffer,buffer+11);
		makeAdmin(key,newsockfd,tbuffer);

	}else if(strncmp(command,"/addtogroup",11) == 0){ // add new client in a group
		
		char tbuffer[256];
		strcpy(tbuffer,buffer+12);
		addToGroup(key,newsockfd,tbuffer);

	}else if(strncmp(command,"/removefromgroup",16) == 0){ // remove a client from a group
		
		char tbuffer[256];
		strcpy(tbuffer,buffer+17);
		removeFromGroup(key,newsockfd,tbuffer);

	}else{
		sendMessageClient(newsockfd,"Not a valid command!\n");
	}
}


/*Call this function from the while loop to initiate the process as requested by the client. In this function you should call performAction which handles requests in a modular fashion.*/
void dostuff()
{
	//WRITE YOUR CODE HERE	
}

int main(int argc, char *argv[])
{
	srand(time(0)); /*seeding the rand() function*/
	
	signal(SIGINT, sigCHandler);  // handles ^C
	signal(SIGTSTP , sigZhandler);    //handles ^Z
	
	if (argc < 2) {
		fprintf(stderr,"ERROR, no port provided\n");
		exit(1);
	}

	int sockfd, newsockfd, portno, clilen, pid,client_id,flags;
	struct sockaddr_in serv_addr, cli_addr;
	  
	sockfd = socket(AF_INET, SOCK_STREAM, 0);  /*getting a sockfd for a TCP connection*/
	if (sockfd < 0)  error("ERROR opening socket");

	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	if ((flags = fcntl(sockfd, F_GETFL, 0)) < 0) 
	{ 
		error("can't get flags to SCOKET!!");
	} 


	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) 
	{ 
		error("fcntl failed.");
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));

	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET; /*symbolic constant for IPv4 address*/
	serv_addr.sin_addr.s_addr = INADDR_ANY; /*symbolic constant for holding IP address of the server*/
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		error("ERROR on binding");
 	}
 	listen(sockfd, 10);
 	/*Initialize all the data structures and the semaphores you will need for handling the client requests*/

	// Initialize file descriptor set
 	fd_set readfds;
	// clear the set
 	FD_ZERO(&readfds);
	// variable used to keep track of maximum value of file descriptor
	int clientfd;

	while (1) {
		/*Write appropriate code to set up fd_set to be used in the select call. Following is a rough outline of what needs to be done here	
			-Calling FD_ZERO
			-Calling FD_SET
			-identifying highest fd for using it in select call
		*/
		// insert sockfd to the set
		FD_SET(sockfd, &readfds);
		int maxfd = sockfd;
		// insert all the client file descriptor to the set, which are currentlt connected to the server
		for(int i=10000;i<=99999;i++){
			if(clientKeys[i].isActive){
				FD_SET(clientKeys[i].sockfd, &readfds);
				// printf("Socked ID = %d\n",clientKeys[i].sockfd);
				if(clientKeys[i].sockfd > maxfd){
					maxfd = clientKeys[i].sockfd;
				}
			}
		}
		// printf("FD_SET regenerated\n");
		int activity = select(maxfd+1, &readfds, NULL, NULL, NULL); //give appropriate parameters 

		if((activity<0)&&(errno!=0)) //fill appropriate parameters here
		{
			error("!!ERROR ON SELECT!!");
		}

		if(FD_ISSET(sockfd, &readfds)){ // if sockfd is in the fd_set that means a new connection is there, so serve it first
			struct sockaddr_in cli_addr;
			printf("New connection\n");
			socklen_t clilen;
			clilen = sizeof(cli_addr);
			const int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
			// get the unique ID for the new client
			int client_id = getClientId();
			FD_SET(newsockfd, &readfds);
			// call the function so that it cam connect the client to the server
			addRecClient(client_id,newsockfd);

		}else{ // no new connection is there, so serve the client

			// for all the clients check which needs any service
			for(int key = 10000;key <= 99999;key++){
				if(FD_ISSET(clientKeys[key].sockfd, &readfds)){ // particular client needs service
					performAction(key);
				}
			}

		}
		/*After successful select call you can now monitor these two scenarios in a non-blocking fashion:
			- A new connection is established, and
			- An existing client has made some request
		  You have to handle both these scenarios after line 191.
		*/

	} /* end of while */
	
	return 0; /* we never get here */
}


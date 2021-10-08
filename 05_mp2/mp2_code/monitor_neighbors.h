#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

// an array that is used to record neighbours
extern int neighbour[256];

// an array that stores the cost between current node and the neighbour
extern int cost[256];

// a 2D array that stores the connection between nodes
extern int network[256][256];

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}
}

void printNeighbour(){
	// print the neighbours
	for(int i = 0; i < 256; i++){
		if(neighbour[i]){
			printf("n");
			printf("%d", i);
			printf("c");
			printf("%d", cost[i]);
			printf(" ");
		}
	}
	printf("\n");
}

void announceChanges(int n){
	// input, change to the nth neighbour
	// the neighbours of current node has changed
	// need to announce this information to available neighbours
	// format: change 1 to 2 cn 1 cs 10, 1 to 2 is connected with cost as 10
	// another example: change 1 to 2 cn 0 cs 10, 1 to 2 is no longer connected
	char message[256] = "change ";
	char temp[256];
	sprintf(temp, "%d", globalMyID);
	strcat(message, temp);
	strcat(message, " to ");
	sprintf(temp, "%d", n);
	strcat(message, temp);
	strcat(message, " cn ");
	sprintf(temp, "%d", neighbour[n]);
	strcat(message, temp);
	strcat(message, " cs ");
	sprintf(temp, "%d", cost[n]);
	strcat(message, temp);
	hackyBroadcast(message, strlen(message));
	printNeighbour();
}

void checkTimeOut(){
	// check whether the neighbour has been lost
	// time interval is 1 s
	struct timeval current_time;
	int timeout = 1;
	gettimeofday(&current_time, NULL);
	for(int i = 0; i < 256; i++){
		//int current_us = (current_time.tv_sec % 10) * 1000000 + current_time.tv_usec;
		//int last_connect_us = (globalLastHeartbeat[i].tv_sec % 10) * 1000000 + globalLastHeartbeat[i].tv_usec;
		if(!neighbour[i]){
			// skip non-neighbours
			continue;
		}
		//printf("test check out: %d %d %d\n", current_time.tv_sec, globalLastHeartbeat[i].tv_sec, timeout );
		if(current_time.tv_sec - globalLastHeartbeat[i].tv_sec  > timeout){
			printf("%d", i);
			printf(", timeout\n");
			neighbour[i] = 0;
			network[globalMyID][i] = 0;
			network[i][globalMyID] = 0;
			// also needs to broadcast this
			announceChanges(i);
		}
	}
}

void listenForNeighbors()
{
	printf("listen...\n");
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000]={'\0'};

	int bytesRecvd;
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		
		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
			if(!neighbour[heardFrom]){
				// a newly connected neighbour
				// record this neighbour, announce to others
				neighbour[heardFrom] = 1;
				network[globalMyID][heardFrom] = cost[heardFrom];
				network[heardFrom][globalMyID] = cost[heardFrom];
				printf("received\n");
				announceChanges(heardFrom);
			}
			//printNeighbour();
			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp(recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
			// ...
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(recvBuf, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
		}
		
		//TODO now check for the various types of packets you use in your own protocol
		else if(!strncmp(recvBuf, "change", 6))
		{
			printf("Connection changed\n");
			printf("%s\n", recvBuf);
			char* token = strtok(recvBuf, " ");
			token = strtok(NULL, " ");
			int source = atoi(token);
			token = strtok(NULL, " ");
			token = strtok(NULL, " ");
			int destination = atoi(token);
			token = strtok(NULL, " ");
                        token = strtok(NULL, " ");
			int connection = atoi(token);
			token = strtok(NULL, " ");
                        token = strtok(NULL, " ");
			int link_cost = atoi(token);
			printf("%d %d %d %d\n", source, destination, connection, link_cost);

		}

		checkTimeOut();
		// ... 
	}
	//(should never reach here)
	close(globalSocketUDP);
}


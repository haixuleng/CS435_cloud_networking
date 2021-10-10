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

extern int broadcastSequence;

extern int broadcastSequenceFrom[256];

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

// an array records the previous node to the destination by the shortest path
extern int pred[256];

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.

void encodeNeighbour(char* message);
void announceChanges(int n);
int nextHop(int destination);

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
	int count = 1;
	int refresh_count = 20; // manually do a refresh every 20 announcement
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		//hackyBroadcast("HEREIAM", 7);
		if(count > refresh_count){
			count = 1;
			broadcastSequence++;
		}
		announceChanges(0);
		nanosleep(&sleepFor, 0);
		count++;
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

void printTopology(){
	// print the known topology
	for(int i = 0; i < 255; i++){
		for(int j = i + 1; j < 256; j++){
			if(network[i][j]){
				printf("%dn%dc%d ", i, j, network[i][j]);
			}
		}
	}
	printf("\n");
	for(int i = 0; i < 256; i++){
		if(broadcastSequenceFrom[i] > 1){
			printf("To %d hop to %d\n", i, nextHop(i));
		}
	}
	printf("\n");
}

void encodeNeighbour(char* message){
	// encode the current neighbour information into a message
	// format:
	// "broadcast 0 1 2 10 3 11"
	// The first number is the sequence number, the second is the source
	// the third is the destination, and the cost is the fourth. The fifth is the next desstination, etc
	// seq 0 cost between 1 and 2 is 10, between 1 and 3 is 11.
	// If no neigbours stop at from 1
	// modify the encoded "message"
	
	char temp[256] = "\0";
	sprintf(message, "%s", "broadcast ");
	sprintf(temp, "%d", broadcastSequence);
	strcat(message, temp);
        strcat(message, " ");
	sprintf(temp, "%d", globalMyID);
	strcat(message, temp);
        strcat(message, " ");
	for(int i = 0; i < 256; i++){
		if(neighbour[i]){
			sprintf(temp, "%d", i);
			strcat(message, temp);
			strcat(message, " ");
			sprintf(temp, "%d", cost[i]);
			strcat(message, temp);
			strcat(message, " ");
		}
	}
	strcat(message, "\0");
	//broadcastSequence++; // increment on the sequence
}

typedef struct decodedMessage{
	// struct to help decode broadcast message
	
	int seq;
	int from;
	int fromCost[256]; // the ith element is the cost between "from" and "i"	
} decodedMessage;

decodedMessage decode(char* message){
	// decode the broadcast message, return a decodeMessage type
	decodedMessage a = {0, 0, {0}}; // initialization
	char* token = strtok(message, " ");
	token = strtok(NULL, " "); // point at sequence
	a.seq = atoi(token);
	token = strtok(NULL, " "); // point at source
	a.from = atoi(token);
	// parse cost pairs
	token = strtok(NULL, " ");
	while(token != NULL){
		int to = atoi(token);
		token = strtok(NULL, " ");
		a.fromCost[to] = atoi(token);
		token = strtok(NULL, " ");
	}
	return a;
}

void announceChanges(int n){
	// send out the broadcast
	char message[256];
	if(!broadcastSequenceFrom[globalMyID]){
		broadcastSequenceFrom[globalMyID] = 1;
		broadcastSequence = 1;
	}
	encodeNeighbour(message);
	hackyBroadcast(message, strlen(message));
}

void checkTimeOut(){
	// check whether the neighbour has been lost
	// time interval is 1 s
	struct timeval current_time;
	int timeout = 1;
	gettimeofday(&current_time, NULL);
	for(int i = 0; i < 256; i++){
		if(!neighbour[i]){
			// skip non-neighbours
			continue;
		}
		if(current_time.tv_sec - globalLastHeartbeat[i].tv_sec  > timeout){
			printf("%d", i);
			printf(", timeout\n");
			neighbour[i] = 0;
			network[globalMyID][i] = 0;
			network[i][globalMyID] = 0;
			// also needs to broadcast this
			broadcastSequence++; // increment on the sequence
			broadcastSequenceFrom[globalMyID] = broadcastSequence;
			announceChanges(i);
		}
	}
}

void dijkstra(){
	// Dijkstra algorithm
	// Tie breaking is not implemented yet

	int INFINITY = 9999;
	int n = 256;
	int start = globalMyID;
	int Graph[256][256] = {0};
	int distance[n];
	//int pred[n];
	int visited[n];
	int count, mindistance, nextnode;
	// initialize Graph by copying values from network
	// if network[i][j] = 0, then Graph[i][j] = INFINITY
	for(int i = 0; i < n; i++){
		for(int j = 0; j < n; j++){
			if(network[i][j]){
				Graph[i][j] = network[i][j];
			}
			else{
				Graph[i][j] = INFINITY;
			}
		}
	}
	
	for(int i = 0; i < n; i++){
		distance[i] = Graph[start][i];
		pred[i] = start;
		visited[i] = 0;
	}

	distance[start] = 0;
	visited[start] = 1;
	count = 1;

	while(count < n - 1){
		// find the minimum distance to extend the frontier
		// it should be implemented with heap
		mindistance = INFINITY;
		for(int i = 0; i < n; i++){
			if(distance[i] < mindistance && !visited[i]){
				mindistance = distance[i];
				nextnode = i;
			}
		}
		// update the map
		visited[nextnode] = 1;
		for(int i = 0; i < n; i++){
			if(!visited[i]){
				if(mindistance + Graph[nextnode][i] < distance[i]){
					distance[i] = mindistance + Graph[nextnode][i];
					pred[i] = nextnode;
				}
			}
		}
		count ++;
	}
}

int nextHop(int destination){
	// return the next hop for the destination
	// if a path does not exit, return -1
	while(pred[destination] != globalMyID){
		destination = pred[destination];
	}
	if(network[globalMyID][destination]){
		return destination;
	}
	else{
		return -1;
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
		memset(recvBuf, '\0', 1000); // reset memory of recvBuf
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
				printf("Found new neighbour\n");
				broadcastSequence++; // increment on the sequence
				broadcastSequenceFrom[globalMyID] = broadcastSequence;
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
		else if(!strncmp(recvBuf, "broadcast", 6))
		{
			char rawRecvBuf[1000]; // the recvBuf will be modified after parsing
			strcpy(rawRecvBuf, recvBuf);
			struct decodedMessage broadcast_message = decode(recvBuf);
			if(broadcast_message.seq > broadcastSequenceFrom[broadcast_message.from]){
				// new announcement to me, update my knowledge
				//printf("Seq: %d, %d id: %d\n", broadcast_message.seq, broadcastSequenceFrom[broadcast_message.from], globalMyID);
				broadcastSequenceFrom[broadcast_message.from] = broadcast_message.seq;
				// reset the network connected to "from"
				for(int i = 0; i< 256; i++){
					network[broadcast_message.from][i] = 0;
					network[i][broadcast_message.from] = 0;
				}
				// update the network connected to "from"
				for(int i = 0; i< 256; i++){
					if(broadcast_message.fromCost[i]){// if connected
						network[broadcast_message.from][i] = broadcast_message.fromCost[i];
						network[i][broadcast_message.from] = broadcast_message.fromCost[i];
					}
                                }
				printf("New message: %s\n", rawRecvBuf);
				//hackyBroadcast(rawRecvBuf, strlen(rawRecvBuf)); // forward the message to the beighbours
				dijkstra();
				printTopology();
			}
			hackyBroadcast(rawRecvBuf, strlen(rawRecvBuf));
		}

		checkTimeOut();
		// ... 
	}
	//(should never reach here)
	close(globalSocketUDP);
}


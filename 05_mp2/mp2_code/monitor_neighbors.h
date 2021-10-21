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
//#include "data_structure.h"

#define graphSize 256

extern int globalMyID;

extern int broadcastSequence;

extern int broadcastSequenceFrom[graphSize];

//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[graphSize];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[graphSize];

// an array that is used to record neighbours
extern int neighbour[graphSize];

// an array that stores the cost between current node and the neighbour
extern int cost[graphSize];

// a 2D array that stores the connection between nodes
extern int network[graphSize][graphSize];

// an array records the previous node to the destination by the shortest path
extern int pred[graphSize];

// set to 1 if we want to forward the announcement
extern int forwardFlag;
extern int broadcastFlag;
// stores the message from broadcast
extern char announcementBuf[512];
// used to avoid duplicated broadcast
extern int announcementSource;
extern int announcementFrom;
extern int announcementBufLength;

extern int dumpReceived = 0;

extern int globalSocketUDP;

pthread_mutex_t forwardFlag_m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t broadcastFlag_m = PTHREAD_MUTEX_INITIALIZER;

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.

int encodeNeighbour(char* message);
void announceChanges(int n);
int nextHop(int destination);

void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<graphSize;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void forward(char* buf, int dest, int length){
	// send message to dest
	//int length = strlen(buf);
	if(sendto(globalSocketUDP, buf, length, 0,
                                  (struct sockaddr*)&globalNodeAddrs[dest], sizeof(globalNodeAddrs[dest]))<0)
	{
		printf("Send %d to %d failed %d \n", globalMyID, dest, neighbour[dest]);
	}
	else
	{
		//printf("Send %d to %d ok \n", globalMyID, dest);
	}
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	int count = 1;
	int refresh_count = 5; // manually do a refresh every 10 announcement
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	hackyBroadcast("HEREIAM", 7); // announcement of a new node
	nanosleep(&sleepFor, 0);
	while(1)
	{
		hackyBroadcast("HELLO", 5);
		nanosleep(&sleepFor, 0);
		//count++;
	}
}

void* forwardToNeighbors(void* unusedParam)
{
	// forward the new message to my neighbors
	while(1)
	{
		if(forwardFlag){
			//printf("%d forward broadcast from %d\n", globalMyID, announcementFrom);
			//pthread_mutex_lock(&forwardFlag_m);
			for(int i = 0; i < graphSize; i++){
				//if(neighbour[i] && i != announcementFrom && i != announcementSource){
				if(i != announcementFrom && i != announcementSource){
					// neighbor and it is not the one that sends the message
					forward(announcementBuf, i, announcementBufLength);
					//printf("forward to %d buf len %d, end %s\n", i, announcementBufLength, announcementBuf + announcementBufLength);
				}
			}
			//nanosleep(&sleepFor, 0);
			forwardFlag = 0;
			//pthread_mutex_unlock(&forwardFlag_m);
		}
		
	}
}

void* broadcastToNeighbors(void* unusedParam)
{
	// broadcast to my neighbors
	char message[graphSize] = {"\0"};
	int length;
	broadcastFlag = 0;
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		if(1){
			//printf("%d forward broadcast from %d\n", globalMyID, announcementFrom);

			length = encodeNeighbour(message);
			for(int i = 0; i < graphSize; i++){
				if(i != globalMyID && neighbour[i]){
					forward(message, i, length);
				}
			}
			//printf("%d brdc with seq %d\n", globalMyID, broadcastSequence);
			nanosleep(&sleepFor, 0);
			pthread_mutex_lock(&broadcastFlag_m);
			//broadcastFlag = 0;
			broadcastSequence++;
			pthread_mutex_unlock(&broadcastFlag_m);
		}
		
	}
}

void printNeighbour(){
	// print the neighbours
	for(int i = 0; i < graphSize; i++){
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
	/*
	int count = 0;
	for(int i = 0; i < 255; i++){
		for(int j = i + 1; j < 256; j++){
			if(network[i][j]){
				//printf("%dn%dc%d ", i, j, network[i][j]);
			}
		}
	}
	printf("\n");
	for(int i = 0; i < 256; i++){
		for(int j = i + 1; j < 256; j++){
			if(network[i][j]){
				printf("%d to %d, cost %d\n", i, j, network[i][j]);
			}
		}
	}
	printf("%d has connection to %d nodes \n", globalMyID, count);
	*/
	int n = 3;
	int x, y;
	for(int i = 0; i < n * n; i++){
		x = i;//i / n * 10 + i % n;
		for(int j = 0; j < n * n; j++){
			y = j;//j / n * 10 + j % n;
			if(network[x][y]){
				printf("1 ");
			}
			else{
				printf("- ");
			}
		}
		printf("\n");
	}
	printf("\n");
}

typedef struct decodedMessage{
	// struct to help decode broadcast message
	int seq;
	int from;
	int fromCost[graphSize]; // the ith element is the cost between "from" and "i"
	int length; // length of the message	
} decodedMessage;

decodedMessage decode(char* message);

int encodeNeighbour(char* message){
	// encode the current neighbour information into a message
	// format:
	// "brdc 0 1 2 10 3 11 *"
	// The first number is the sequence number, the second is the source
	// the third is the destination, and the cost is the fourth. The fifth is the next desstination, etc
	// seq 0 cost between 1 and 2 is 10, between 1 and 3 is 11.
	// If no neigbours stop at from 1
	// modify the encoded "message"
     

	strcpy(message, "brdc");
	short int temp = htons((short int) broadcastSequence);
	memcpy(message + 4, &temp, sizeof(short int));
	temp = htons((short int) globalMyID);
	memcpy(message + 4 + sizeof(short int), &temp, sizeof(short int));
	int count = 2;
	for(short int i = 0; i < graphSize; i++){
		if(neighbour[i]){
			temp = htons((short int) i);
			memcpy(message + 4 + sizeof(short int) * count, &temp, sizeof(short int));
			count++;
			temp = htons((short int) cost[i]);
			memcpy(message + 4 + sizeof(short int) * count, &temp, sizeof(short int));
			count++;
		}
	}
	char end[2] = "*";
	memcpy(message + 4 + sizeof(short int) * count, end, 1);
	return 4 + count * sizeof(short int) + 1;
	//broadcastSequence++; // increment on the sequence
}

decodedMessage decode(char* message){
	// decode the broadcast message, return a decodeMessage type
	decodedMessage a = {0, 0, {0}, 0}; // initialization


	short int seq;
	memcpy(&seq, message+4, sizeof(short int));
	a.seq = ntohs(seq);
	memcpy(&seq, message+4+sizeof(short int), sizeof(short int));
	a.from = ntohs(seq);
	// parse cost pairs
	//printf("seq %d, from %d\n", a.seq, a.from);
	int i = 2;
	//printf("next %s\n", message + 4 + sizeof(short int) * i);
	//printf("test %d\n", strncmp(message + 4 + sizeof(short int) * i, "*", 1));
	while(1){
		if(!strncmp(message + 4 + sizeof(short int) * i, "*", 1)){
			//printf("find *\n");
			break; // end reached
		}
		else{
			memcpy(&seq, message+4+sizeof(short int)*i, sizeof(short int));
			short int to = ntohs(seq);
			i++;
			memcpy(&seq, message+4+sizeof(short int)*i, sizeof(short int));
			i++;
			a.fromCost[to] = ntohs(seq);
		}
	}
	a.length = 4 + sizeof(short int) * i + 1;
	return a;
}


typedef struct sendMessage{
	// parse the "send" message from the manager
	int destination;
	int nextDestination;
	char message[graphSize];
} sendMessage;

sendMessage parseSendMessage(char* message){
	// parse the send message from the manager
	// for example, send4hello
	//printf("in parseSendMessage\n");
	sendMessage a = {0, 0, "\0"};
	short int no_destID;
	memcpy(&no_destID, message+4, sizeof(short int));
	short int dest;
	dest = ntohs(no_destID);
	a.destination = dest;
	//printf("dest: %d\n", dest);
	char* token = message + 4 + sizeof(short int);
	strcpy(a.message, token);
	//printf("msg: %s\n", a.message);
	a.nextDestination = nextHop(a.destination);
	return a;
}

void announceChanges(int n){
	// send out the broadcast
	char message[graphSize] = {"\0"};
	if(!broadcastSequenceFrom[globalMyID]){
		broadcastSequenceFrom[globalMyID] = 1;
		broadcastSequence = 1;
	}
	int length = encodeNeighbour(message);
	hackyBroadcast(message, length);
}


void checkTimeOut(){
	// check whether the neighbour has been lost
	// time interval is 1 s
	struct timeval current_time;
	int timeout = 2;
	gettimeofday(&current_time, NULL);
	for(int i = 0; i < graphSize; i++){
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

void dijkstra(int print){
	// Dijkstra algorithm
	// Tie breaking is implemented

	int INFINITY = 9999;
	int n = graphSize;
	int start = globalMyID;
	int Graph[graphSize][graphSize] = {0};
	int distance[n];
	//int pred[n];
	int visited[n];
	int count, mindistance, nextnode;
	// this array is used to break ties, it stores the first hop for this path.
	// we alwayse choose the small firstHop value patch if the path length is the same
	int firstHop[graphSize] = {0}; 
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
		if(Graph[start][i] < INFINITY){
			firstHop[i] = i; // directly connected node
		}
		else{
			firstHop[i] = INFINITY; // no hop yet
		}
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
		//if(print){
		//	printf("count %d, choose %d, min_d %d, 1st_hop %d\n", count, nextnode, mindistance, firstHop[nextnode]);
		//}
		// update the map
		visited[nextnode] = 1;
		for(int i = 0; i < n; i++){
			if(!visited[i]){
				// if the path lengths are equal, update if the first hop is smaller
				if(mindistance + Graph[nextnode][i] < distance[i] || ((mindistance + Graph[nextnode][i] == distance[i]) && (firstHop[nextnode] < firstHop[i]))){
					distance[i] = mindistance + Graph[nextnode][i];
					pred[i] = nextnode;
					firstHop[i] = firstHop[nextnode]; // share the same first hop
				}
			}
		}
		count ++;
	}
}

int nextHop(int destination){
	// return the next hop for the destination
	// if a path does not exit, return -1
	for(int i = 0; i < graphSize; i++){
		if(pred[destination] != globalMyID){
			destination = pred[destination];
		}
		else{
			break;
		}
	}
	if(network[globalMyID][destination]){
		return destination;
	}
	else{
		return -1;
	}
}

void forwardBroadcast(char* buf, int fromNode, int source, int length){
	// forward the broadcast message to the rest of my neighbors
	// arguments, buffer, the node that send me the message, the original node that send the message
	
	for(int i = 0; i < graphSize; i++){
		if(neighbour[i] && i != fromNode && i != source){
			// neighbor and it is not the one that sends the message
			forward(buf, i, length);
		}
	}

}

int packNetwork(char* message){
	// input: an initialized char pointer to store the network information
	// return: the length of the buf
	int shortIntCount = 0;
	//printf("%d dumps\n", globalMyID);
	strcpy(message, "dump");
	for(int i = 0; i < graphSize; i++){
		for(int j = i + 1; j < graphSize; j++){
			short int temp;
			if(network[i][j]){
				temp = htons((short int) i);
				memcpy(message + 4 + shortIntCount * sizeof(short int), &temp, sizeof(short int)); //insert i
				shortIntCount++;
				temp = htons((short int) j);
				memcpy(message + 4 + shortIntCount * sizeof(short int), &temp, sizeof(short int)); //insert j
				shortIntCount++;
				temp = htons((short int) network[i][j]);
				memcpy(message + 4 + shortIntCount * sizeof(short int), &temp, sizeof(short int)); //insert j
				shortIntCount++;
				//printf("%d to %d cost %d\n", i, j, network[i][j]);
			}
		}
	}
	char end[2] = "*"; // end with *
	memcpy(message + 4 + shortIntCount * sizeof(short int), end, 1);
	return 4 + shortIntCount * sizeof(short int) + 1;
}

void updateNetworkFromDump(char* message){
	// update the network based on the information received from the neighbor's dump
	int i = 0;
	short int seq;
	short int from;
	short int to;
	short int cost;
	//printf("%d received dump\n", globalMyID);
	//printf("%s\n", message);
	while(1){
		if(!strncmp(message + 4 + sizeof(short int) * i, "*", 1)){
			//printf("find *\n");
			break; // end reached
		}
		else{ // update network
			memcpy(&seq, message + 4 + sizeof(short int) * i, sizeof(short int));
			from = ntohs(seq);
			i++;
			memcpy(&seq, message + 4 + sizeof(short int) * i, sizeof(short int));
			to = ntohs(seq);
			i++;
			memcpy(&seq, message + 4 + sizeof(short int) * i, sizeof(short int));
			i++;
			cost = ntohs(seq);
			network[from][to] = cost;
			network[to][from] = cost;
			//printf("%d to %d cost %d\n", from, to, cost);
		}
	}
}

void listenForNeighbors(char* logFile)
{
	FILE *fp;
	fp = fopen(logFile, "w");
	//printf("listen...\n");
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000]={'\0'};

	int bytesRecvd;
	while(1)
	{
		struct timeval run_time;
		int t, t1;
		
		memset(recvBuf, '\0', 1000); // reset memory of recvBuf
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		//printf("Received: %s\n", recvBuf);
		
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
				//printf("Found new neighbour\n");
				broadcastSequence++; // increment on the sequence
				broadcastSequenceFrom[globalMyID] = broadcastSequence;
				//broadcastFlag = 1;
				//announceChanges(heardFrom);
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
			//dijkstra(1); // for debug use only
			//gettimeofday(&run_time, 0);
			//t = run_time.tv_sec * 1000000 + run_time.tv_usec ;
			dijkstra(0);
			//gettimeofday(&run_time, 0);
			//t1 = run_time.tv_sec * 1000000 + run_time.tv_usec;
			//printf("dijkstra used %d us\n", t1-t);
			char rawBuf[512] = {"\0"}; 
			char logLine[512] = {"\0"};
			//printf("%d received from %d\n", globalMyID, heardFrom);
			strcpy(rawBuf, recvBuf);
			//gettimeofday(&run_time, 0);
			//t = run_time.tv_sec * 1000000 + run_time.tv_usec;
			sendMessage m = parseSendMessage(recvBuf);
			//gettimeofday(&run_time, 0);
			//t1 = run_time.tv_sec * 1000000 + run_time.tv_usec;
			//printf("parse used %d us\n", t1-t);
			//printf("Heard from: %d\n", heardFrom);
			if(heardFrom < 0){// message from the manager
				sprintf(logLine, "sending packet dest %d nexthop %d message %s\n", m.destination, m.nextDestination, m.message);
				printf("%d sending packet dest %d nexthop %d message %s\n", globalMyID, m.destination, m.nextDestination, m.message);
				//gettimeofday(&run_time, 0);
				//t = run_time.tv_sec * 1000000 + run_time.tv_usec;			
				forward(recvBuf, m.nextDestination, 4 + sizeof(short int) + strlen(m.message));
				//gettimeofday(&run_time, 0);
				//t1 = run_time.tv_sec * 1000000 + run_time.tv_usec;
				//printf("forward used %d us\n", t1-t);
			}
			else if(m.destination == globalMyID){
				sprintf(logLine, "receive packet message %s\n", m.message);
				printf("%d receive packet message %s\n", globalMyID, m.message);
			}
			else if(m.nextDestination == -1){
				sprintf(logLine, "unreachable dest %d\n", m.destination);
				printf("%d unreachable dest %d\n", globalMyID, m.destination);
			}
			else{
				sprintf(logLine, "forward packet dest %d nexthop %d message %s\n", m.destination, m.nextDestination, m.message);
				printf("%d forward packet dest %d nexthop %d message %s\n", globalMyID, m.destination, m.nextDestination, m.message);
				/*char networkBuf[512] = {"\0"};
				int length = packNetwork(networkBuf); // pack the network information into networkBuf
				forward(networkBuf, m.nextDestination, length);*/
				forward(recvBuf, m.nextDestination, 4 + sizeof(short int) + strlen(m.message));
			}	
			

			int n = 3;
			int x, y;
			/*for(int i = 0; i < n * n; i++){
				x = i;//i / n * 10 + i % n;
				for(int j = 0; j < n * n; j++){
					y = j;//j / n * 10 + j % n;
					if(network[x][y]>0){
						strcat(logLine, "1 ");
						//sprintf(logLine, "1 ");
					}
					else{
						strcat(logLine, "- ");
						//printf(logLine, "- ");
					}
				}
				strcat(logLine, "\n");
				//printf(logLine, "\n");
			}*/

			fprintf(fp, "%s", logLine);
			fflush(fp);
			
			//
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(recvBuf, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
		}
		
		//TODO now check for the various types of packets you use in your own protocol
		else if(!strncmp(recvBuf, "brdc", 4))
		{
			// receive broadcast from another node
			pthread_mutex_lock(&forwardFlag_m);
			struct decodedMessage broadcast_message = decode(recvBuf);
			//printf("%d recv brdc from %d with seq %d\n", globalMyID, broadcast_message.from, broadcast_message.seq);
			if(broadcast_message.seq > broadcastSequenceFrom[broadcast_message.from]){
				// new announcement to me, update my knowledge
				broadcastSequenceFrom[broadcast_message.from] = broadcast_message.seq;
				// update the network connected to "from"
				for(int i = 0; i< graphSize; i++){
					int fromCost = broadcast_message.fromCost[i];
					if(fromCost>0){//broadcast_message.fromCost[i]){// if connected
						network[broadcast_message.from][i] = fromCost;//broadcast_message.fromCost[i];
						network[i][broadcast_message.from] = fromCost;//broadcast_message.fromCost[i];
					}
                }
				announcementFrom = heardFrom;
				announcementSource = broadcast_message.from;
				announcementBufLength = broadcast_message.length;
				memcpy(announcementBuf, recvBuf, announcementBufLength);
				//Henan //forwardFlag = 1; // forward the announcement
				//printTopology();
				forwardBroadcast(announcementBuf, heardFrom, broadcast_message.from, announcementBufLength);
			}
			pthread_mutex_unlock(&forwardFlag_m);
			// forward the broadcast
			//forwardBroadcast(rawRecvBuf, heardFrom, broadcast_message.from);
			//hackyBroadcast(rawRecvBuf, strlen(rawRecvBuf));
		}
		else if(!strncmp(recvBuf, "HEREIAM", 7)){
			// found a new node, dump my information to it
			// send out what I know from network[i][j]
			
			char networkBuf[512] = {"\0"};
			//gettimeofday(&run_time, 0);
			//t = run_time.tv_sec * 1000000 + run_time.tv_usec;
			int length = packNetwork(networkBuf); // pack the network information into networkBuf
			//gettimeofday(&run_time, 0);
			//t1 = run_time.tv_sec * 1000000 + run_time.tv_usec;
			//printf("dump pack used %d us\n", t1-t);
			//gettimeofday(&run_time, 0);
			//t = run_time.tv_sec * 1000000 + run_time.tv_usec;
			forward(networkBuf, heardFrom, length);
			//gettimeofday(&run_time, 0);
			//t1 = run_time.tv_sec * 1000000 + run_time.tv_usec;
			//printf("forward dump used %d us\n", t1-t);
		}
		else if(!strncmp(recvBuf, "dump", 4) && !dumpReceived){
			// received dump message from the neighbor, update my network
			
			//printf("%d recv dmp from %d\n", globalMyID, heardFrom);
			//gettimeofday(&run_time, 0);
			//t = run_time.tv_sec * 1000000 + run_time.tv_usec;
			updateNetworkFromDump(recvBuf);
			//gettimeofday(&run_time, 0);
			//t1 = run_time.tv_sec * 1000000 + run_time.tv_usec;
			//printf("update dump used %d us\n", t1-t);
			dumpReceived = 1;
		}
		//checkTimeOut();
		// ... 
	}
	//(should never reach here)
	close(globalSocketUDP);
}


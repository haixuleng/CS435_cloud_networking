/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "8000" // the port client will be connecting to 

#define MAXDATASIZE 255 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int getSplitStr(char* inp, char* host, char* dir, char* port)
{
/*
 * split the input argument into host name and file path
 * makes sure the name contains http
 *
 */
        const char *delim = "/";
        const char start[] = "http:";
        char *token = strtok(inp, delim);
        int count = 2;

        if(strcmp(token, start) != 0){
                //printf("INVALIDPROTOCOL\n");
                return -1;
        }
        token = strtok(NULL, delim);
        while(token){
                if(count == 2){
                        strcpy(host, token);
                }
                else{
                        char temp[50];
                        strcpy(temp, token);
                        strcat(dir, temp);
                        strcat(dir, "/");
                }
                count++;
                token = strtok(NULL, delim);//printf("%s\n", token);
        }
        dir[strlen(dir)-1] = '\0';

        const char *port_delim = ":";
        char host_copy[50];
        strcpy(host_copy, host);
        token = strtok(host, port_delim);
        strcpy(host, token);
        token = strtok(NULL, port_delim);
        if(token != NULL){
                strcpy(port, token);
        }
        else{
                strcpy(port, "80");
        }
        return 1;
}

int check404(char *buf){
// check whether the header contains 404 File not found
// if so return -1
	//printf("%s", strstr(buf, "404 File not found"));
	if(strstr(buf, "404 File not found") == NULL){
		return -1;
	}
	else{
		return 1;
	}
}

int checkText(char *buf){
// check what is the type of the content, whether it is a text file
	if(strstr(buf, "Content-type: text") == NULL){
		return -1;
	}
	else{
		return 1;
	}

}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char dir[50] = "/", message[500];
        char host[50], port[50];	
	FILE *fp, *fb;

	fp = fopen("output", "w+");

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	if(getSplitStr(argv[1], host, dir, port) < 0){
		// the input is not correct
		fprintf(fp, "INVALIDPROTOCOL");
		fclose(fp);
		exit(1);
	}
	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		//fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		fprintf(fp, "NOCONNECTION");
		fclose(fp);
		exit(1);
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			//perror("client: socket");
			fprintf(fp, "NOCONNECTION");
			fclose(fp);
			exit(1);
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			//perror("client: connect");
			fprintf(fp, "NOCONNECTION");
			fclose(fp);
			exit(1);
			continue;
		}

		break;
	}

	if (p == NULL) {
		//fprintf(stderr, "client: failed to connect\n");
		fprintf(fp, "NOCONNECTION");
		fclose(fp);
		exit(1);
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	//printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure
	
	//message = "GET /test.txt HTTP/1.0\r\n\r\n";
	//concatenate the http request as a string
	strcpy(message, "GET ");
	strcat(message, dir);
	strcat(message, " HTTP/1.0\r\n\r\n");
        if( send(sockfd , message , strlen(message) , 0) < 0)
        {
                puts("Send failed");
                return 1;
        }
	
	// start to write to the output, it seems that the header and the content
	// are split into two transfers.
	int write_flag = 0;
	int count = 0;
	int text_flag = 1;
	while((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) > 0){
		buf[numbytes] = '\0';
		//printf("nb: %d\n", numbytes);
		//printf("sz: %d\n", (int) sizeof(buf));
		if(count == 0){
			// first check whether the file exist
		       	if(check404(buf) > 0){
				fprintf(fp, "FILENOTFOUND");
				fclose(fp);
				exit(1);
			}
			// check to see whether the file is a binary file or not
			if(checkText(buf) > 0){
				text_flag = 1;
			}	
			else{
				// for binary file case, use fb to write instead of fp
				text_flag = 0;
				fclose(fp);
				fb = fopen("output", "wb");
			}
			char *ret = strstr(buf, "\r\n\r\n");
			ret = ret + strlen("\r\n\r\n");
			if(text_flag > 0){
				fprintf(fp, "%s", ret);
			}
			else{
				// starts from 0, also skip the last byte
				fwrite(ret, 0, sizeof(ret) - 1, fb);
			}
		}
		else{
			// for count > 0, there is no header information 
			if(text_flag > 0){
				fprintf(fp, "%s", buf);
			}
			else{
				// for binary case, use numbytes to confine the number of bytes
				fwrite(buf, 1, numbytes, fb);
			}
			// reset buf, may not be useful
			memset(buf, 0, sizeof buf);		
		}
		count = count + 1;
	
	}
	if(count == 0 && numbytes == 0){
		// special case, when the connection is made but the host is wrong
		fprintf(fp, "NOCONNECTION");
	}
	if(text_flag > 0){
		fclose(fp);
	}
	else{
		fclose(fb);
	}
	close(sockfd);

	return 0;
}


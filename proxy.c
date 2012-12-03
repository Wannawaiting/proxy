//
#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agentStr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *acceptStr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encodingStr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connectionStr = "Connection: close\r\n";
static const char *proxyConnectionStr = "Proxy-Connection: close\r\n";

void handleRequest(int *toClientFDPtr);
char *correctHeaders(rio_t *rp, char *buf, char *host, char *portStr);
void clienterror(int fd, char *cause, char *errnum, 
				char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
	int listenfd, *connfdPtr;
	unsigned int clientlen;
    struct sockaddr_in clientaddr;
	pthread_t tid;
	
	if(argc != 2) {
		printf("bad params\n");
		exit(0);
	}
	
	signal(SIGPIPE, SIG_IGN);
	
    listenfd = Open_listenfd(atoi(argv[1]));
    while (1) {
		clientlen = sizeof(clientaddr);
		connfdPtr = Malloc(sizeof(int));
		*connfdPtr = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		printf("connection found...\n");
		//handleRequest(connfd);
		Pthread_create(&tid, NULL, handleRequest, connfdPtr);
		//printf("closing connection...\n");
		//Close(connfd);
    }
	return 0;
}

void handleRequest(int *toClientFDPtr) {
	Pthread_detach(pthread_self());
	int toClientFD = *toClientFDPtr;
	free(toClientFDPtr); //we can free early since it just stores a primative
	
	int toServerFD;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], 
			version[MAXLINE], hostname[MAXLINE], portStr[6]; //max port length is 5 characters
	hostname[0] = '\0'; portStr[0] = '\0';
    rio_t clientRIO, serverRIO;
	
	char *request;
	
  
    /* Read request line and headers */
    Rio_readinitb(&clientRIO, toClientFD);
    Rio_readlineb(&clientRIO, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) { 
        clienterror(toClientFD, method, "501", "Not Implemented",
                "Proxy does not implement this method");
        return;
    }
	
    request = correctHeaders(&clientRIO, buf, hostname, (char *) portStr); //remember to free later
	printf("host: %s| port: %s\n", hostname, portStr);
	printf("correctedHeader:\n%s", request);
	if(strlen(portStr) == 0) {
		portStr[0] = '8';
		portStr[1] = '0';
		portStr[2] = '\0';
	}
	
	toServerFD = Open_clientfd(hostname, portStr);
	
	// read from server and write to client only if valid connection to server
	if(toServerFD >= 0) {
		Rio_readinitb(&serverRIO, toServerFD);
		
		//send request
		Rio_writen(toServerFD, request, strlen(request));
		
		//get response and immediatly write it to the client
		int bufLen;
		while((bufLen = Rio_readnb(&serverRIO, buf, MAXLINE)) > 0) {
			Rio_writen(toClientFD, buf, bufLen);
		}
		Close(toServerFD);
	}
	
	Close(toClientFD);
	free(request);
	printf("connection closed...\n");
	return;
}

char *correctHeaders(rio_t *rp, char *buf, char *host, char *portStr) {
	int resultActLen = strlen(buf)+strlen(acceptStr)+strlen(accept_encodingStr)
			+strlen(user_agentStr)+strlen(proxyConnectionStr)
			+strlen(connectionStr)+2; //+2 for /r/n
	int resultAllocLen = 0;
	char *result = (char *) malloc(sizeof(char)*(resultActLen+1)); //+1 for null term
	result[resultAllocLen] = '\0';
	
	strcpy(result, buf); //buf is the get request line
	strcat(result, user_agentStr);
	strcat(result, acceptStr);
	strcat(result, accept_encodingStr);
	strcat(result, proxyConnectionStr);
	strcat(result, connectionStr);
	resultAllocLen = resultActLen-2;
	
	
	
    while(strcmp(buf, "\r\n") && strcmp(buf, "\n")) {
		char key[MAXLINE], value[MAXLINE], *append;
		
		Rio_readlineb(rp, buf, MAXLINE);
		sscanf(buf, "%s %s", key, value);
		if(!strcasecmp(key, "cookie:") || !strcasecmp(key, "proxy-connection:") 
			|| !strcasecmp(key, "connection:") || !strcasecmp(key, "accept-encoding:")
			|| !strcasecmp(key, "accept:") || !strcasecmp(key, "user-agent:")) 
		{append = "";}
		else {
			append = buf;
			if(!strcasecmp(key, "host:")) {
				char *portStrTmp, *hostTmp;
				hostTmp = strtok_r(value, ":", &portStrTmp);
				if(strlen(hostTmp) == 0) {strcpy(host, value);}
				else {strcpy(host, hostTmp);}
				if(strlen(portStrTmp) == 0) {strcpy(portStr, "80");}
				else {strcpy(portStr, portStrTmp);}
			}
		}
		
		int sizeNeeded = resultAllocLen + strlen(append);
		if(sizeNeeded >= resultActLen) {
			int resizeSize = 2*resultAllocLen;
			if(sizeNeeded > resizeSize) {resizeSize = sizeNeeded;}
			result = (char *) realloc(result, sizeof(char)*(resizeSize+1)); //+1 for null terminator
			resultActLen = resizeSize;
		}
		
		strcat(result, append);
		resultAllocLen = sizeNeeded;
    }
	
	//end request
	
	if(resultAllocLen+2 < resultActLen) {//downsize if necessary, +2 because we are saving space of '\r\n'
		result = (char *) realloc(result, sizeof(char)*(resultAllocLen+3)); //+3 for null terminator and EOF flag ('\r\n')
	}
	result[resultAllocLen] = '\r'; result[resultAllocLen+1] = '\n'; result[resultAllocLen+2] = '\0';
	
    return result;
}


//from tiny.c
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}



//
#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"
#include "cache2.c"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define DEBUG(X) X
#define DEBUG1(X) X


// global variables
Cache cache;
static const char *user_agentStr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *acceptStr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encodingStr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connectionStr = "Connection: close\r\n";
static const char *proxyConnectionStr = "Proxy-Connection: close\r\n";

//function prototypes
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
	cache = newCache(1, MAX_OBJECT_SIZE, MAX_CACHE_SIZE);
	
    listenfd = Open_listenfd(atoi(argv[1]));
    while (1) {
		clientlen = sizeof(clientaddr);
		connfdPtr = Malloc(sizeof(int));
		*connfdPtr = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		printf("connection found...\n");
		Pthread_create(&tid, NULL, handleRequest, connfdPtr);
		//handleRequest(connfdPtr);
    }
	
	freeCache(cache);
	return 0;
}

void handleRequest(int *toClientFDPtr) {
	Pthread_detach(Pthread_self());
	int toClientFD = *toClientFDPtr;
	free(toClientFDPtr); //we can free early since it just stores a primative
	
	int toServerFD;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], 
			version[MAXLINE], hostname[MAXLINE], portStr[6]; //max port length is 5 characters
	hostname[0] = '\0'; portStr[0] = '\0';
    rio_t clientRIO, serverRIO;
	char *request;
	int responseAllocLen = MAX_OBJECT_SIZE;
	int responseLen = 0;
	char *response = malloc(responseAllocLen);
	int cacheResponse = 1; //bool weither or not to cache the response

	
  
    /* Read request line and headers */
    Rio_readinitb(&clientRIO, toClientFD);
    Rio_readlineb(&clientRIO, buf, MAXLINE); //read the get request
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) { 
        clienterror(toClientFD, method, "501", "Not Implemented",
                "Proxy does not implement this method");
        return;
    }
	
    request = correctHeaders(&clientRIO, buf, hostname, (char *) portStr); //remember to free later
	//DEBUG(printf("host: %s| port: %s\n", hostname, portStr);)
	//DEBUG(printf("correctedHeader:\n%s", request);)
	
	//try to find request in cache
	if((responseLen = readCache(cache, request, (void **) &response)) > 0) {
		DEBUG1(printf("----use cache---\n");)
		DEBUG(printf("found in cache: \n");)
		DEBUG(printf("%s\n", response);)
		Rio_writen(toClientFD, response, responseLen); //we will need a response length field
		cacheResponse = 0;
	}
	else {
		DEBUG1(printf("----not using cache---\n");)
		
		toServerFD = Open_clientfd(hostname, portStr);
		
		// read from server and write to client only if valid connection to server
		if(toServerFD >= 0) {
			Rio_readinitb(&serverRIO, toServerFD);
			
			//send request
			Rio_writen(toServerFD, request, strlen(request));
			
			//create responseBuf of size 102400
					
			//get response and immediatly write it to the client
			int bufLen;
			while((bufLen = Rio_readnb(&serverRIO, buf, MAXLINE)) > 0) {
				Rio_writen(toClientFD, buf, bufLen);
				//printf("buf: %s\n", buf);
				//write to responseBuf for caching later, and if no space left, do not cache
				if(cacheResponse != 0) {
					int newResponseLen = responseLen+bufLen+1;
					if(newResponseLen <= responseAllocLen) {
						DEBUG(printf("copying: %s, bufLen: %d to response(%x)+responseLen(%x) = %x\n", buf, bufLen, response, responseLen, response+responseLen);)
						memcpy((void *)(response+responseLen+1), buf, bufLen+1);
						responseLen = newResponseLen;
						DEBUG(printf("copied: %s\n", response);)
					}
					else {
						DEBUG1(printf("data to large\n");)
						cacheResponse = 0;
					}
				}
			}
			Close(toServerFD);
			
			if(cacheResponse != 0) {
				//downsize responseBuf
				response = realloc(response, responseLen);
			
				//cache responseBuf, do not free it
				writeCache(cache, request, (void *) response, responseLen);
				DEBUG1(printf("-----wrote into cache--------\n");)
				DEBUG(readCache(cache, request, (void **) &response);)
				DEBUG(printf("cached: %s\n", response);)
				//exit(0);
			}
		}
	}
	
	free(response);
	free(request);
	Close(toClientFD);
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
		append = "";
		/*if(!strcasecmp(key, "cookie:") || !strcasecmp(key, "proxy-connection:") 
			|| !strcasecmp(key, "connection:") || !strcasecmp(key, "accept-encoding:")
			|| !strcasecmp(key, "accept:") || !strcasecmp(key, "user-agent:")) 
		{append = "";}*/
		if(!strcasecmp(key , "host:") || !strcasecmp(key, "accept-language:")){
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



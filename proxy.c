/***
 * Tomer Borenstein (tborenst) Vansi Vallabhanini (vvallabh)
 * General Comment:
 * We do have some memory leaks that we haven't been able to fix.
 * After a long bug hunt and help from the TAs we decided to write this
 * comment. The problem occures on line 149 - if we do not free the response
 * we end up having #of_threads_spawned "definitely lost" blocks when we 
 * checked with valgrind. This suggested that we lost one block per thread,
 * and that we should free response on line 149. However, if we did free
 * on line 149, we would get "invalid write" errors when ran with valgridn
 * and segfault when we run without valgrind. We tried to look for a place
 * where we may have double freed the response, but we couldn't come up
 * with a solution to this. 
 *
 * Thanks for reading!
 */
#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"
#include "cache2.c"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define DEBUG(X) //X
#define DEBUG1(X) //X


// global variables
Cache cache;
//these static constants were given to us above 80 characters per line
//in the starter code, so we didn't change it...
static const char *user_agentStr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *acceptStr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encodingStr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connectionStr = "Connection: close\r\n";
static const char *proxyConnectionStr = "Proxy-Connection: close\r\n";
int maxThreads = 16;

//function prototypes
void sigintHandler(int sig);
void handleRequest(int *toClientFDPtr);
char *correctHeaders(rio_t *rp, char *buf, char *host, char *portStr);
void clienterror(int fd, char *cause, char *errnum, 
				char *shortmsg, char *longmsg);

/**
 * Frees the cash when the program is interrupted
 */
void sigintHandler(int sig) {
	freeCache(cache);
	exit(0);
}
				
int main(int argc, char **argv) {
	int listenfd, *connfdPtr;
	unsigned int clientlen;
    struct sockaddr_in clientaddr;
	pthread_t tid;
	
	if(argc != 2) {
		printf("bad params\n");
		exit(0);
	}
	
	//handle signals
	Signal(SIGPIPE, SIG_IGN);
	Signal(SIGTSTP, sigintHandler);

	cache = newCache(10, MAX_OBJECT_SIZE, MAX_CACHE_SIZE);
	
    listenfd = Open_listenfd(atoi(argv[1]));
    while (1) {
		clientlen = sizeof(clientaddr);
		connfdPtr = Malloc(sizeof(int));
		*connfdPtr = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Pthread_create(&tid, NULL, handleRequest, connfdPtr);
    }
	
	return 0;
}

/*
 * Get request from client, send it to a server, return response to client
 */
void handleRequest(int *toClientFDPtr) {
	Pthread_detach(Pthread_self());
	int toClientFD = *toClientFDPtr;
	free(toClientFDPtr); //we can free early since it just stores a primative
	
	int toServerFD;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], 
			version[MAXLINE], hostname[MAXLINE], portStr[6]; 
	hostname[0] = '\0'; portStr[0] = '\0';
    rio_t clientRIO, serverRIO;
	char *request;
	int responseAllocLen = MAX_OBJECT_SIZE-1;
	int responseLen = 0;
	char *response = Malloc(responseAllocLen);
	void *response2 = response+1;
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
	
	//fix headers in request to conform to our requirements
    request = correctHeaders(&clientRIO, buf, hostname, (char *) portStr);
	
	//try to find request in cache
	if((responseLen = readCache(cache, request, (void **) &response2)) > 0) {
		response[0] = 'H';
		Rio_writen(toClientFD, response, responseLen); 
		cacheResponse = 0;
	}
	else {
		//not found, ask server for response
		toServerFD = Open_clientfd(hostname, portStr);
		
		// read from server, write to client only if valid connection to server
		if(toServerFD >= 0) {
			Rio_readinitb(&serverRIO, toServerFD);
			
			//send request
			Rio_writen(toServerFD, request, strlen(request));
			
			//get response and immediatly write it to the client
			int bufLen;
			while((bufLen = Rio_readnb(&serverRIO, buf, MAXLINE)) > 0) {
				Rio_writen(toClientFD, buf, bufLen);
				//write to responseBuf for caching later, 
				//and if no space left, do not cache.
				if(cacheResponse != 0) {
					int newResponseLen = responseLen+bufLen;
					if(newResponseLen <= responseAllocLen) {
						void *resWritePos = (response+responseLen);
						memcpy((char *)resWritePos, buf, bufLen);
						responseLen = newResponseLen;	
					}
					else {
						cacheResponse = 0;
					}
				}
			}
			Close(toServerFD);
			
			if(cacheResponse != 0) {
				writeCache(cache, request, (void *) response, responseLen);
			}
		}
	}
	
	//see top of file to understand why this is commented out
	//free(response);
	free(request);
	Close(toClientFD);
	return;
}

/* 
 * Change a request's headers to conform to our requirements
 */
char *correctHeaders(rio_t *rp, char *buf, char *host, char *portStr) {
	int resultActLen = strlen(buf)+strlen(acceptStr)+strlen(accept_encodingStr)
			+strlen(user_agentStr)+strlen(proxyConnectionStr)
			+strlen(connectionStr)+2; //+2 for /r/n
	int resultAllocLen = 0;
	char *result = (char *) Malloc(sizeof(char)*(resultActLen+1)); //+1 '\0'
	result[resultAllocLen] = '\0';
	
	strcpy(result, buf); //buf is the get request line
	strcat(result, user_agentStr);
	strcat(result, acceptStr);
	strcat(result, accept_encodingStr);
	strcat(result, proxyConnectionStr);
	strcat(result, connectionStr);
	resultAllocLen = resultActLen-2;
	
	//read response and correct the headers when you encounter them
    while(strcmp(buf, "\r\n") && strcmp(buf, "\n")) {
		char key[MAXLINE], value[MAXLINE], *append;
		
		Rio_readlineb(rp, buf, MAXLINE);
		sscanf(buf, "%s %s", key, value);
		append = "";

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
			result = (char *) Realloc(result, sizeof(char)*(resizeSize+1));
			resultActLen = resizeSize;
		}
		
		strcat(result, append);
		resultAllocLen = sizeNeeded;
    }
	
	//end the request
	
	//make sure we have the right size
	if(resultAllocLen+2 < resultActLen) {
		//+3 for null terminator and EOF flag ('\r\n')
		result = (char *) Realloc(result, sizeof(char)*(resultAllocLen+3));
	}

	result[resultAllocLen] = '\r'; 
	result[resultAllocLen+1] = '\n'; 
	result[resultAllocLen+2] = '\0';
	
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



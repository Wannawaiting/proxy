//
#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agentStr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *acceptStr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encodingStr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connectionStr = "Connection: close\r\n";
static const char *proxyConnectionStr = "Proxy-Connection: close\r\n";

void handleRequest(int fd);
char *correctHeaders(rio_t *rp, char *buf, char *host);
void clienterror(int fd, char *cause, char *errnum, 
				char *shortmsg, char *longmsg);

int main()
{
	int listenfd, connfd;
	unsigned int clientlen;
    struct sockaddr_in clientaddr;
	
    listenfd = Open_listenfd(18845);
    while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		printf("connection found...\n");
		handleRequest(connfd);
		printf("closing connection...\n");
		Close(connfd);
    }
	return 0;
}

void handleRequest(int toClientFD) {
	int toServerFD;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], 
			version[MAXLINE], hostname[MAXLINE];
	hostname[0] = '\0';
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
	
    request = correctHeaders(&clientRIO, buf, hostname); //remember to free later
	
	printf("correctedHeader:\n%s", request);
	toServerFD = Open_clientfd(hostname, 80);
	
	Rio_readinitb(&serverRIO, toServerFD);
	
	//send request
	Rio_writen(toServerFD, request, strlen(request));
	
	//get response and immediatly write it to the client
	int bufLen;
	while((bufLen = Rio_readnb(&serverRIO, buf, MAXLINE)) > 0) {
		Rio_writen(toClientFD, buf, bufLen);
	}
	Close(toServerFD);
	
	free(request);
}

char *correctHeaders(rio_t *rp, char *buf, char *host) {
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
		Rio_readlineb(rp, buf, MAXLINE);
		char key[MAXLINE], value[MAXLINE];
		sscanf(buf, "%s %s", key, value);
		char *append;
		if(!strcasecmp(key, "cookie:") || !strcasecmp(key, "proxy-connection:") 
			|| !strcasecmp(key, "connection:") || !strcasecmp(key, "accept-encoding:")
			|| !strcasecmp(key, "accept:") || !strcasecmp(key, "user-agent:")) 
		{append = "";}
		else {
			append = buf;
			if(!strcasecmp(key, "host:")) {
				printf("found host: %s\n", value);
				strcpy(host, value);
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



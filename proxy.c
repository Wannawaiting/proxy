//
#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agentStr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *acceptStr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encodingStr = "Accept-Encoding: gzip, deflate\r\n";

void handleRequest(int fd);
char *correctHeaders(rio_t *rp, char *buf, char *host);
char *compileResponse(rio_t *rp);
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
		handleRequest(connfd); //return response and send back to client
		printf("closing connection...\n");
		Close(connfd);
    }
	return 0;
}

//like doit in
void handleRequest(int toClientFD) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], 
			version[MAXLINE], hostname[MAXLINE];
	hostname[0] = '\0';
    rio_t clientRIO, serverRIO;
	
	char *request, *response;
	
  
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
	//if(hostname == "") {hostname = parseURIForHost(uri);} //ask tas, i think strtok would work
	
	printf("correctedHeader: %s\n", request);
	int toServerFD = Open_clientfd(hostname, 80);
	
	//compile response
	Rio_readinitb(&serverRIO, toServerFD);
	Rio_writen(toServerFD, request, strlen(request));
	free(request);
	response = compileResponse(&serverRIO); //reuse buf
	
	Close(toServerFD);
	
	Rio_writen(toClientFD, response, strlen(response));
	printf("response: %s\n", response);
	free(response); //error is right here, can't figure out what it is though
}

char *compileResponse(rio_t *rp) {
	int responseActLen = 10;
	int responseAllocLen = 0;
	char *response = (char *) malloc(sizeof(char)*(10+1));
	response[responseAllocLen] = '\0';
	
	char buf[MAXLINE];
	
	while(Rio_readnb(rp, buf, MAXLINE) != 0) {//readnb returns MAXLINE-strlen(buf)		
		if(responseAllocLen >= responseActLen) {
			response = (char *) realloc(response, sizeof(char)*(2*responseAllocLen+1)); //+1 for null terminator
			responseActLen = 2*responseAllocLen;
		}
		
		strcat(response, buf);
		responseAllocLen += strlen(buf);
	}
	
	if(responseAllocLen < responseActLen) {//downsize if necessary
		response = (char *) realloc(response, sizeof(char)*(responseAllocLen+1)); //+1 for null terminator
		response[responseAllocLen] = '\0';
	}
	
	response[responseAllocLen] = '\0';
	return response;
}

char *correctHeaders(rio_t *rp, char *buf, char *host) {
	int resultActLen = 21*MAXLINE+2; //+2 for /r/n
	int resultAllocLen = 0;
	char *result = (char *) malloc(sizeof(char)*(resultActLen+1));
	result[resultAllocLen] = '\0';
	
	//printf("found buf: %s\n", buf);
    while(strcmp(buf, "\r\n") && strcmp(buf, "\n")) {
		//printf("asdf\n");
		Rio_readlineb(rp, buf, MAXLINE);
		//printf("found buf: %s\n", buf);
		char key[MAXLINE], value[MAXLINE];
		sscanf(buf, "%s %s", key, value);
		char *append;
		if(!strcasecmp(key, "user-agent:")) {
			append = (char *) user_agentStr;
		}
		else if(!strcasecmp(key, "accept:")) {
			append = (char *) acceptStr;
		}
		else if(!strcasecmp(key, "accept-encoding:")) {
			append = (char *) accept_encodingStr;
		}
		else {
			append = buf;
			if(!strcasecmp(key, "host:")) {
				printf("found host: %s\n", value);
				strcpy(host, value);
			}
		}
		
		
		int appendLen = strlen(append);
		if(resultAllocLen+2 >= resultActLen) { //+2 because we are saving space of '\r\n'
			result = (char *) realloc(result, sizeof(char)*(resultAllocLen+appendLen+1)); //+1 for null terminator
			resultActLen = resultAllocLen+appendLen;
		}
		
		strcat(result, append);
		resultAllocLen += appendLen;
    }
	
	//end request
	result[resultAllocLen] = '\r'; result[resultAllocLen+1] = '\n'; result[resultAllocLen+2] = '\0';
	
	if(resultAllocLen < resultActLen) {//downsize if necessary
		result = (char *) realloc(result, sizeof(char)*(resultAllocLen+1));
		result[resultAllocLen] = '\0';
	}
	
    return result;
}


//from tiny.c
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}



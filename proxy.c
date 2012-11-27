//
#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agentStr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *acceptStr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encodingStr = "Accept-Encoding: gzip, deflate\r\n";

void handleRequest(int fd);
char *correctHeaders(rio_t *rp, char *buf);
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
		Close(connfd);
    }
	return 0;
}

//like doit in
void handleRequest(int fd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    //char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
  
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) { 
       clienterror(fd, method, "501", "Not Implemented",
                "Proxy does not implement this method");
        return;
    }
	
    char *request = correctHeaders(&rio, buf); //remember to free later
	printf("correctedHeader: %s\n", request);
}

char *correctHeaders(rio_t *rp, char *buf) {
	int resultActLen = 21*MAXLINE+2; //+2 for /r/n
	int resultAllocLen = 0;
	char *result = (char *) malloc(sizeof(char)*(resultActLen+1));
	result[0] = '\0';
	
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
		else {append = buf;}
		
		int appendLen = strlen(append);
		if(resultAllocLen+2 >= resultActLen) { //+2 because we are saving space of '\r\n'
			result = (char *) realloc(result, resultAllocLen+appendLen+1); //+1 for null terminator
			resultActLen = resultAllocLen+appendLen;
		}
		
		strcat(result, append);
		resultAllocLen += appendLen;
    }
	
	//end request
	result[resultAllocLen] = '\r'; result[resultAllocLen+1] = '\n'; result[resultAllocLen+2] = '\0';
	
	if(resultAllocLen < resultActLen) {//downsize if necessary
		result = (char *) realloc(result, resultAllocLen+1);
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



#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "csapp.h"

#define DEBUGC(X) //X


/***********
 * Structs *
 ***********/
 
typedef struct Node * Node;
struct Node {
	Node next;
	char *req;
	void *res;
	int resSize;
};

//singly linked list
typedef struct LinkedList * LinkedList;
struct LinkedList {
	Node head;
	Node tail;
};

typedef struct Cache * Cache;
struct Cache {
	LinkedList *arr;
	int numRows;
	int cachedSize;
	int maxBlockSize;
	int maxCacheSize;
	int readCount;
	sem_t all;
	sem_t write;
};



/***********************
 * Function Prototypes *
 ***********************/

//Node
Node newNode(Node next, char *req, void *res, int resSize);
int freeNode(Node n);
void printNode(Node n);

//LinkedList
LinkedList newLinkedList(Node head, Node tail);
int freeLinkedList(LinkedList ll);
int evictLinkedList(LinkedList ll);
Node findNode(LinkedList ll, char *req, Node *retPrevNode);
void moveToHead(LinkedList ll, Node n, Node pN);

//Cache
Cache newCache(int numRows, int maxBlockSize, int maxCacheSize); 
int freeCache(Cache c);
void evictIfNecessary(Cache c, int llStartIdx, int evictSize);
int readCache(Cache c, char *req, void **retRes);
void writeCache(Cache c, char *req, void *res, int resSize);

//Util
unsigned long hash(char *str);
 

/******************
 * Node Functions *
 ******************/
 
//Node Constructor
Node newNode(Node next, char *req, void *res, int resSize) {
	Node n = (Node) Malloc(sizeof(struct Node));
	n->next = next;
	
	n->req = (char *) Calloc(strlen(req)+1, sizeof(char));
	strcpy(n->req, req);
	DEBUGC(printf("n->req: %s\n", n->req);)
	
	/*store response as void * because it can be binary data, 
	so this will avoid coder error*/
	n->res = (void *) Malloc(resSize);
	memcpy(n->res, res, resSize);
	//printf("newNode mmDiff: %d\n", memcmp(n->res, res, resSize));
	n->resSize = resSize;
	
	DEBUGC(printf("new Node: ");)
	DEBUGC(printNode(n);)
	return n;
}

//Node Destroyer
int freeNode(Node n) {
	if(n == NULL) {return -1;}
	
	/*do not free n->next since it is just a pointer which is 
	instantiated outside of this node's construction*/
	
	//DEBUGC(printf("req\n");)
	if(n->req != NULL) {free(n->req);}
	//DEBUGC(printf("res\n");)
	if(n->res != NULL) {free(n->res);}
	
	//DEBUGC(printf("freeSize\n");)
	int freeSize = n->resSize;
	
	//DEBUGC(printf("free n\n");)
	free(n);
	//DEBUGC(printf("return\n");)
	return freeSize;
}

void printNode(Node n) {
	printf("| node: %x, \n", n);
	//printf("next: %x, \n", n->next);
	//printf("req: %s, \n", n->req);
	//printf("res: %s, \n", (char *) n->res);
	//printf("resSize: %d |\n", n->resSize);
}

/************************
 * LinkedList Functions *
 ************************/
 
LinkedList newLinkedList(Node head, Node tail) {
	LinkedList ll = (LinkedList) Malloc(sizeof(struct LinkedList));
	ll->head = head;
	ll->tail = tail;
	return ll;
}

int freeLinkedList(LinkedList ll) {
	if(ll == NULL) {return -1;}
	DEBUGC(printf("ll: %x\n", ll);)
	int freeSize = 0;
	int count = 0;
	while(ll->head != NULL) {
		DEBUGC(printf("id: %d\n", count);)
		Node n = ll->head;
		ll->head = n->next;
		DEBUGC(printf("asdf n: %x\n", n);)
		freeSize += freeNode(n);
		count++;
	}
	
	DEBUGC(printf("free ll\n");)
	free(ll);
	return freeSize;
}

int evictLinkedList(LinkedList ll) {
	if(ll == NULL) {return -2;}
	DEBUGC(printf("evicting...\n");)
	int evictSize = -1;
	Node pTail = ll->head;
	while(pTail != NULL && pTail->next != ll->tail) {
		pTail = pTail->next;
	}
	
	if(ll->tail != NULL) {
		if(ll->tail == ll->head) {
			ll->head = NULL;
		}
		
		evictSize = freeNode(ll->tail);
		ll->tail = pTail;
		ll->tail->next = NULL;
	}
	//exit(0);
	return evictSize;
}

Node findNode(LinkedList ll, char *req, Node *retPrevNode) {
	if(ll == NULL || req == NULL || retPrevNode == NULL) {
		return NULL;
	}
	
	Node n = ll->head;
	*retPrevNode = NULL;
	DEBUGC(printf("searching: \n");)
	DEBUGC(printNode(n);)
	
	while(n != NULL && strcasecmp(req, n->req) != 0) {
		DEBUGC(printf("searching: ");)
		DEBUGC(printNode(n);)
		*retPrevNode = n;
		n = n->next;
	}
	
	return n;
}

void moveToHead(LinkedList ll, Node n, Node pN) {
	//notice for pN to be null, n is the head node due to findNode
	if(ll != NULL && n != NULL && pN != NULL && ll->head != n) {
		pN->next = n->next;
		if(ll->tail == n) {ll->tail = pN;}
		n->next = ll->head;
		ll->head = n;
	}
}


/*******************
 * Cache Functions *
 *******************/

Cache newCache(int numRows, int maxBlockSize, int maxCacheSize) {
	Cache c = (Cache) Malloc(sizeof(struct Cache));
	
	c->arr = (LinkedList *) Malloc(sizeof(LinkedList)*numRows);
	c->numRows = numRows;
	int r;
	for(r = 0; r < numRows; r++) {
		c->arr[r] = newLinkedList(NULL, NULL);
	}
	
	c->cachedSize = 0;
	c->maxBlockSize = maxBlockSize;
	c->maxCacheSize = maxCacheSize;
	
	Sem_init(&(c->all), 0, 1);
	Sem_init(&(c->write), 0, 1);
	c->readCount = 0;
	
	return c;
}
 
int freeCache(Cache c) {
	if(c == NULL) {return -1;}
	
	int freeSize = 0;
	if(c->arr != NULL) {
		int r;
		for(r = 0; r < c->numRows; r++) {
			if(c->arr[r] != NULL)
			{freeSize += freeLinkedList(c->arr[r]);}
		}
		free(c->arr);
	}
	free(c);
	return freeSize;
}

void evictIfNecessary(Cache c, int llStartIdx, int evictSize) {
	if(0 < evictSize && evictSize <= c->maxBlockSize
		&& c->cachedSize + evictSize > c->maxCacheSize) {
		evictSize -= (c->maxCacheSize - c->cachedSize);
		
		int rInc;
		for(rInc = 0; evictSize > 0 && rInc < c->numRows; rInc++) {
			int llIdx = (llStartIdx + rInc)%(c->numRows);
			int breakLoop = 0;
			while(breakLoop == 0 && evictSize > 0) {
				int delSize = evictLinkedList(c->arr[llIdx]);
				if(delSize < 0) {breakLoop = 1;}
				else {evictSize -= delSize;}
			}
		}
	}
	else {DEBUGC(printf("no need to evict\n");)}
}

int readCache(Cache c, char *req, void **retRes) {
	int llIdx = (hash(req))%(c->numRows);
	Node pN;
	DEBUGC(printf("read %s from llIdx: %d\n", req, llIdx);)
	DEBUGC(printNode(c->arr[llIdx]->head);)
	
	//Reader Initialization
	P(&c->all);
	c->readCount++;
	if(c->readCount == 1) {
		//first reader - must lock writers
		P(&c->write);
	}
	V(&c->all);
	
	//Read
	Node n = findNode(c->arr[llIdx], req, &pN);
	
	//Reader Finalization
	P(&c->all);
	c->readCount--;
	if(c->readCount == 0) {
		//last reader - unlock writers
		V(&c->write);
	}
	V(&c->all);
	
	if(n != NULL) {
		//Writer Initialized - lock writers
		P(&c->write);
		
		//*retRes = realloc(*retRes, n->resSize);
		memcpy(*retRes, n->res, n->resSize);
		moveToHead(c->arr[llIdx], n, pN);
		
		//Writer Finalization - unlock writers
		V(&c->write);
		return n->resSize;
	}
	
	return -1;
}

//assumes req is not in cache, otherwise creates a duplicate
void writeCache(Cache c, char *req, void *res, int resSize) {
	int llIdx = (hash(req))%(c->numRows);
	DEBUGC(printf("writing %s to llIdx: %d\n", req, llIdx);)
	
	//Writer Initialization - lock writers
	P(&c->write);
	
	evictIfNecessary(c, llIdx, resSize);
	Node oldHead = c->arr[llIdx]->head;
	c->arr[llIdx]->head = newNode(oldHead, req, res, resSize);
	if(c->arr[llIdx]->tail == NULL) {
		c->arr[llIdx]->tail = c->arr[llIdx]->head;
	}
	DEBUGC(printf("wrote node: ");)
	DEBUGC(printNode(c->arr[llIdx]->head);)
	
	//Writer Finalization - unlock writers
	V(&c->write);
	return;
}


/********
 * Util * 
 ********/ 
 
// String Hashing: djb2
unsigned long hash(char *str){
	unsigned long hash = 5381;
	int c;

	while ((c = *str++)){
		hash = ((hash << 5) + hash) + c;
	}

    return hash;
}

/****************/
/* Testing */
/****************/
//for testing purposes
/*
int main(){
	Cache c = newCache(10, 102400, 1049000);
	char *req = malloc(sizeof(char)*4);
	req[0] = 'A';
	req[0] = 'A';
	req[0] = 'A';
	req[1] = '\0';
	char *res = malloc(sizeof(char)*3);
	res[0] = 'B';
	res[0] = 'B';
	res[1] = '\0';
	
	writeCache(c, req, (void *) res, 2);
	free(res);
	void *cachedRes = malloc(1);
	int cachedResSize = readCache(c, req, &cachedRes);
	free(req);
	printf("Cached Res: %s Cached Res Size: %d\n", (char *) cachedRes, cachedResSize);
	free(cachedRes);
	return freeCache(c);
}
*/


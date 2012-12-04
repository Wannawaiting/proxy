#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "csapp.h"

#define NOCACHE(X) X;


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
};


/***********************
 * Function Prototypes *
 ***********************/
//Node
Node newNode(Node next, char *req, void *res, int resSize);
int freeNode(Node n);

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

//Util
unsigned long hash(char *str);
 

/******************
 * Node Functions *
 ******************/

Node newNode(Node next, char *req, void *res, int resSize) {
	Node n = malloc(sizeof(struct Node));
	n->next = next;
	
	n->req = malloc(strlen(req)*sizeof(char));
	n->req[0] = '\0';
	strcpy(n->req, req);
	
	n->res = malloc(resSize);
	memcpy(n->res, res, resSize);
	n->resSize = resSize;
	
	return n;
}

int freeNode(Node n) {
	if(n == NULL) {return -1;}
	
	if(n->req != NULL) {free(n->req);}
	if(n->res != NULL) {free(n->res);}
	
	int freeSize = n->resSize;
	free(n);
	return freeSize;
}


/************************
 * LinkedList Functions *
 ************************/
 
LinkedList newLinkedList(Node head, Node tail) {
	LinkedList ll = malloc(sizeof(struct LinkedList));
	ll->head = head;
	ll->tail = tail;
	return ll;
}

int freeLinkedList(LinkedList ll) {
	if(ll == NULL) {return -1;}
	
	int freeSize = 0;
	while(ll->head != NULL) {
		Node n = ll->head;
		ll->head = n->next;
		freeSize += freeNode(n);
	}
	
	free(ll);
	return freeSize;
}

int evictLinkedList(LinkedList ll) {
	if(ll == NULL) {return -2;}
	
	int evictSize = -1;
	Node pTail = ll->head;
	if(pTail != NULL && pTail->next != ll->tail) {
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
	return evictSize;
}

Node findNode(LinkedList ll, char *req, Node *retPrevNode) {
	if(ll != NULL || req != NULL || retPrevNode != NULL) {
		return NULL;
	}
	
	Node n = ll->head;
	*retPrevNode = NULL;
	while(n != NULL && !strcasecmp(n->req, req)) {
		*retPrevNode = n;
		n = n->next;
	}
	
	return n;
}

void moveToHead(LinkedList ll, Node n, Node pN) {
	//notice for pN to be null, n is the head node due to findNode
	if(ll != NULL && n != NULL && pN != NULL) {
		//LOCK ALL
		
		pN->next = n->next;
		if(ll->tail == n) {ll->tail = pN;}
		n->next = ll->head;
		ll->head = n;
		
		//UNLOCK ALL
	}
}


/*******************
 * Cache Functions *
 *******************/

Cache newCache(int numRows, int maxBlockSize, int maxCacheSize) {
	Cache c = malloc(sizeof(struct Cache));
	
	c->arr = malloc(sizeof(LinkedList)*numRows);
	c->numRows = numRows;
	for(int r = 0; r < numRows; r++) {
		c->arr[r] = newLinkedList(NULL, NULL);
	}
	
	c->cachedSize = 0;
	c->maxBlockSize = maxBlockSize;
	c->maxCacheSize = maxCacheSize;
	
	return c;
}
 
int freeCache(Cache c) {
	if(c == NULL) {return -1;}
	
	int freeSize = 0;
	for(int r = 0; r < c->numRows; r++) {
		freeSize += freeLinkedList(c->arr[r]);
	}
	free(c->arr);
	
	return freeSize;
}

void evictIfNecessary(Cache c, int llStartIdx, int evictSize) {
	if(0 < evictSize && evictSize <= c->maxBlockSize
		&& c->cachedSize + evictSize > c->maxCacheSize) {
		evictSize -= (c->maxCacheSize - c->cachedSize);
		
		for(int rInc = 0; evictSize > 0 && rInc < c->numRows; rInc++) {
			int llIdx = (llStartIdx + rInc)%(c->numRows);
			int breakLoop = 0;
			while(breakLoop == 0 && evictSize > 0) {
				int delSize = evictLinkedList(c->arr[llIdx]);
				if(delSize < 0) {breakLoop = 1;}
				else {evictSize -= delSize;}
			}
		}
	}
}

int readCache(Cache c, char *req, void **retRes) {
	int llIdx = (hash(req))%(c->numRows);
	Node pN;
	Node n = findNode(c->arr[llIdx], req, &pN);
	
	if(n != NULL) {
		memcpy(*retRes, n->res, n->resSize);
		moveToHead(c->arr[llIdx], n, pN);
		return n->resSize;
	}
	
	*retRes = NULL;
	return -1;
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
 
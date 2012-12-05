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
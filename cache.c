#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "csapp.h"

#define NOCACHE(X) X;

/***************/
/* Node Struct */
/***************/
//make a doubly linked list
typedef struct Node *Node;
struct Node{
	Node next;
	Node prev;
	char *request;
	char *response;
	int responseSize;
	int valid;
};

//function prototypes
Node newNode(Node prev, Node next, char *request, char *response, int responseSize);
void freeNode(Node n);
void appendNode(Node n1, Node n2);
void prependNode(Node n1, Node n2);
void removeNode(Node n);
void printNode(Node n);

//returns a pointer to a Node struct
//uses malloc, so call freeNode at the end of the program
Node newNode(Node prev, Node next, char *request, char *response, int responseSize){
	Node n;
	if((n = malloc(sizeof(struct Node))) == NULL){
		printf("malloc error\n");
		exit(1);
	}
	n->prev = prev;
	n->next = next;
	n->request = request;
	n->response = response;
	n->responseSize = responseSize;
	n->valid = 0;
	return n;
}

//recursively frees node and all subsequent nodes until reaches end of list
//NOTE: request and response are malloced by the user, so we must free them
void freeNode(Node n){
	if(n == NULL){
		return;
	} else {
		Node next = n->next;
		if(n->request != NULL) free(n->request);
		if(n->response != NULL) free(n->response);
		free(n);
		freeNode(next);
		return;
	}
}

//appends Node n1 to the list after n2 and before n2's next
void appendNode(Node n1, Node n2){
	if(n1 == NULL || n2 == NULL){
		//this can lead to problems when freeing memory
		return;
	} else {
		//rearrange links in list
		Node n2Next = n2->next; 
		n2->next = n1;
		n1->prev = n2;
		n1->next = n2Next;
		if(n2Next != NULL){
			n2Next->prev = n1;
		}
		return;
	}
}

//makes Node n1 be the first in the list
void prependNode(Node n1, Node n2){
	if(n1 == NULL || n2 == NULL){
		//this can lead to problems when freeing memory
		return;
	} else {
		//rearrange links in list
		n1->next = n2;
		n1->prev = NULL;
		n2->prev = n1;
	}
	return;
}

//removes Node n from list and links it's prev and next
void removeNode(Node n){
	Node next = n->next;
	Node prev = n->prev;
	//set up new links
	if(next != NULL){
		next->prev = prev;
	}
	if(prev != NULL){
		prev->next = next;
	}
	n->next = NULL;
	n->prev = NULL;
	return;
}

//recursively prints the contents of node n and all subsequent nodes
void printNode(Node n){
	if(n == NULL){
		//reached end of list
		printf("[END]\n");
		return;
	} else {
		//get all required data (next, req, res...)
		Node next = n->next;
		char *req = n->request;
		char *res = n->response;
		int valid = n->valid;
		//print data in this node
		printf("[req:%s | res:%s | valid:%d] -> ", req, res, valid);
		//print the next node
		printNode(next);
		return;
	}
}

//returns an uninitialized linked list with 'n' nodes
Node newList(int n){
	if(n <= 0){
		return NULL;
	}

	//first node
	Node root = newNode(NULL, NULL, NULL, NULL, 0);
	
	//create (n-1) more nodes
	int i;
	Node temp;
	for(i = 1; i < n; i++){
		temp = newNode(NULL, NULL, NULL, NULL, 0);
		appendNode(temp, root);
	}

	return root;
}

/******************/
/* SETS FUNCTIONS */
/******************/
//functions that treat nodes as sets in a cache

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
/* Cache Struct */
/****************/
//an array of linked lists
typedef struct Cache *Cache;
struct Cache{
	Node *array;
	int length;
	//for threading
	int readCount;
	sem_t all;
	sem_t write;

};

//tries to retrieve a response from cache based on request
//if found - HIT - move to beginning of set (LRU) and return responseSize and copy response to *userResponse
//if not found - MISS - return -1 and set *userResponse to NULL
int getFromCache(Cache c, char *req, char **userResponse){
	NOCACHE(return -1;)
	//reader initialisation
	P(&c->all);
	c->readCount++;
	if(c->readCount == 1){
		//first reader - lock writers
		P(&c->write);
	}
	V(&c->all);

	int index = hash(req)%(c->length);
	Node set = c->array[index];
	Node node = c->array[index];
	while(node != NULL){
		if(node->valid == 0){
			//no match found - reader finalisation
			P(&c->all);
			c->readCount--;
			if(c->readCount == 0){
				//last reader - unlock writers
				V(&c->write);
			}
			V(&c->all);
			//*userResponse = NULL;
			return -1;
		}

		if(!strcasecmp(req, node->request)){
			//match found - reader finalisation
			P(&c->all);
			c->readCount--;
			if(c->readCount == 0){
				//last reader - unlock writers
				V(&c->write);
			}
			V(&c->all);

			//lock writers
			P(&c->write);
			//move node to beginning of set and make set point to it
			//ONLY IF IT IS NOT ALREADY THE BEGINNING
			if(set != node){
				c->array[index] = node;
				prependNode(node, set);
			}
			
			//unlock writers
			V(&c->write);
			
			//set the "returned" response
			//*userResponse = realloc(*userResponse, node->responseSize);
			memcpy(*userResponse, node->response, node->responseSize);
			//*userResponse = node->response;
						
			return node->responseSize;
		} else {
			//keep looking
			node = node->next;
		}
	}

	//no match found - reader finalisation
	P(&c->all);
	c->readCount--;
	if(c->readCount == 0){
		//last reader - unlock writers
		V(&c->write);
	}
	V(&c->all);
	//*userResponse = NULL;
	return -1;
}

//writes a request:response pair into the cache
//eviction policy is LRU
void writeToCache(Cache c, char *req, char *res, int resSize){
	NOCACHE(return;)
	//lock writers
	P(&c->write);
	//new node to insert
	Node n = newNode(NULL, NULL, req, res, resSize);
	n->valid = 1;
	//get set
	int index = hash(req)%(c->length);
	Node set = c->array[index];
	//remove last node
	Node last = c->array[index];
	while(last != NULL){
		if(last->next == NULL){
			//last node, free and remove it
			removeNode(last);
			free(last);
			last = NULL;
		} else {
			last = last->next;
		}
	}
	//put new node in beginning of set and make the set point to it
	c->array[index] = n;
	prependNode(n, set);
	//unlock writers
	V(&c->write);
}

/*******************/
/* Cache Functions */
/*******************/

//function prototypes
Cache newCache(int sets, int slots);
void freeCache(Cache c);
void printCache(Cache c);

//returns a new cache struct with 'sets' linked lists, each with 'slots' nodes
Cache newCache(int sets, int slots){
	if(sets <= 0 || slots <= 0){
		return NULL;
	}

	Cache cache;
	Node *array;

	if((cache = malloc(sizeof(struct Cache))) == NULL){
		printf("malloc error\n");
		exit(1);
	}

	if((array = malloc(sizeof(Node)*sets)) == NULL){
		printf("malloc error\n");
		exit(1);
	}

	//add sets to the cache
	int i;
	for(i = 0; i < sets; i++){
		array[i] = newList(slots);
	}

	cache->array = array;
	cache->length = sets;
	Sem_init(&cache->all, 0, 1);
	Sem_init(&cache->write, 0, 1);

	return cache;
}

//frees all sets in the cache and the cache itself
void freeCache(Cache c){
	//free all sets
	int i;
	for(i = 0; i < c->length; i++){
		freeNode(c->array[i]);
	}
	//free cache struct
	free(c);
}

//prints all sets in the cache
void printCache(Cache c){
	//print all sets
	int i;
	for(i = 0; i < c->length; i++){
		printNode(c->array[i]);
	}
	printf("[END CACHE]\n");
}

/****************/
/* MAIN ROUTINE */
/****************/
//for testing purposes
/*
int main(){
	Cache c = newCache(10, 3);
	char *req = malloc(sizeof(char)*2);
	req[0] = 'A';
	req[1] = '\0';
	char *res = malloc(sizeof(char)*2);
	res[0] = 'B';
	res[1] = '\0';
	writeToCache(c, req, res);
	//printCache(c);
	//printCache(c);
	char *response;
	response = getFromCache(c, req);
	printf("Response: %s\n", response);
	printCache(c);
	freeCache(c);
	return 0;
}*/
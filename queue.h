#ifndef __queue_h
#define __queue_h

// Queue Structures
struct queue
{
       char *message;
       //size_t *msgSize;
       struct queue *next;
};

struct q_head
{
       struct queue *first;
       struct queue *last;
};

// Binary Data FIFO Queue Functions
inline int fnQput( struct q_head *, char * );  // puts a string onto the queue
inline int fnQget( struct q_head *, char * );  // gets a string from the queue, releases the memory
int fnQlook( struct q_head *, char *);  // gets a string from the queue but leaves it on the queue
inline int fnQstatus( struct q_head *head );   // check to see if there are any items on the queue: TRUE = yes
int fnQinit( struct q_head * );         // Initializes the queue - if items are on the queue, they are removed

// Text Data FIFO Queue Functions
int fnSrvPush( struct q_head *, char *  );
int fnSrvGet( struct q_head *, char * );
int fnSrvStatus( struct q_head * );
int fnSrvInit( struct q_head * );

#endif

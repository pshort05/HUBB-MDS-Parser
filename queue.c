#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "global.h"

#define TRUE                    1
#define FALSE                   0

// ----------------------------------------------------------------------------
//     FIFO Queue Functions 
//			Used to send Binary data between threads
// ----------------------------------------------------------------------------

// **************** Put an item onto a queue ****************
inline int fnQput( struct q_head *head, char * temp_message )
{
    struct queue *temp_structure;

	//fnDebug( "fnQput" );
    
    temp_structure = (struct queue *)malloc( (unsigned)(sizeof(struct queue)) );
    if( temp_structure == (struct queue *)NULL )
        return FALSE;
        
	// Allocate memory for the message buffer
	// The first character is the size of the data being put on the queue
    temp_structure->message = (char *)malloc( (int)temp_message[0]+3 );
    if( temp_structure->message == (char *)NULL )
    {
		if( temp_structure != NULL )
	        free( (struct queue *)temp_structure );
        return FALSE;
    }
    
    memcpy( temp_structure->message, temp_message, temp_message[0]+1 );
    
    temp_structure->next = (struct queue *)0;
    
    if ( head->first == (struct queue *)0 )   
    {
         // Queue is empty - initialize with first items
         head->first = temp_structure;
         head->last = temp_structure;
    }
    else
    {
        // Queue had items, put at end
        head->last->next = temp_structure;
        head->last = temp_structure;
    }
    
    return TRUE;
}

// **************** Get the next item from a queue ****************
//          This will destroy the item and free the memory
inline int fnQget( struct q_head *head, char *ret_message )
{
    struct queue *temp;
	//fnDebug( "fnQGet" );
    
    if( head->first != (struct queue *)0 )
    {
        if( head->first->message[0] == 0 )
			return FALSE;
		memcpy( ret_message, head->first->message, head->first->message[0]+1 );
        if ( head->first->message != NULL )
			free( head->first->message );
        temp = head->first;
        head->first = temp->next;
        if( temp != NULL )
			free( (char *)temp );
        return TRUE;
    }
    else	// Queue is empty
	{
		iITCHQueueSize=0;
        return FALSE;
	}
}

// **************** Look at the next item on a queue ****************
//            The item will stay at the top of the queue
int fnQlook( struct q_head *head, char *ret_message)
{
	//fnDebug( "fnQlook" );
    if( head->first != (struct queue *)0 )
    {
        memcpy( ret_message, head->first->message, head->first->message[0] );
        return TRUE;
    }
    else
    {
        //fnThrowError( "fnQlook", "Queue is empty", TMERR_WARNING );
        strcpy( ret_message, "" );
        return FALSE;
    }
}

// **************** Check if there are any items on the queue ****************
inline int fnQstatus( struct q_head *head )
{
	//fnDebug( "fnQstatus" );

    if( head->first != (struct queue *)0 )
        return TRUE;
    else
        return FALSE;
}

       
// **************** Initialize the queue ****************
//  Can also be used to reinitialize the entire queue and
//  release all the memory  
int fnQinit( struct q_head *head )
{
    struct queue *q_pointer, *next_structure;
    
    // read the queue and remove each member - free all memory in use
    for( q_pointer=head->first; q_pointer != (struct queue *)0; q_pointer = next_structure )
    {
         if (q_pointer == NULL)
            break;
         next_structure = q_pointer->next;
         if ( q_pointer->message != NULL )
			free( q_pointer->message );
         if( q_pointer != NULL )
			free( (char*)q_pointer );
    }
    
    head->first = (struct queue *)0;
    head->last = (struct queue *)0;
    
    //printf( "Queue initialized\n" );
    iITCHQueueSize=0;
    return TRUE;
} 

// **************** Put an item onto a queue ****************
int fnSrvPush( struct q_head *head, char * temp_message )
{
    struct queue *temp_structure;
    
    temp_structure = (struct queue *)malloc( (unsigned)(sizeof(struct queue)) );
    if( temp_structure == (struct queue *)NULL )
        return FALSE;
        
	// Allocate memory for the message buffer
    temp_structure->message = (char *)malloc( strlen(temp_message)+2 );
    if( temp_structure->message == (char *)NULL )
    {
		if( temp_structure != NULL )
        	free( (struct queue *)temp_structure );
        return FALSE;
    }

	// Lock the queue while we add a new memember
	pthread_mutex_lock( &qOutMessages_mutex );
	
    strcpy( temp_structure->message, temp_message );
    
    temp_structure->next = (struct queue *)0;
    
    if ( head->first == (struct queue *)0 )   
    {
         // Queue is empty - initialize with first items
         head->first = temp_structure;
         head->last = temp_structure;
    }
    else
    {
        // Queue had items, put at end
        head->last->next = temp_structure;
        head->last = temp_structure;
    }
	iOutboundQueueSize++;
	pthread_mutex_unlock( &qOutMessages_mutex );			     
    return TRUE;
}

// ----------------------------------------------------------------------------
//     FIFO Queue Functions 
//			Used to send text data between threads
// ----------------------------------------------------------------------------
//		Get the next item from a queue
//      This will destroy the item and free the memory
int fnSrvGet( struct q_head *head, char *ret_message )
{
    struct queue *temp;
    
    if( head->first != (struct queue *)0 )
    {
        strcpy( ret_message, head->first->message );
		if ( head->first->message != NULL )
	        free( head->first->message );
        temp = head->first;
        head->first = temp->next;
		if( temp != NULL )
	        free( (char *)temp );
		iOutboundQueueSize--;
        return TRUE;
    }
    else
        return FALSE;
}

// **************** Check if there are any items on the queue ****************
int fnSrvStatus( struct q_head *head )
{
	//fnDebug( "fnQstatus" );

    if( head->first != (struct queue *)0 )
        return TRUE;
    else
        return FALSE;
}

       
// **************** Initialize the queue ****************
//  Can also be used to reinitialize the entire queue and
//  release all the memory  
int fnSrvInit( struct q_head *head )
{
    struct queue *q_pointer, *next_structure;
    
    // read the queue and remove each member - free all memory in use
    for( q_pointer=head->first; q_pointer != (struct queue *)0; q_pointer = next_structure )
    {
         if (q_pointer == NULL)
            break;
         next_structure = q_pointer->next;
		 if ( q_pointer->message != NULL )
	         free( q_pointer->message );
		 if ( q_pointer  != NULL )
         	free( (char*)q_pointer );
    }
    
    head->first = (struct queue *)0;
    head->last = (struct queue *)0;
	
	iOutboundQueueSize=0;
    
    //printf( "Queue initialized\n" );
    
    return TRUE;
} 


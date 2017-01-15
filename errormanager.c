#include "errormanager.h"

FILE *fpLogFile;

//----------------------------------------------------------------------------
/*

Note: There are several instances of the error handling routine - this was done
 on purpose to allow different types of errors to be spooled and handled
 differently.  At this point the only change is type three letter "Message Type"
 that can be used to split the file via GREP.

*/
//----------------------------------------------------------------------------
 

//-----------------------------------------------------------------------------
//		Error Handling
//			stub function right now - will expand later
//-----------------------------------------------------------------------------
inline void fnHandleError( char *szFunction, char *szError )
{
	struct timeval tv;
	char szErrorBuffer[BUFFER_SIZE];
	
	gettimeofday( &tv, NULL );
	snprintf( szErrorBuffer, sizeof(szErrorBuffer),"%i:%06i : ERR : %s : %s", (int)tv.tv_sec, (int)tv.tv_usec, szFunction, szError );
	//printf( "%s\n", szErrorBuffer );
	
	pthread_mutex_lock( &qErrorMessages_mutex );
	fnErrPush( &qErrorMessages, szErrorBuffer );
	pthread_mutex_unlock( &qErrorMessages_mutex );
}       

//-----------------------------------------------------------------------------
//		Error Handling
//			This is the error and debugging subsystem
//			It runs in its own thread with a lower priority
//-----------------------------------------------------------------------------
inline void fnDebug( char *szMsg )
{
	struct timeval tv;
	char szErrorBuffer[BUFFER_SIZE];
	
	gettimeofday( &tv, NULL );
	snprintf( szErrorBuffer, sizeof(szErrorBuffer),"%i:%06i : INF : %s", (int)tv.tv_sec, (int)tv.tv_usec, szMsg );
	//printf( "%s\n", szErrorBuffer );

	pthread_mutex_lock( &qErrorMessages_mutex );
	fnErrPush( &qErrorMessages, szErrorBuffer );
	pthread_mutex_unlock( &qErrorMessages_mutex );
}       

//-----------------------------------------------------------------------------
//		MDS Logging
//-----------------------------------------------------------------------------
inline void fnMDSLog( char *szMsg )
{
	struct timeval tv;
	char szErrorBuffer[BUFFER_SIZE];

	gettimeofday( &tv, NULL );
	snprintf( szErrorBuffer, sizeof(szErrorBuffer),"%i:%06i : MDS : %s", (int)tv.tv_sec, (int)tv.tv_usec, szMsg );
	//printf( "%s\n", szErrorBuffer );

	pthread_mutex_lock( &qErrorMessages_mutex );
	fnErrPush( &qErrorMessages, szErrorBuffer );
	pthread_mutex_unlock( &qErrorMessages_mutex );
}       

//-----------------------------------------------------------------------------
//		Volume Logging
//-----------------------------------------------------------------------------
inline void fnVolumeLog( char *szMsg )
{
	struct timeval tv;
	char szErrorBuffer[BUFFER_SIZE];

	gettimeofday( &tv, NULL );
	snprintf( szErrorBuffer, sizeof(szErrorBuffer),"%i:%06i : VOL : %s", (int)tv.tv_sec, (int)tv.tv_usec, szMsg );
	//printf( "%s\n", szErrorBuffer );

	pthread_mutex_lock( &qErrorMessages_mutex );
	fnErrPush( &qErrorMessages, szErrorBuffer );
	pthread_mutex_unlock( &qErrorMessages_mutex );
}

//-----------------------------------------------------------------------------
//		Agent Logging
//-----------------------------------------------------------------------------
inline void fnAgentLog( char *szMsg )
{
	struct timeval tv;
	char szErrorBuffer[BUFFER_SIZE];

	gettimeofday( &tv, NULL );
	snprintf( szErrorBuffer, sizeof(szErrorBuffer),"%i:%06i : AGT : %s", (int)tv.tv_sec, (int)tv.tv_usec, szMsg );
	//printf( "%s\n", szErrorBuffer );

	pthread_mutex_lock( &qErrorMessages_mutex );
	fnErrPush( &qErrorMessages, szErrorBuffer );
	pthread_mutex_unlock( &qErrorMessages_mutex );
}

//-----------------------------------------------------------------------------
// 			Put an item onto a queue 
//-----------------------------------------------------------------------------
int fnErrPush( struct q_head *head, char * temp_message )
{
    struct queue *temp_structure;
    
    temp_structure = (struct queue *)malloc( (unsigned)(sizeof(struct queue)) );
    if( temp_structure == (struct queue *)NULL )
        return FALSE;
        
	// Allocate memory for the message buffer
    temp_structure->message = (char *)malloc( ERROR_BUFFER_SIZE );
    if( temp_structure->message == (char *)NULL )
    {
		if( temp_structure != NULL )
	        free( (struct queue *)temp_structure );
        return FALSE;
    }
    
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
    
	iErrorQueueSize++;
    return TRUE;
}

//-----------------------------------------------------------------------------
//          Get the next item from a queue 
//          This will destroy the item and free the memory
//-----------------------------------------------------------------------------
int fnErrGet( struct q_head *head, char *ret_message )
{
    struct queue *temp;
    
    if( head->first != (struct queue *)0 )
    {
        strcpy( ret_message, head->first->message );
        if( head->first->message != NULL )
			free( head->first->message );
        temp = head->first;
        head->first = temp->next;
		if( temp != NULL )
	        free( (char *)temp );
		iErrorQueueSize--;
        return TRUE;
    }
    else
        return FALSE;
}

//-----------------------------------------------------------------------------
//  Check if there are any items on the queue 
//-----------------------------------------------------------------------------
int fnErrStatus( struct q_head *head )
{
	//fnDebug( "fnQstatus" );

    if( head->first != (struct queue *)0 )
        return TRUE;
    else
        return FALSE;
}

       
//-----------------------------------------------------------------------------
//  Initialize the queue 
//  Can also be used to reinitialize the entire queue and
//  release all the memory  
//-----------------------------------------------------------------------------
int fnErrorInit( struct q_head *head )
{
    struct queue *q_pointer, *next_structure;
    
    // read the queue and remove each member - free all memory in use
    for( q_pointer=head->first; q_pointer != (struct queue *)0; q_pointer = next_structure )
    {
         if (q_pointer == NULL)
            break;
         next_structure = q_pointer->next;
         if( q_pointer->message != NULL )
			free( q_pointer->message );
		 if( q_pointer != NULL )
	         free( (char*)q_pointer );
    }
    
    head->first = (struct queue *)0;
    head->last = (struct queue *)0;
    
    //printf( "Queue initialized\n" );
	iErrorQueueSize=0;
    
    return TRUE;
} 

//-----------------------------------------------------------------------------
// This is the main error handling thread - it runs in the "background"
// and prints out the errors and debug info 
//-----------------------------------------------------------------------------
void *fnErrorManager( void )
{
	char szBuffer[MULTICAST_BUFFER_SIZE];
	char szLogFile[SHORT_BUFFER_SIZE];
	int retCode;
	int iScreen = FALSE;
	struct tm *tLocalTime;
	time_t tTime;
	

	//fnErrorInit( &qErrorMessages );
	//fnDebug( "Error Manager Initialized" );

	tTime = time(NULL);
	tLocalTime = localtime(&tTime);
	snprintf( szLogFile, sizeof(szLogFile), "%s-%04d-%02d-%02d.log", PROGRAM_LOG_FILE, tLocalTime->tm_year+1900, tLocalTime->tm_mon+1, tLocalTime->tm_mday);

	
	fpLogFile = NULL;
	fpLogFile = fopen( szLogFile, "a" );
	if( fpLogFile == NULL )
	{
		printf( "Cannot open %s : streaming updates to the screen.\n", szLogFile );
		iScreen = TRUE;
	}
	sleep(1); //let the queue initially build up during startup

	while( TRUE )
	{
		if( iErrorQueueSize > 0 )
		{
			pthread_mutex_lock( &qErrorMessages_mutex );		
			retCode = fnErrGet( &qErrorMessages, szBuffer );
			pthread_mutex_unlock( &qErrorMessages_mutex );
			sched_yield();										
			if (  retCode == TRUE  )
			{
				if ( szBuffer[strlen(szBuffer)-1] != '\n' )
				{
					if ( iScreen != TRUE )
						fprintf( fpLogFile, "%s\n", szBuffer );
					else
						printf( "%s\n", szBuffer );
				}		
				else
				{
					if ( iScreen != TRUE )
						fprintf( fpLogFile, "%s", szBuffer );
					else
						printf( "%s", szBuffer );
				}
				sched_yield();
			}
			else
			{
				// No messages were found so reset the counter
				iErrorQueueSize=0;
				sched_yield();
				usleep(100);
			}
		}
		else
		{	
			// There were no messages to process, flush the buffer and sleep for
			// 1 milli-seconds
			sched_yield();
			fflush( fpLogFile );
			usleep(100);
		}
		sched_yield();
		sched_yield();
	}
	pthread_exit(NULL);
}

//-----------------------------------------------------------------------------
//	Close the error log
//-----------------------------------------------------------------------------
void fnCloseErrorLog( void )
{
	fflush( fpLogFile );
	fclose( fpLogFile );
}


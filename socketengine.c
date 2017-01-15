#include "main.h"

// ----------------------------------------------------------------------------
//      Standard Socket Functions
//
// ----------------------------------------------------------------------------
//****************************  fnWriteSocket  **************************
//      Function to initialize the socket layer within the program
//		This will be called in it's own thread
// ----------------------------------------------------------------------------

// Read a line from a socket
ssize_t Readline(int sockd, void *vptr, size_t maxlen) 
{
    ssize_t n, rc;
    char    c, *buffer;

    buffer = vptr;

    for ( n = 1; n < maxlen; n++ ) 
	{
	
		if ( (rc = read(sockd, &c, 1)) == 1 ) 
		{
	    	*buffer++ = c;
	    	if ( c == '\n' )
				break;
		}
		else if ( rc == 0 ) 
		{
	    	if ( n == 1 )
				return 0;
	    	else
				break;
		}
		else 
		{
	    	if ( errno == EINTR )
				continue;
	    	return -1;
		}
    }

    *buffer = 0;
    return n;
}


//  Write a line to a socket
ssize_t Writeline(int sockd, const void *vptr, size_t n) 
{
    size_t      nleft;
    ssize_t     nwritten;
    const char *buffer;

    buffer = vptr;
    nleft  = n;

    while ( nleft > 0 ) 
	{
		if ( (nwritten = write(sockd, buffer, 1)) <= 0 ) 
		{
	    	if ( errno == EINTR )
				nwritten = 0;
	    	else
				return -1;
		}
		nleft  -= nwritten;
		buffer += nwritten;
    }

    return n;
}

//****************************  fnWriteSocket  **************************
inline int fnWriteSocket(int tSocket, char *szBuffer, size_t soBuffer) 
{
	return send(tSocket, szBuffer, soBuffer, 0);
}

//****************************  fnReadSocket  **************************
inline int fnReadSocket(int tSocket, char *szBuffer, size_t soBuffer) 
{
    ssize_t n, rc;
    char    c, *buffer;

    buffer = szBuffer;

    for ( n = 1; n < soBuffer; n++ ) 
	{
	
		if ( (rc = read(tSocket, &c, 1)) == 1 ) 
		{
	    	*buffer++ = c;
	    	if ( c == '\n' )
				break;
		}
		else if ( rc == 0 ) 
		{
	    	if ( n == 1 )
				return 0;
	    	else
				break;
		}
		else 
		{
	    	if ( errno == EINTR )
				continue;
	    	return -1;
		}
    }
	return 0;
}

void *fnSocketEngine( void )
{
    int       list_s;                /*  listening socket          */
    int       conn_s;                /*  connection socket         */
    short int port;                  /*  port number               */
    struct    sockaddr_in servaddr;  /*  socket address structure  */
    char      szBuffer[MULTICAST_BUFFER_SIZE];      /*  character buffer          */
	char	  szSocketBuffer[MULTICAST_BUFFER_SIZE];
	char	  szServerIP[CONFIG_BUFFER_SIZE];
	struct pollfd	sPoll;
	double 	fPrice;
	int		iStock;
	int		iFound;


	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting( szBuffer, "SERVERSOCKETPORT", "2994" );
	port = atoi( szBuffer );
	fnGetConfigSetting( szBuffer, "SERVERSOCKETADDRESS", "127.0.0.1" );
	strcpy ( szServerIP, szBuffer );
	pthread_mutex_unlock( &config_mutex );

	iSocket = 0;
	
		// Create the listening socket
		if ( (list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) 
		{
			fnHandleError ( "fnSocketEngine", "Error creating listening socket");
			//pthread_exit(NULL);
		}

		
		// Set all bytes in socket address structure to zero  
		// fill in all the relevant data members
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family      = AF_INET;
		servaddr.sin_addr.s_addr = inet_addr(szServerIP);
		servaddr.sin_port        = htons(port);


		// Bind our socket addresss to the listening socket, and call listen()
		if ( bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) 
		{
			fnHandleError( "fnSocketEngine", "Error calling bind()");
		}

		if ( listen(list_s, LISTENQ) < 0 ) 
		{
			fnHandleError( "fnSocketEngine", "Error calling listen()");
		}

    
    // Enter an infinite loop to respond to client requests and stream MDS Data
    while ( TRUE ) 
	{
		fnDebug( "Socket Server Initialized - waiting for connection" );
		sched_yield();

		// Wait for a connection, then accept() it
		if ( (conn_s = accept(list_s, NULL, NULL) ) < 0 ) 
		{
	    	fnHandleError ( "fnSocketEngine", "Error calling accept()");
	    	continue;
		}
		
		// start the Socket Connection Thread
		iSocket = conn_s;
		
		// The client has attached to the server - start streaming data
		fnDebug ( "Client Connected, streaming data" );
		iClientConnected = TRUE;
		sPoll.fd = conn_s;
		sPoll.events = POLLOUT;
		
		// Stream the current prices in memory if necessary
		if( iSendTU == TRUE )
		{
			for( iStock=0; iStock<INDEX_COUNT_PLUS_2; iStock++ )
			{
				fPrice = fnGetStockPrice(iStock);
				if ( fPrice > 0.00 )
				{
					// Poll the socket to see if the client has disconnected
					poll( &sPoll, 1, 1 );
					if (sPoll.revents&POLLHUP)
						goto SocketClosed; // Jump out of the loop if there is no active client
					snprintf( szSocketBuffer, sizeof(szSocketBuffer), "TU %s 0 %.3f @ S 100 S\n", 
							 fnGetStockSymbol(iStock), fPrice );
					if ( send( conn_s, szSocketBuffer,strlen(szSocketBuffer), 0) < strlen(szSocketBuffer) ) 
					{
						fnHandleError( "fnSocketEngine", "Error writing to socket" );
						goto SocketClosed;
					}
					usleep(25);
				}
			}
		}
						 
		// Start reading the trade updates from the Queue
		while( TRUE )
		{
			if( iOutboundQueueSize > 0 )
			{				
				memset( szSocketBuffer, 0, sizeof(szSocketBuffer) );

				pthread_mutex_lock( &qOutMessages_mutex );			
				iFound = fnSrvGet( &qOutMessages, szSocketBuffer );
				pthread_mutex_unlock( &qOutMessages_mutex );			
			
				if ( iFound == TRUE )
				{
					// Poll the socket to see if the client has disconnected
					poll( &sPoll, 1, 1 );
					if (sPoll.revents&POLLHUP)
						goto SocketClosed;
					if ( fnWriteSocket ( conn_s, szSocketBuffer,strlen(szSocketBuffer)) < strlen(szSocketBuffer) ) 
					{
						fnHandleError( "fnSocketEngine", "Error writing to socket" );
						goto SocketClosed;
					}
				}
				else
					sched_yield();
			}
			else
				sched_yield();

		}

		SocketClosed:
		sched_yield();	

		iClientConnected = FALSE;
		fnDebug( "Client Disconnected" );
			
		// Remove any messages on the queue that haven't been processed	
		fnSrvInit( &qOutMessages );
		sched_yield();
	}
	pthread_exit(NULL);
}


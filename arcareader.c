#include "main.h"
#include "arcareader.h"
#include "gidsreader.h"


// Thread pointers for ARCA
pthread_t pArcaMulticastEngine;
pthread_t pArcaETFMulticastEngine;
pthread_t pArcaOTCMulticastEngine;
pthread_t pARCAMessageParser;


void fnStartARCA( void )
{
	pthread_attr_t	paArcaMulticastEngine;
	pthread_attr_t	paArcaETFMulticastEngine;
	pthread_attr_t	paArcaOTCMulticastEngine;
	pthread_attr_t paARCAMessageParser;

	fnQinit( &qARCAMessages );

		pthread_attr_init( &paARCAMessageParser);
		pthread_attr_setdetachstate( &paARCAMessageParser, PTHREAD_CREATE_DETACHED );
		pthread_create( &pARCAMessageParser, &paARCAMessageParser, (void *)fnARCAMessageParser, NULL );
		fnDebug( "ARCA Message Parser Thread Initialized" );
		usleep( 100 );

		pthread_attr_init( &paArcaMulticastEngine);
		pthread_attr_setdetachstate( &paArcaMulticastEngine, PTHREAD_CREATE_DETACHED );
		pthread_create( &pArcaMulticastEngine, &paArcaMulticastEngine, (void *)fnArcaMulticastEngineReader, NULL );
		fnDebug( "ARCA Multicast reader initialized" );

		pthread_attr_init( &paArcaETFMulticastEngine);
		pthread_attr_setdetachstate( &paArcaETFMulticastEngine, PTHREAD_CREATE_DETACHED );
		pthread_create( &pArcaETFMulticastEngine, &paArcaETFMulticastEngine, (void *)fnArcaETFMulticastEngineReader, NULL );
		fnDebug( "ARCA ETF Multicast reader initialized" );

		pthread_attr_init( &paArcaOTCMulticastEngine);
		pthread_attr_setdetachstate( &paArcaOTCMulticastEngine, PTHREAD_CREATE_DETACHED );
		pthread_create( &pArcaOTCMulticastEngine, &paArcaOTCMulticastEngine, (void *)fnArcaOTCMulticastEngineReader, NULL );
		fnDebug( "ARCA OTC Multicast reader initialized" );
	
}



void fnStopARCA( void )
{

		pthread_cancel( pArcaMulticastEngine );
		pthread_cancel( pArcaOTCMulticastEngine );
		pthread_cancel( pArcaETFMulticastEngine );
		usleep(300);
		pthread_cancel( pARCAMessageParser );
		fnQinit( &qARCAMessages );
		fnDebug( "ARCA reader functions shut down" );
}


//-----------------------------------------------------------------------------
// This thread reads the ARCA Multicast packets and pushes the trade messages 
// off into a new thread there is the absolute minimum processing here so no 
// ARCA messages are missed
//-----------------------------------------------------------------------------
void *fnArcaMulticastEngineReader( void )
{
    struct sockaddr_in name; 			/* Multicast Address */
    struct ip_mreq imr;  				/* Multicast address join structure */
	u_long groupaddr = MULTICAST_DEFAULT_GROUP;
	u_short groupport = MULTICAST_DEFAULT_PORT;	
	char *interface = NULL;

	int sock;                         // Socket 
    //struct sockaddr_in multicastAddr; // Multicast Address 
    char multicastIP[CONFIG_BUFFER_SIZE];                // IP Multicast Address
    //unsigned short multicastPort;     // Port 
    int recvStringLen;                // Length of received string 
    //struct ip_mreq multicastRequest;  // Multicast address join structure
	char byBuffer[MULTICAST_BUFFER_SIZE];
	//char *ptrBuf;
	//char sendBuffer[MULTICAST_BUFFER_SIZE];
	//char *szMsgPtr, *szBufferPtr;
	int iErrorCount = 0;
	int iMaxErrorCount = 10;
	//struct timeval tv;
	//int c,n;


	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting( byBuffer, "ARCAMAXMULTICASTERRORCOUNT", "10" );
	iMaxErrorCount = atoi( byBuffer);

	fnGetConfigSetting( multicastIP, "ARCAMULTICASTSERVER", "224.1.2.224");
	groupaddr = inet_addr(multicastIP);

	fnGetConfigSetting( byBuffer, "ARCAMULTICASTPORT", "17001" );
	groupport = (u_short)atoi(byBuffer);
	
	fnGetConfigSetting( multicastIP, "ARCAMULTICASTINTERFACE", "10.210.105.37");
	interface=multicastIP;
	pthread_mutex_unlock( &config_mutex );
	
    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        fnHandleError ("fnARCAMulticastReader", "socket() failed");

	/* Create multicast structure */
    imr.imr_multiaddr.s_addr = groupaddr;
    imr.imr_multiaddr.s_addr = htonl(imr.imr_multiaddr.s_addr);
    if (interface!=NULL) 
        imr.imr_interface.s_addr = inet_addr(interface);
	else 
		imr.imr_interface.s_addr = htonl(INADDR_ANY);
    imr.imr_interface.s_addr = htonl(imr.imr_interface.s_addr);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq)) < 0 ) 
        fnHandleError ("fnARCAMulticastReader", "setsockopt failed - IP_ADD_MEMBERSHIP");

	/* Bind Socket */
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(groupaddr);
    name.sin_port = htons(groupport);
    if (bind(sock, (struct sockaddr *)&name, sizeof(name))) 
        fnHandleError ("fnARCAMulticastReader", "bind failed");

	
	fnDebug( "ARCA Multicast port opened start reading" );


	// Zero out the entire receive buffer
   	memset( byBuffer, 0, sizeof(byBuffer) );

	while ( TRUE )
	{

		//fnDebug( "Get multicast data" );

		// Receive a datagram from the server
		if ((recvStringLen = recvfrom(sock, byBuffer, sizeof(byBuffer)-1, 0, NULL, 0)) < 0)
		{
			fnHandleError( "fnARCAMulticastEngineReader", "recvfrom() failed" );
			iErrorCount++;
			if (iErrorCount == MAX_RECIEVE_ERRORS )
			{
				// Set a maximum error count so the program will die if something is not working
				fnHandleError( "fnARCAMulticastEngineReader", "Maximum Consecutive Recieve Errors" );
				close( sock );
				pthread_exit(NULL);
			}
			else
				continue;	
		}
		else
			iErrorCount = 0;
		
			//fnDebug( "Got ARCA Multicast Message" );
	
		//**** OK we should have a good data packet now - let's filter it ****// 

		// Filter out any non pricing messages and retransmissions
		if ( byBuffer[12] == 0x71 &&  byBuffer[13] == 0x01 )
		{
			//memset( sendBuffer, 0, ARCA_TRADE_MESSAGE_SIZE+4);
			byBuffer[0] = 0x50;
			byBuffer[1] = 0x20;
			//for( c=ARCA_MULTICAST_HEADER_SIZE, n=2; c<ARCA_TRADE_MESSAGE_SIZE; c++,n++)
			//	sendBuffer[n]=byBuffer[c];
			// Add the size and message type to the header so we can filter this like an ARCA msg
			// memcpy( szBufferPtr, szMsgPtr, (size_t) ARCA_TRADE_MESSAGE_SIZE );
			
			pthread_mutex_lock( &qARCAMessages_mutex );
			fnQput( &qARCAMessages, byBuffer );	// Push this message onto the queue for processing
			iARCAQueueSize++;
			pthread_mutex_unlock( &qARCAMessages_mutex );
		}


		// Zero out the part of the buffer that was used
		memset( byBuffer, 0, sizeof(byBuffer) );
	}		

    
    close(sock);
    pthread_exit(NULL);
}

//-----------------------------------------------------------------------------
// This thread reads the ARCA ETF Multicast packets and pushes the trade messages 
// off into a new thread there is the absolute minimum processing here so no 
// ARCA messages are missed
//-----------------------------------------------------------------------------
void *fnArcaETFMulticastEngineReader( void )
{
    struct sockaddr_in name; 			/* Multicast Address */
    struct ip_mreq imr;  				/* Multicast address join structure */
	u_long groupaddr = MULTICAST_DEFAULT_GROUP;
	u_short groupport = MULTICAST_DEFAULT_PORT;	
	char *interface = NULL;

	int sock;                         // Socket 
    //struct sockaddr_in multicastAddr; // Multicast Address 
    char multicastIP[CONFIG_BUFFER_SIZE];                // IP Multicast Address
    //unsigned short multicastPort;     // Port 
    int recvStringLen;                // Length of received string 
    //struct ip_mreq multicastRequest;  // Multicast address join structure
	char byBuffer[MULTICAST_BUFFER_SIZE];
	//char sendBuffer[MULTICAST_BUFFER_SIZE];
	//char *szMsgPtr, *szBufferPtr;
	int iErrorCount = 0;
	int iMaxErrorCount = 10;
	//struct timeval tv;
	//int c,n;


	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting( byBuffer, "ARCAETFMAXMULTICASTERRORCOUNT", "10" );
	iMaxErrorCount = atoi( byBuffer);

	fnGetConfigSetting( multicastIP, "ARCAETFMULTICASTSERVER", "224.1.2.224");
	groupaddr = inet_addr(multicastIP);

	fnGetConfigSetting( byBuffer, "ARCAETFMULTICASTPORT", "17001" );
	groupport = (u_short)atoi(byBuffer);
	
	fnGetConfigSetting( multicastIP, "ARCAETFMULTICASTINTERFACE", "10.210.105.37");
	interface=multicastIP;
	pthread_mutex_unlock( &config_mutex );
	
    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        fnHandleError ("fnARCAETFMulticastReader", "socket() failed");

	/* Create multicast structure */
    imr.imr_multiaddr.s_addr = groupaddr;
    imr.imr_multiaddr.s_addr = htonl(imr.imr_multiaddr.s_addr);
    if (interface!=NULL) 
        imr.imr_interface.s_addr = inet_addr(interface);
	else 
		imr.imr_interface.s_addr = htonl(INADDR_ANY);
    imr.imr_interface.s_addr = htonl(imr.imr_interface.s_addr);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq)) < 0 ) 
        fnHandleError ("fnArcaETFMulticastReader", "setsockopt failed - IP_ADD_MEMBERSHIP");

	/* Bind Socket */
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(groupaddr);
    name.sin_port = htons(groupport);
    if (bind(sock, (struct sockaddr *)&name, sizeof(name))) 
        fnHandleError ("fnArcaETFMulticastReader", "bind failed");


	fnDebug( "ARCA ETF Multicast port opened start reading" );


	// Zero out the entire receive buffer
   	memset( byBuffer, 0, sizeof(byBuffer) );

	while ( TRUE )
	{

		//fnDebug( "Get multicast data" );

		// Receive a datagram from the server
		if ((recvStringLen = recvfrom(sock, byBuffer, sizeof(byBuffer)-1, 0, NULL, 0)) < 0)
		{
			fnHandleError( "fnARCAETFMulticastEngineReader", "recvfrom() failed" );
			iErrorCount++;
			if (iErrorCount == MAX_RECIEVE_ERRORS )
			{
				// Set a maximum error count so the program will die if something is not working
				fnHandleError( "fnARCAETFMulticastEngineReader", "Maximum Consecutive Recieve Errors" );
				close( sock );
				pthread_exit(NULL);
			}
			else
				continue;	
		}
		else
			iErrorCount = 0;
		
		//fnDebug( "Got ARCA ETF Multicast Message" );
		
		//**** OK we should have a good data packet now - let's filter it ****// 

		// Filter out any non pricing messages and retransmissions
		if ( byBuffer[12] == 0x71 &&  byBuffer[13] == 0x01 )
		{
			//memset( sendBuffer, 0, sizeof(sendBuffer));
			byBuffer[0] = 0x50;
			byBuffer[1] = 0x20;
			//for( c=ARCA_MULTICAST_HEADER_SIZE, n=2; c<ARCA_TRADE_MESSAGE_SIZE; c++,n++)
			//	sendBuffer[n]=byBuffer[c];
			// Add the size and message type to the header so we can filter this like an ARCA msg
			// memcpy( szBufferPtr, szMsgPtr, (size_t) ARCA_TRADE_MESSAGE_SIZE );

			pthread_mutex_lock( &qARCAMessages_mutex );
			fnQput( &qARCAMessages, byBuffer );	// Push this message onto the queue for processing
			iARCAQueueSize++;
			pthread_mutex_unlock( &qARCAMessages_mutex );
		}


		// Zero out the part of the buffer that was used
		memset( byBuffer, 0, sizeof(byBuffer) );
	}		

    
    close(sock);
    pthread_exit(NULL);
}






//-----------------------------------------------------------------------------
// This thread reads the ARCA OTC Multicast packets and pushes the trade messages 
// off into a new thread there is the absolute minimum processing here so no 
// ARCA messages are missed - this grabs all the non-NYSE listed stocks (NASDAQ)
//-----------------------------------------------------------------------------
void *fnArcaOTCMulticastEngineReader( void )
{
    struct sockaddr_in name; 			/* Multicast Address */
    struct ip_mreq imr;  				/* Multicast address join structure */
	u_long groupaddr = MULTICAST_DEFAULT_GROUP;
	u_short groupport = MULTICAST_DEFAULT_PORT;	
	char *interface = NULL;

	int sock;                         // Socket 
    char multicastIP[CONFIG_BUFFER_SIZE];                // IP Multicast Address
    int recvStringLen;                // Length of received string 
	char byBuffer[MULTICAST_BUFFER_SIZE];
	//char sendBuffer[MULTICAST_BUFFER_SIZE];
	int iErrorCount = 0;
	int iMaxErrorCount = 10;
	//int c,n;


	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting( byBuffer, "ARCAOTCMAXMULTICASTERRORCOUNT", "10" );
	iMaxErrorCount = atoi( byBuffer);

	fnGetConfigSetting( multicastIP, "ARCAOTCMULTICASTSERVER", "224.1.2.224");
	groupaddr = inet_addr(multicastIP);

	fnGetConfigSetting( byBuffer, "ARCAOTCMULTICASTPORT", "17001" );
	groupport = (u_short)atoi(byBuffer);
	
	fnGetConfigSetting( multicastIP, "ARCAOTCMULTICASTINTERFACE", "10.210.105.37");
	interface=multicastIP;
	pthread_mutex_unlock( &config_mutex );
	
    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        fnHandleError ("fnARCAOTCMulticastReader", "socket() failed");

	/* Create multicast structure */
    imr.imr_multiaddr.s_addr = groupaddr;
    imr.imr_multiaddr.s_addr = htonl(imr.imr_multiaddr.s_addr);
    if (interface!=NULL) 
        imr.imr_interface.s_addr = inet_addr(interface);
	else 
		imr.imr_interface.s_addr = htonl(INADDR_ANY);
    imr.imr_interface.s_addr = htonl(imr.imr_interface.s_addr);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq)) < 0 ) 
        fnHandleError ("fnArcaOTCMulticastReader", "setsockopt failed - IP_ADD_MEMBERSHIP");

	/* Bind Socket */
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(groupaddr);
    name.sin_port = htons(groupport);
    if (bind(sock, (struct sockaddr *)&name, sizeof(name))) 
        fnHandleError ("fnArcaOTCMulticastReader", "bind failed");


	fnDebug( "ARCA OTC Multicast port opened start reading" );


	// Zero out the entire receive buffer
   	memset( byBuffer, 0, sizeof(byBuffer) );

	while ( TRUE )
	{

		//fnDebug( "Get multicast data" );

		// Receive a datagram from the server
		if ((recvStringLen = recvfrom(sock, byBuffer, sizeof(byBuffer)-1, 0, NULL, 0)) < 0)
		{
			fnHandleError( "fnARCAOTCMulticastEngineReader", "recvfrom() failed" );
			iErrorCount++;
			if (iErrorCount == MAX_RECIEVE_ERRORS )
			{
				// Set a maximum error count so the program will die if something is not working
				fnHandleError( "fnARCAOTCMulticastEngineReader", "Maximum Consecutive Recieve Errors" );
				close( sock );
				pthread_exit(NULL);
			}
			else
				continue;	
		}
		else
			iErrorCount = 0;
		
		//fnDebug( "Got ARCA OTC Multicast Message" );
		
		//**** OK we should have a good data packet now - let's filter it ****// 

		// Filter out any non pricing messages and retransmissions
		if ( byBuffer[12] == 0x71 &&  byBuffer[13] == 0x01 )
		{
			//memset( sendBuffer, 0, sizeof(sendBuffer));
			byBuffer[0] = 0x50;
			byBuffer[1] = 0x20;
			//for( c=ARCA_MULTICAST_HEADER_SIZE, n=2; c<ARCA_TRADE_MESSAGE_SIZE; c++,n++)
			//	sendBuffer[n]=byBuffer[c];

			// Add the size and message type to the header so we can filter this like an ARCA msg
			// memcpy( szBufferPtr, szMsgPtr, (size_t) ARCA_TRADE_MESSAGE_SIZE );

			pthread_mutex_lock( &qARCAMessages_mutex );
			fnQput( &qARCAMessages, byBuffer );	// Push this message onto the queue for processing
			iARCAQueueSize++;
			pthread_mutex_unlock( &qARCAMessages_mutex );
		}


		// Zero out the part of the buffer that was used
		memset( byBuffer, 0, sizeof(byBuffer) );
	}		

    
    close(sock);
    pthread_exit(NULL);
}

typedef union {
        int   value;
        char  string[5];
} bindata;


//-----------------------------------------------------------------------------
//		Message Parcer - to be run in its own thread - pulls the Arca messages 
//		off the queue and parses them into the HUBB readable text format
//-----------------------------------------------------------------------------
void *fnARCAMessageParser( void )
{
	struct ITCHData sId, tmpOrder;							// ITCH Order Structure
	union binary_int binInt;								// 4 byte union for converting Integers
	union binary_long binLong;								// 8 byte union for converting long integers (64 bit only)
 	char szBuffer[MULTICAST_BUFFER_SIZE];					// Temporary message buffer
	long iFound;
	char *price;
	char szPrice[BUFFER_SIZE];
	int iStockIndex=0;
	int n, c;
	char buffer[MULTICAST_BUFFER_SIZE];
	double fPrice=0;
	bindata b;
	struct ITCHData dOrder;

	fnDebug( "fnARCAEngineParser Started" );

	// Initialize the buffers
	memset( &sId, 0, sizeof(sId));
	memset( &tmpOrder, 0, sizeof(tmpOrder));
	memset( binInt.string, 0, sizeof(binInt.string) );
	memset( binLong.string, 0, sizeof(binLong.string) );
	
	
	// Grab ARCA messages off the queue and process them
	while ( TRUE )
	{
		if ( iARCAQueueSize > 0 )
		{
			//printf("get messages\n");

			// There are entries on the queue so start pulling them off
			memset( buffer, 0, sizeof(buffer) );

			pthread_mutex_lock( &qARCAMessages_mutex );
			iFound = fnQget( &qARCAMessages, buffer );
			if (iFound)
				iARCAQueueSize--;
			else
				iARCAQueueSize=0;
			pthread_mutex_unlock( &qARCAMessages_mutex );

			//fnDebug( "fnARCAMessageParser Got ARCA Trade Message from Queue" );

	                // Print out the message for debugging purposes 
    			/******************************/
                  		//struct timeval tv;
                  		//pthread_mutex_lock( &qBufferPrint_mutex );			
                  		//gettimeofday( &tv, NULL );
                  		//printf( "ARCA Trade Message: Time: %2d:%06d : ", (int)tv.tv_sec, (int)tv.tv_usec );
                  		//for( c=0; c<=buffer[0]; c++ )
                    		//	printf( "%.2X ", buffer[c] );
                  		//printf( "\n\n" );    	
                  		//pthread_mutex_unlock( &qBufferPrint_mutex );			
                  	/******************************/

			
			if ( iFound == TRUE )
			{

					memset( &dOrder, 0, sizeof(dOrder) );
					switch( buffer[1] )
					{
							
							case 0x20:
							{
								//fnDebug( "Got ARCA 0x20 trade message" );
								memset( dOrder.Stock, 0, sizeof(dOrder.Stock) );
								for( c=60, n=0; n<6; c++, n++ )
									 if( buffer[c] != 0x00 )
										 dOrder.Stock[n]=buffer[c];
								
								// Check if this is a stock we are tracking - if not discard this message
								iStockIndex = fnStockFilter( dOrder.Stock );
								if( iStockIndex == STOCK_NOT_FOUND )
								{
									sched_yield();
									break;
								}

								memset( b.string, 0, sizeof(b.string) );
								for( c=36, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Price = b.value;
								fPrice = dOrder.Price/10000.00;

								memset( b.string, 0, sizeof(b.string) );
								for( c=40, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Shares = b.value;

								snprintf( szBuffer, sizeof(szBuffer), "TU %s 20 %.2f @ Z %i A A 0 ARCA\n", 
										 dOrder.Stock, fPrice, dOrder.Shares );  
								if ( iClientConnected == TRUE && iSendTU == TRUE )
									fnSrvPush( &qOutMessages, szBuffer );
								if ( iStockIndex == QQQQ )
									fnSaveQVolume( dOrder.Shares );
								fnMDSLog ( szBuffer );
								
								break;
							}
							
							case GIDS_MESSAGE_TYPE:
							{
								//fnDebug( "Got GIDS trade message" );
								memset( dOrder.Stock, 0, sizeof(dOrder.Stock) );
								for( c=26, n=0; n<STOCK_SYMBOL_SIZE; c++, n++ )
									if( buffer[c] != 0x20 )
										dOrder.Stock[n]=buffer[c];
									else
										break;

								if( fnStockFilter( dOrder.Stock ) == STOCK_NOT_FOUND )
									break;							
									
								price = buffer+44;
								strncpy( szPrice, price, 12);
								fPrice = atof(szPrice);

									
								snprintf( szBuffer, sizeof(szBuffer), "TU NDX 20 %4.2f @ Q 0 G G 0 GIDS\n", 
										 fPrice );  
								if ( iClientConnected == TRUE && iSendTU == TRUE )
									fnSrvPush( &qOutMessages, szBuffer );
								fnMDSLog ( szBuffer );
                       								
								break;
							}

							default:
							{
									
								// Print out the message for debugging purposes 
								/******************************/
								struct timeval tv;
								pthread_mutex_lock( &qBufferPrint_mutex );			
								gettimeofday( &tv, NULL );
								printf( "Unknown Msg from ARCA Queue: Time: %2d:%06d : ", (int)tv.tv_sec, (int)tv.tv_usec );
								for( c=0; c<=buffer[0]; c++ )
									printf( "%.2X ", buffer[c] );
								printf( "\n\n" );    	
								pthread_mutex_unlock( &qBufferPrint_mutex );			
								/******************************/
								

								break;
							}
					}
									
			}
			else
                sched_yield();
		}	
		else
			sched_yield();
			usleep(100);
	}		

    pthread_exit(NULL);

}


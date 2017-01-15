#include "main.h"
#include "batsreader.h"

// Basic Structure to hold all the BATS IP address and thread pointers
struct BATSThreadStruct
{
	char Name[NAME_STRING_SIZE];
	char IPAddress[IP_STRING_LEN];
	char Interface[IP_STRING_LEN];
	int Port;
	int loop;
	pthread_t Thread;
};

typedef union {
        unsigned int   value;
        char  string[5];
} bindata;

// Local Function Prototypes
void *fnBATSMulticastReader( void* data );
void *fnBATSMessageParser( void );
int fnBATSOrderBuffer( int iUnit, int iOperation, struct ITCHData *pOrder, unsigned int iOrderNum );


// Local variables for this module
int iBATSOrderBufferSize[BUFFER_UNITS];
pthread_mutex_t qBATSMessages_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t qBATSOrderBuffer_mutex = PTHREAD_MUTEX_INITIALIZER;
int iBATSQueueSize;
struct q_head qBATSMessages;


// Local Data - one for each thread required
struct BATSThreadStruct BATSThreads[BATS_UNITS];
int iBATSOrderBufferSort[BATS_UNITS];
pthread_t pBATSMessageParser;

// ----------------------------------------------------------------------------
// Function that will startup all the BATS reader threads
// ----------------------------------------------------------------------------
void fnStartBATSReaderThreads( void )
{
	char szTemp[BUFFER_SIZE];
	char szBuffer[BUFFER_SIZE];
	pthread_attr_t	paBATS;
	int i,j;


	// Initialize the message parser
	fnQinit( &qBATSMessages );
	
	memset( BATSThreads, 0, sizeof(BATSThreads));

	// lock and Initialize the order buffers before starting any BATS threads
	pthread_mutex_lock( &qBATSOrderBuffer_mutex );
	for( i=0; i<BATS_UNITS; i++ )
	{
		fnBATSOrderBuffer( i, ORDER_BUFFER_INITIALIZE, NULL, 0 );
		fnBATSOrderBuffer( i, ORDER_BUFFER_LOAD_FROM_DISK, NULL, 0 );
		fnBATSOrderBuffer( i, ORDER_BUFFER_SORT, NULL, 0 );
		iBATSOrderBufferSort[i] = 0;
		iBATSOrderBufferSize[i] = 0;
	}
	pthread_mutex_unlock( &qBATSOrderBuffer_mutex );

	// Start up the message parser thread
	pthread_attr_init( &paBATS);
	pthread_attr_setdetachstate( &paBATS, PTHREAD_CREATE_DETACHED );
	pthread_create( &pBATSMessageParser, &paBATS, (void *)fnBATSMessageParser, NULL  );

	// Startup in the individual "Unit" threads to read the raw BATS feed
	for( i=0, j=1; i<BATS_UNITS; i++,j++ )
	{
		BATSThreads[i].loop = TRUE;
		snprintf( BATSThreads[i].Name, sizeof(BATSThreads[i].Name), "BATS Unit %i", j );

		pthread_mutex_lock( &config_mutex );
		snprintf( szTemp, sizeof(szTemp), "BATS%iMULTICASTSERVER", j);
		fnGetConfigSetting( BATSThreads[i].IPAddress, szTemp, "224.0.62.2");

		snprintf( szTemp, sizeof(szTemp), "BATS%iMULTICASTPORT", j);
		fnGetConfigSetting( szBuffer, szTemp, "30001" );
		BATSThreads[i].Port = atoi(szBuffer);
	
		snprintf( szTemp, sizeof(szTemp), "BATS%iINTERFACE", j);
		fnGetConfigSetting( BATSThreads[i].Interface, szTemp, "10.210.35.37");
		pthread_mutex_unlock( &config_mutex );

		pthread_attr_init( &paBATS);
		pthread_attr_setdetachstate( &paBATS, PTHREAD_CREATE_DETACHED );
		pthread_create( &BATSThreads[i].Thread, &paBATS, (void *)fnBATSMulticastReader, (void*)&i  );
		usleep(1000);
	}
	fnDebug( "BATS Threads Started" );
		
}

// ----------------------------------------------------------------------------
// Function that will shutdown all the BATS threads
// ----------------------------------------------------------------------------
void fnShutdownBATSReaderThreads( void )
{
	int i;
	
	// Lock the buffer system during the entire shutdown process
	pthread_mutex_lock( &qBATSMessages_mutex );
	// Shutdown each individual reader thread and clear out the associated buffer
	for( i=0; i<BATS_UNITS; i++ )
	{
		BATSThreads[i].loop = FALSE;
		usleep(10000);
		pthread_mutex_lock( &qBATSOrderBuffer_mutex );
		fnBATSOrderBuffer( i, ORDER_BUFFER_SAVE_TO_DISK, NULL, 0 );
		fnBATSOrderBuffer( i, ORDER_BUFFER_INITIALIZE, NULL, 0 );
		pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
		pthread_cancel( BATSThreads[i].Thread );
	}
	pthread_cancel( pBATSMessageParser );
	pthread_mutex_unlock( &qBATSMessages_mutex );

	// Initialize the message parser to release any allocated memory
	fnQinit( &qBATSMessages );

	fnDebug( "BATS Threads Shutdown" );
}

// ----------------------------------------------------------------------------
// These are the individual BATS multicast reader threads for each unit
// ----------------------------------------------------------------------------
void *fnBATSMulticastReader( void* data )
{
    struct sockaddr_in name; 			/* Multicast Address */
    struct ip_mreq imr;  				/* Multicast address join structure */
	u_long groupaddr = MULTICAST_DEFAULT_GROUP;
	u_short groupport = MULTICAST_DEFAULT_PORT;	
	char *interface = NULL;

	int sock;                         // Socket 
    int recvStringLen;                // Length of received string 
	char byBuffer[MULTICAST_BUFFER_SIZE];
	int iErrorCount = 0;
	int iMaxErrorCount = 10;
	int me = *((int*)data);     /* thread identifying number */
	char szFName[BUFFER_SIZE];
	int iLogMulticastData = FALSE;

	char *szMessage;
	int iMsgSize;
	int iNumMessages;
	int m;

	snprintf( szFName, sizeof(szFName), "fnBATSMulticastEngineReder-Unit%i", me );
	
	iMaxErrorCount = BATS_MULTICAST_ERROR_COUNT;

	groupaddr = inet_addr(BATSThreads[me].IPAddress);
	groupport = (u_short)BATSThreads[me].Port;
	interface=BATSThreads[me].Interface;
	
    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        fnHandleError ( szFName, "socket() failed");

	/* Create multicast structure */
    imr.imr_multiaddr.s_addr = groupaddr;
    imr.imr_multiaddr.s_addr = htonl(imr.imr_multiaddr.s_addr);
    if (interface!=NULL) 
        imr.imr_interface.s_addr = inet_addr(interface);
	else 
		imr.imr_interface.s_addr = htonl(INADDR_ANY);
    imr.imr_interface.s_addr = htonl(imr.imr_interface.s_addr);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq)) < 0 ) 
        fnHandleError ( szFName, "setsockopt failed - IP_ADD_MEMBERSHIP");

	/* Bind Socket */
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(groupaddr);
    name.sin_port = htons(groupport);
    if (bind(sock, (struct sockaddr *)&name, sizeof(name))) 
        fnHandleError ( szFName, "bind failed");


	// Zero out the entire receive buffer
   	memset( byBuffer, 0, sizeof(byBuffer) );

	snprintf( byBuffer, sizeof(byBuffer), "BATS Thread Unit %i started", me );
	fnDebug( byBuffer );

	while ( BATSThreads[me].loop )
	{

		//fnDebug( "Get BATS multicast data" );

		// Receive a datagram from the server
		if ((recvStringLen = recv(sock, byBuffer, sizeof(byBuffer)-1, 0)) < 0)
		{
			fnHandleError( szFName, "recvfrom() failed" );
			iErrorCount++;
			if (iErrorCount == MAX_RECIEVE_ERRORS )
			{
				// Set a maximum error count so the program will die if something is not working
				fnHandleError( szFName, "Maximum Consecutive Recieve Errors" );
				close( sock );
				pthread_exit(NULL);
			}
			else
				continue;	
		}
		else
			iErrorCount = 0;
		
		//fnDebug( "Got BATS multicast data" );

		// Save the multicast packets out to disk
		if( iLogMulticastData == TRUE )
		{
			char szTemp[BUFFER_SIZE];
			struct timeval tv;
			int p;
			FILE *fpLog;

			snprintf(szTemp, sizeof(szTemp), "BATS%iMulticastData.log", me );
			fpLog = fopen( szTemp, "a" );
			if( fpLog != NULL )
			{		
				// Print out the datagram in hex
				gettimeofday( &tv, NULL );
				fprintf( fpLog, "Time: %2d:%6d : Received BATS Datagram :", (int)tv.tv_sec, (int)tv.tv_usec );
				for( p=0; p<=recvStringLen; p++ )
					fprintf( fpLog, "%.2X ", byBuffer[p] );
				fprintf( fpLog, "\n\n" );    	
				fclose( fpLog );
			}
		}
		
		//**** OK we should have a good data packet now - let's filter it ****// 
		if ( byBuffer[1] > 0x2C || byBuffer[1] < 0x21 )
		{

			// Grab the number of messages in this buffer
			iNumMessages = (int)byBuffer[2];
			
			// Jump past the multicast header and start splitting up messages
			szMessage = byBuffer + BATS_MSG_HEADER_SIZE;

			//iMsgSize = MULTICAST_MSG_HEADER_SIZE;	
			for ( m=0; m<=iNumMessages; m++ )
			{
				//memcpy( szTempBuffer, byMessage, byMessage[0]+1 );
				iMsgSize = (int)szMessage[0];
				if ( iMsgSize == 0 )
					break;

				// Flag the message with the unit number
				szMessage[2] = (char)me;

				// Filter out messages by type
				switch( szMessage[1] )
				{
					// Messages that require action
					case ( BATS_ADD_ORDER_MESSAGE_LONG ):
					case ( BATS_ADD_ORDER_MESSAGE_SHORT	):
					case ( BATS_ORDER_EXECUTED_MESSAGE ):
					case ( BATS_ORDER_EXECUTED_AT_PRICE_MESSAGE ):
					case ( BATS_REDUCE_SIZE_MESSAGE_LONG ):
					case ( BATS_REDUCE_SIZE_MESSAGE_SHORT ):
					case ( BATS_MODIFY_ORDER_MESSAGE_LONG ):
					case ( BATS_MODIFY_ORDER_MESSAGE_SHORT ):
					case ( BATS_DELETE_ORDER_MESSAGE ):
					case ( BATS_TRADE_MESSAGE_LONG ):
					case ( BATS_TRADE_MESSAGE_SHORT ):
						pthread_mutex_lock( &qBATSMessages_mutex );
						fnQput( &qBATSMessages, szMessage );	// Push this message onto the queue for processing
						iBATSQueueSize++;
						pthread_mutex_unlock( &qBATSMessages_mutex );
						break;
					// Messages that can be discarded for now
					case ( BATS_TIME_MESSAGE ):
					case ( BATS_TRADE_BREAK_MESSAGE ):
					case ( BATS_SYMBOL_MAP_MESSAGE ):
					default:
						//fnDebug( "BATS message discarded" );
						break;
				}
				// Move to the next message in the buffer 
				szMessage = szMessage + iMsgSize;
			}

		}
	   	memset( byBuffer, 0, recvStringLen );			

	}		

	snprintf( byBuffer, sizeof(byBuffer), "BATS Reader Unit %i closed", me );
	fnDebug( byBuffer );

	
    close(sock);
    pthread_exit(NULL);
}


// ----------------------------------------------------------------------------
//		Message Parcer - to be run in its own thread - pulls the MDS messages 
//		off the queue and parses them into the HUBB readable text format
// ----------------------------------------------------------------------------
void *fnBATSMessageParser( void )
{
	struct ITCHData sId, tmpOrder;							// ITCH Order Structure
	union binary_int binInt;								// 4 byte union for converting Integers
	union binary_long binLong;								// 8 byte union for converting long integers (64 bit only)
 	char szBuffer[MULTICAST_BUFFER_SIZE];					// Temporary message buffer
	long iFound;
	int iStockIndex=0;
	int n, c;
	int iReturn;
	char buffer[MULTICAST_BUFFER_SIZE];
	double fPrice=0;
	bindata b;
	struct ITCHData dOrder;
	char *stock;
	int iValue=0;

	fnDebug( "fnBATSMessageParser Started" );

	// Initialize the buffers
	memset( &sId, 0, sizeof(sId));
	memset( &tmpOrder, 0, sizeof(tmpOrder));
	memset( binInt.string, 0, sizeof(binInt.string) );
	memset( binLong.string, 0, sizeof(binLong.string) );
	
	
	// Grab BATS messages off the queue and process them
	while ( TRUE )
	{
		if ( iBATSQueueSize > 0 )
		{
			//printf("get messages\n");

			// There are entries on the queue so start pulling them off
			memset( buffer, 0, sizeof(buffer) );

			pthread_mutex_lock( &qBATSMessages_mutex );
			iFound = fnQget( &qBATSMessages, buffer );
			if (iFound)
				iBATSQueueSize--;
			pthread_mutex_unlock( &qBATSMessages_mutex );

			//snprintf( szBuffer, sizeof(szBuffer), "fnBATSEngineParser processing message:  type %c ", buffer[1] );
			//fnDebug( szBuffer );			
			
			// Print out the message for debuggin purposes 
			/******************************
			pthread_mutex_lock( &qBufferPrint_mutex );			
			gettimeofday( &tv, NULL );
			printf( "Got Msg from Queue: Time: %2d:%6d : ", (int)tv.tv_sec, (int)tv.tv_usec );
			for( c=0; c<=buffer[0]; c++ )
				printf( "%.2X ", buffer[c] );
			printf( "\n\n" );    	
			pthread_mutex_unlock( &qBufferPrint_mutex );			
			******************************/
			
			if ( iFound == TRUE )
			{
					memset( &dOrder, 0, sizeof(dOrder) );
					switch( buffer[1] )
					{

							case ( BATS_ADD_ORDER_MESSAGE_SHORT	):
							{
								// grab the stock symbol and terminate the string
								stock = buffer+17;
								for( c=0; c<BATS_STOCK_SYMBOL_SIZE; c++ )
								{
									if ( stock[c] == 0x20 )
									{
										stock[c]=0x00;
										break;
									}
								}
								// determine if it's a stock we need to track
								dOrder.StockIndex = fnStockFilter( stock );
								if( dOrder.StockIndex == STOCK_NOT_FOUND )
									break;
								strcpy( dOrder.Stock, stock );
								
								// We are tracking this stock, now grab the order number
								for( c=9, n=0; n<4; c--, n++ )
									 b.string[n]=buffer[c];
								dOrder.OrderNumber = b.value;
								
								// now grab the size
								b.string[0] = 0x00;
								b.string[1] = 0x00;
								b.string[2] = buffer[16];
								b.string[3] = buffer[15];
								dOrder.Shares = b.value;

								// now get the price
								b.string[0] = 0x00;
								b.string[1] = 0x00;
								b.string[2] = buffer[24];
								b.string[3] = buffer[23];
								dOrder.Price = b.value * 10;

								dOrder.BuySell = buffer[14];

								// lets save this into the order buffer, but first filter out any bad data
								if( dOrder.Price == 0 || dOrder.OrderNumber == 0 )
									fnHandleError( "fnBATSMessageParser","Data Error found - price or order number is 0 - discarding packet" );
								else
								{
									//snprintf( szBuffer, sizeof(szBuffer), "BATS add order(short): %d %u %d %c %d %s %d", buffer[2], dOrder.OrderNumber, dOrder.Shares, dOrder.BuySell, dOrder.Shares, dOrder.Stock, dOrder.Price );
									//fnDebug( szBuffer );
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer( (int)buffer[2], ORDER_BUFFER_ADD, &dOrder, dOrder.OrderNumber );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								}
								break;
							}						
							
							case ( BATS_ADD_ORDER_MESSAGE_LONG ):
							{
								// grab the stock symbol and terminate the string
								stock = buffer+19;
								for( c=0; c<BATS_STOCK_SYMBOL_SIZE; c++ )
								{
									if ( stock[c] == 0x20 )
									{
										stock[c]=0x00;
										break;
									}
								}
								// determine if it's a stock we need to track
								dOrder.StockIndex = fnStockFilter( stock );
								if( dOrder.StockIndex == STOCK_NOT_FOUND )
									break;
								strcpy( dOrder.Stock, stock );
								
								// We are tracking this stock, now grab the order number
								for( c=9, n=0; n<4; c--, n++ )
									 b.string[n]=buffer[c];
								dOrder.OrderNumber = b.value;
								
								// now grab the size
								b.string[0] = buffer[18];
								b.string[1] = buffer[17];
								b.string[2] = buffer[16];
								b.string[3] = buffer[15];
								dOrder.Shares = b.value;

								// now get the price
								b.string[0] = buffer[28];
								b.string[1] = buffer[27];
								b.string[2] = buffer[26];
								b.string[3] = buffer[25];
								dOrder.Price = b.value;

								dOrder.BuySell = buffer[14];

								// lets save this into the order buffer, but first filter out any bad data
								if( dOrder.Shares == 0 || dOrder.Price == 0 || dOrder.OrderNumber == 0 )
									fnHandleError( "fnBATSMessageParser","Data Error found - price or order number is 0 - discarding packet" );
								else
								{
									//snprintf( szBuffer, sizeof(szBuffer), "BATS add order(long): %d %d %d %c %d %s %d", buffer[2], dOrder.OrderNumber, dOrder.Shares, dOrder.BuySell, dOrder.Shares, dOrder.Stock, dOrder.Price );
									//fnDebug( szBuffer );
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_ADD, &dOrder, dOrder.OrderNumber );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								}
								break;
							}
							
							case ( BATS_ORDER_EXECUTED_MESSAGE ):
							{								
								// Grab the order number
								for( c=9, n=0; n<4; c--, n++ )
									 b.string[n]=buffer[c];
								dOrder.OrderNumber = b.value;

								// Lookup the order number to make sure it's one we are tracking - if not exit
								iFound = fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_SEARCH, &tmpOrder, dOrder.OrderNumber );
								if( iFound <= ORDER_NOT_FOUND )
									break;						
								
								// now grab the size
								b.string[0] = buffer[17];
								b.string[1] = buffer[16];
								b.string[2] = buffer[15];
								b.string[3] = buffer[14];
								dOrder.Shares = b.value;
						
								// Check for a valid price and size
								if ( dOrder.OrderNumber == 0 || dOrder.Shares == 0)
								{
									fnHandleError ("fnBATSMessageParser", "Data Error found in Execute PITCH message - Order or shares is 0 - discarding message" );
									break;
								}
								
								// Get the data from the order buffer
								pthread_mutex_lock( &qBATSOrderBuffer_mutex );
								iReturn = fnBATSOrderBuffer( (int)buffer[2], ORDER_BUFFER_GET_BY_INDEX, &tmpOrder, iFound);
								pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								
								// Double Check that we have a valid price for the stock
								if( tmpOrder.Price == 0 )
								{
									fnHandleError ( "fnBATSMessageParser", "Bad price found in BATS order - discarding order." );
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
									break;
								}

								// Compare the size to see if this is a partial or complete fill
								if( dOrder.Shares >= tmpOrder.Shares )
								{
									// This is a complete fill - remove the old order
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								}
								else
								{
									// This is a partial fill - update the order amount to reflect the remaining shares
									tmpOrder.Shares = tmpOrder.Shares - dOrder.Shares;
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer(buffer[2], ORDER_BUFFER_MODIFY_BY_INDEX, &tmpOrder, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								}	
								
								// Save the price and calculate the Index value
								fPrice = (double)tmpOrder.Price/1000.00;

								// Create the trade message and push it out if necessary
								snprintf( szBuffer, sizeof(szBuffer), "TU %s %u %.2f @ X %i E E %lu BATS\n", 
												 tmpOrder.Stock, buffer[2], fPrice, dOrder.Shares, tmpOrder.OrderNumber );									
								if (iClientConnected == TRUE && iSendTU == TRUE )
									fnSrvPush( &qOutMessages, szBuffer );
								if ( tmpOrder.StockIndex == QQQQ )
									fnSaveQVolume( dOrder.Shares );
								fnMDSLog( szBuffer );									
																	
								break;
							}

							case ( BATS_TRADE_MESSAGE_LONG ):
							{
								// grab the stock symbol and terminate the string
								stock = buffer+19;
								for( c=0; c<BATS_STOCK_SYMBOL_SIZE; c++ )
								{
									if ( stock[c] == 0x20 )
									{
										stock[c]=0x00;
										break;
									}
								}

								iStockIndex = fnStockFilter( stock );
								if( iStockIndex == STOCK_NOT_FOUND )
									break;

								// now grab the size
								b.string[0] = buffer[18];
								b.string[1] = buffer[17];
								b.string[2] = buffer[16];
								b.string[3] = buffer[15];
								dOrder.Shares = b.value;

								// now get the price
								b.string[0] = buffer[28];
								b.string[1] = buffer[27];
								b.string[2] = buffer[26];
								b.string[3] = buffer[25];
								fPrice = (float)b.value / 10000.00;					

								// Check that we have good data - shares and price > 0
								if ( fPrice == 0 || dOrder.Shares == 0)
								{
									fnHandleError ("fnBATSMessageParser", "Data Error found in Execute Long PITCH message - discarding message" );
									break;
								}
								
								// OK - we have good data, look up the order number in the order buffer by OrderNumber
									
								// Save the price and calculate the Index value
								/*if( fPrice != fnGetStockPrice( iStockIndex ) )
								{
									fnSaveStockPrice( iStockIndex, fPrice );
									fnCalculateIndex();
								}*/

								// Create the trade message and push it out if necessary
								snprintf( szBuffer, sizeof(szBuffer), "TU %s %u %.2f @ X %i E L 0 BATS\n", 
												 stock, buffer[2], fPrice, dOrder.Shares );									
								if (iClientConnected == TRUE && iSendTU == TRUE  )
									fnSrvPush( &qOutMessages, szBuffer );
								if ( iStockIndex == QQQQ )
									fnSaveQVolume( dOrder.Shares );
								fnMDSLog( szBuffer );									
								
								break;
							}

							case ( BATS_TRADE_MESSAGE_SHORT ):
							{
								// grab the stock symbol and terminate the string
								stock = buffer+17;
								for( c=0; c<BATS_STOCK_SYMBOL_SIZE; c++ )
								{
									if ( stock[c] == 0x20 )
									{
										stock[c]=0x00;
										break;
									}
								}
								iStockIndex = fnStockFilter( stock );
								if( iStockIndex == STOCK_NOT_FOUND )
									break;

								// now grab the size
								b.string[0] = 0x00;
								b.string[1] = 0x00;
								b.string[2] = buffer[16];
								b.string[3] = buffer[15];
								dOrder.Shares = b.value;

								// now get the price
								b.string[0] = 0x00;
								b.string[1] = 0x00;
								b.string[2] = buffer[24];
								b.string[3] = buffer[23];
								fPrice = (float)b.value / 100.00;					
								
								// Check that we have good data - shares and price > 0
								if ( fPrice == 0 || dOrder.Shares == 0)
								{
									fnHandleError ("fnBATSMessageParser", "Data Error found in Execute Short PITCH message - discarding message" );
									break;
								}
								
								// OK - we have good data, look up the order number in the order buffer by OrderNumber
									
								// Create the trade message and push it out if necessary
								snprintf( szBuffer, sizeof(szBuffer), "TU %s %u %.2f @ X %i E S 0 BATS\n", 
												 stock, buffer[2], fPrice, dOrder.Shares );									
								if (iClientConnected == TRUE && iSendTU == TRUE )
									fnSrvPush( &qOutMessages, szBuffer );
								if ( iStockIndex == QQQQ )
									fnSaveQVolume( dOrder.Shares );
								fnMDSLog( szBuffer );									

								break;
							}

							case ( BATS_DELETE_ORDER_MESSAGE ):
							{
								// Get the order number
								for( c=9, n=0; n<4; c--, n++ )
									 b.string[n]=buffer[c];
								dOrder.OrderNumber = b.value;

								// Look up the order and if found - delete it
								iFound = fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_SEARCH, NULL, dOrder.OrderNumber );
								if( iFound > ORDER_NOT_FOUND )
								{
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
									//snprintf( szBuffer, sizeof(szBuffer), "BATS Order %u Deleted from Unit %d", dOrder.OrderNumber, (int)buffer[2] );
									//fnDebug( szBuffer );
								}
								break;
							}
							
							case ( BATS_ORDER_EXECUTED_AT_PRICE_MESSAGE ):
							{
								// Get the order number
								for( c=9, n=0; n<4; c--, n++ )
									 b.string[n]=buffer[c];
								dOrder.OrderNumber = b.value;

								// Lookup the order number to make sure it's one we are tracking - if not exit
								iFound = fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_SEARCH, &tmpOrder, dOrder.OrderNumber );
								if( iFound <= ORDER_NOT_FOUND )
									break;		

								// now grab the size
								b.string[0] = buffer[17];
								b.string[1] = buffer[16];
								b.string[2] = buffer[15];
								b.string[3] = buffer[14];
								dOrder.Shares = b.value;

								// Filter out any bad data
								if (dOrder.Shares == 0 || dOrder.OrderNumber == 0 )
									break;
								
								// now get the remaining quantity
								b.string[0] = buffer[21];
								b.string[1] = buffer[20];
								b.string[2] = buffer[19];
								b.string[3] = buffer[18];
								iValue = b.value;
								
								// Get the data from the order buffer
								pthread_mutex_lock( &qBATSOrderBuffer_mutex );
								iReturn = fnBATSOrderBuffer( (int)buffer[2], ORDER_BUFFER_GET_BY_INDEX, &tmpOrder, iFound);
								pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								
								// Double Check that we have a valid price for the stock
								if( tmpOrder.Price == 0 )
								{
									fnHandleError ( "fnBATSMessageParser", "Bad price found in BATS Execute Size order - discarding order." );
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
									break;
								}

								// Compare the size to see if this is a partial or complete fill
								if( dOrder.Shares >= tmpOrder.Shares )
								{
									// This is a complete fill - remove the old order
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								}
								else
								{
									// This is a partial fill - update the order amount to reflect the remaining shares
									tmpOrder.Shares = iValue;
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer(buffer[2], ORDER_BUFFER_MODIFY_BY_INDEX, &tmpOrder, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								}	
								
								// Save the price and calculate the Index value
								fPrice = (double)tmpOrder.Price/1000.00;

								// Create the trade message and push it out if necessary
								snprintf( szBuffer, sizeof(szBuffer), "TU %s %u %.2f @ X %i E P %lu BATS\n", 
												 tmpOrder.Stock, buffer[2], fPrice, dOrder.Shares, tmpOrder.OrderNumber);									
								if (iClientConnected == TRUE && iSendTU == TRUE )
									fnSrvPush( &qOutMessages, szBuffer );
								if ( tmpOrder.StockIndex == QQQQ )
									fnSaveQVolume( dOrder.Shares );
								fnMDSLog( szBuffer );									
																	
								break;
							}
								
							case ( BATS_REDUCE_SIZE_MESSAGE_LONG ):
							case ( BATS_REDUCE_SIZE_MESSAGE_SHORT ):
							{
								// Grab the order Number
								for( c=9, n=0; n<4; c--, n++ )
									 b.string[n]=buffer[c];
								dOrder.OrderNumber = b.value;
								
								// Lookup the order number to make sure it's one we are tracking - if not exit
								iFound = fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_SEARCH, &tmpOrder, dOrder.OrderNumber );
								if( iFound <= ORDER_NOT_FOUND )
									break;		

								// now grab the size
								if( buffer[1] == BATS_REDUCE_SIZE_MESSAGE_LONG )
								{	
									b.string[0] = buffer[17];
									b.string[1] = buffer[16];
								}
								else
								{	
									b.string[0] = 0x00;
									b.string[1] = 0x00;
								}
								b.string[2] = buffer[15];
								b.string[3] = buffer[14];
								dOrder.Shares = b.value;
								if( dOrder.Shares == 0 )
								{
									fnHandleError ( "fnBATSMessageParser", "Data Error in PITCH message - 0 Shares found in Size reduce message" );
									break;
								}
								
								// Get the data from the order buffer
								pthread_mutex_lock( &qBATSOrderBuffer_mutex );
								iReturn = fnBATSOrderBuffer( (int)buffer[2], ORDER_BUFFER_GET_BY_INDEX, &tmpOrder, iFound);
								pthread_mutex_unlock( &qBATSOrderBuffer_mutex );

								if( iReturn <= ORDER_NOT_FOUND )
								{
									fnHandleError( "fnBATSMessageParser", "Data Error in Order Buffer - order may have been deleted during reduce size processing" );
									break;
								}
								tmpOrder.Shares = tmpOrder.Shares - dOrder.Shares;
								if( tmpOrder.Shares > 0 )
								{
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer(buffer[2], ORDER_BUFFER_MODIFY_BY_INDEX, &tmpOrder, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								}
								else
								{
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
								}
								break;								
							}

							
							case ( BATS_MODIFY_ORDER_MESSAGE_SHORT ):
							case ( BATS_MODIFY_ORDER_MESSAGE_LONG ):
							{
								// Grab the order Number
								for( c=9, n=0; n<4; c--, n++ )
									 b.string[n]=buffer[c];
								dOrder.OrderNumber = b.value;
								
								// Lookup the order number to make sure it's one we are tracking - if not exit
								iFound = fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_SEARCH, &tmpOrder, dOrder.OrderNumber );
								if( iFound <= ORDER_NOT_FOUND )
									break;	

								if( buffer[1] == BATS_MODIFY_ORDER_MESSAGE_LONG )
								{	
									// now grab the size
									b.string[0] = buffer[17];
									b.string[1] = buffer[16];
									b.string[2] = buffer[15];
									b.string[3] = buffer[14];
									dOrder.Shares = b.value;

									// now get the price
									b.string[0] = buffer[21];
									b.string[1] = buffer[20];
									b.string[2] = buffer[19];
									b.string[3] = buffer[18];
									dOrder.Price = b.value;	
								}
								else
								{	
									// now grab the size
									b.string[0] = 0x00;
									b.string[1] = 0x00;
									b.string[2] = buffer[15];
									b.string[3] = buffer[14];
									dOrder.Shares = b.value;

									// now get the price
									b.string[0] = 0x00;
									b.string[1] = 0x00;
									b.string[2] = buffer[17];
									b.string[3] = buffer[16];
									dOrder.Price = b.value *10;	
								}
									
								
								// Check that we have good data - shares and price > 0
								if ( dOrder.Price == 0 || dOrder.Shares == 0)
								{
									pthread_mutex_lock( &qBATSOrderBuffer_mutex );
									fnBATSOrderBuffer( buffer[2], ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
									pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
									fnHandleError ("fnBATSMessageParser", "Data Error found in Modify PITCH message - discarding message" );
									break;
								}
								
								// Get the data from the order buffer
								pthread_mutex_lock( &qBATSOrderBuffer_mutex );
								iReturn = fnBATSOrderBuffer( (int)buffer[2], ORDER_BUFFER_GET_BY_INDEX, &tmpOrder, iFound);
								pthread_mutex_unlock( &qBATSOrderBuffer_mutex );

								if( iReturn <= ORDER_NOT_FOUND )
								{
									fnHandleError( "fnBATSMessageParser", "Data Error in Order Buffer - order may have been deleted during modify order" );
									break;
								}
								tmpOrder.Shares = dOrder.Shares;
								tmpOrder.Price = dOrder.Price;
								pthread_mutex_lock( &qBATSOrderBuffer_mutex );
								fnBATSOrderBuffer(buffer[2], ORDER_BUFFER_MODIFY_BY_INDEX, &tmpOrder, iFound );
								pthread_mutex_unlock( &qBATSOrderBuffer_mutex );

								break;
							}

							default:
							{
								for( c=9, n=0; n<4; c--, n++ )
									 b.string[n]=buffer[c];
								dOrder.OrderNumber = b.value;
								
								snprintf( szBuffer, sizeof(szBuffer),  "BATS Unknown Message Processing: Unit %.2X Type %.2X Order %u", buffer[2], buffer[1], b.value );
								fnDebug( szBuffer );
									
								// Print out the message for debugging purposes 
								/******************************/
								struct timeval tv;
								pthread_mutex_lock( &qBufferPrint_mutex );			
								gettimeofday( &tv, NULL );
								printf( "Unknown Msg from BATS Queue: Time: %2d:%6d : ", (int)tv.tv_sec, (int)tv.tv_usec );
								for( c=0; c<=buffer[0]; c++ )
									printf( "%.2X ", buffer[c] );
								printf( "\n\n" );    	
								pthread_mutex_unlock( &qBufferPrint_mutex );			
								/******************************/
								

								break;
							}
					}
									
			}
				
		}	
		else
			sched_yield();
			//usleep(100);
	}		

	fnDebug( "BATS Message Parser Thread Shutdown" );
    pthread_exit(NULL);

}

//-----------------------------------------------------------------------------
//	Function to sort the buffer when out of sequence orders are added
//-----------------------------------------------------------------------------
void fnCheckBATSOrderBuffer( void )
{
	int i;

	for( i=0; i<BATS_UNITS ; i++ )
	{
		if ( iBATSOrderBufferSort[i] > 0 )
		{
			pthread_mutex_lock( &qBATSOrderBuffer_mutex );
			fnBATSOrderBuffer( i, ORDER_BUFFER_SORT, NULL, 0 );
			pthread_mutex_unlock( &qBATSOrderBuffer_mutex );
			iBATSOrderBufferSort[i] = 0;
		}
	}
}

//-----------------------------------------------------------------------------
//	Function to display the 
//-----------------------------------------------------------------------------
void fnDisplayBATSStats( void )
{
	char szBuffer[BUFFER_SIZE];

	snprintf( szBuffer, sizeof(szBuffer), "Queue Sizes: BATS Messages %d Unit Order Buffer Sizes %d %d %d %d %d %d %d %d %d %d %d %d",
	         iBATSQueueSize, iBATSOrderBufferSize[0],  iBATSOrderBufferSize[1], iBATSOrderBufferSize[2], iBATSOrderBufferSize[3], iBATSOrderBufferSize[4],
	          iBATSOrderBufferSize[5], iBATSOrderBufferSize[6], iBATSOrderBufferSize[7], iBATSOrderBufferSize[8], iBATSOrderBufferSize[9], iBATSOrderBufferSize[10],
	          iBATSOrderBufferSize[11] );
	fnDebug( szBuffer );
}


//-----------------------------------------------------------------------------
//	BATS Order Storage
//-----------------------------------------------------------------------------
// BATS Data Structure
//
//	Order storage system:
//	 To initialize:  fnOrderBuffer( ORDER_BUFFER_INITIALIZE, NULL, NULL ) : only first parameter used
//	 To add an order:  fnOrderBuffer( ORDER_BUFFER_ADD, &pOrder, NULL ) : last parameter ignored
//	 To search:  fnOrderBuffer( ORDER_BUFFER_SEARCH, NULL, OrderNumber ): second parameter ignored
//					note: this returns the index of the found item or ORDER_NOT_FOUND if unsuccessful
//	 To remove: fnOrderBuffer( ORDER_BUFFER_REMOVE, NULL, OrderNumber ): second parameter ignored
//	 To replace: fnOrderBuffer( ORDER_BUFFER_REPLACE, &pOrder, OrderNumber )
//	
//-----------------------------------------------------------------------------
struct OrderData
{
        int Shares;             // Number of Shares
        int Price;              // Price in Integer Format (divide by 10000 for float)
        char Stock[ITCH_STOCK_SYMBOL_SIZE+1];   // Stock Symbol
		int StockIndex;
};

struct OrderBuffer
{
	unsigned int OrderNumber;
	struct OrderData *data;
};

//int fnOrderCompare( const void *a, const void *b )
//{
	//const struct OrderBuffer *da = (const struct OrderBuffer *) a;
    //const struct OrderBuffer *db = (const struct OrderBuffer *) b;
	//struct OrderBuffer * const *da = a;
 	//struct OrderBuffer * const *db = b;
     
    //return ((*da)->OrderNumber > (*db)->OrderNumber) - ((*da)->OrderNumber < (*db)->OrderNumber);

//}

int fnBATSOrderCompare(const void* p1, const void* p2)
{
	return ((struct OrderBuffer*)p1)->OrderNumber - ((struct OrderBuffer*)p2)->OrderNumber;
}


int fnBATSOrderBuffer( int iUnit, int iOperation, struct ITCHData *pOrder, unsigned int iOrderNum )
{
	static struct OrderBuffer sBuffer[BUFFER_UNITS][BATS_ORDER_BUFFER_SIZE];
	static int iLastIndex[BUFFER_UNITS];
	static char szBuffer[BUFFER_SIZE];
	
	switch (iOperation)
	{
		// Scan the order buffer, set values to zero and free any memory
		case ORDER_BUFFER_INITIALIZE:
		{
			int i, unit;

			iOrderBufferSort=0;
			iLastIndex[iUnit]=0;
			iBATSOrderBufferSize[iUnit]=0;
			for( unit=0; unit<BUFFER_UNITS; unit++ )
			{
				for( i=0; i<BATS_ORDER_BUFFER_SIZE; i++ )
				{
					sBuffer[unit][i].OrderNumber = 0;
					if( sBuffer[unit][i].data != NULL )
					{
						free (sBuffer[unit][i].data);
						sBuffer[unit][i].data = NULL;
					}
				}
			}
			return ORDER_BUFFER_SUCCESS;
		}
		// Add a new item into the buffer - check for buffer overflow and handle
		//	out of sequence order #'s
		case ORDER_BUFFER_ADD:
		{
			if ( iLastIndex[iUnit] >= BATS_ORDER_BUFFER_SIZE_MINUS_1 )
				return ORDER_BUFFER_FULL;
			if ( sBuffer[iUnit][iLastIndex[iUnit]].OrderNumber > pOrder->OrderNumber )
			{
				// We found an out of sequence entry - flag the buffer for a sort and add the entry into the buffer
				snprintf( szBuffer, sizeof(szBuffer), "Order Buffer Sequence Error: Last Entry %u  New Entry %u", sBuffer[iUnit][iLastIndex[iUnit]].OrderNumber, pOrder->OrderNumber );
				fnHandleError( "fnOrderBuffer", szBuffer );
				iBATSOrderBufferSort[iUnit]++;						
				
			} 	// end of sequence error handling code
			
			sBuffer[iUnit][iLastIndex[iUnit]+1].data = (struct OrderData *)malloc( sizeof(struct OrderData) );
			if( sBuffer[iUnit][iLastIndex[iUnit]+1].data != NULL )
			{
				// OK, we have the memory to add the new entry - copy the data into the buffer
				iLastIndex[iUnit]++;
				iBATSOrderBufferSize[iUnit]++;
				sBuffer[iUnit][iLastIndex[iUnit]].OrderNumber = pOrder->OrderNumber;
				sBuffer[iUnit][iLastIndex[iUnit]].data->Shares = pOrder->Shares;
				sBuffer[iUnit][iLastIndex[iUnit]].data->Price = pOrder->Price;
				sBuffer[iUnit][iLastIndex[iUnit]].data->StockIndex = pOrder->StockIndex;
				strcpy( sBuffer[iUnit][iLastIndex[iUnit]].data->Stock, pOrder->Stock);
				return iLastIndex[iUnit];
			}
			else
			{
				// The memory allocation failed
				fnHandleError( "fnOrderBuffer", "Failed to allocated memory for new order entry" );
				return ORDER_BUFFER_MEMORY_ERROR;
			}
		}				
		
		// Scan the order buffer for an order
		case ORDER_BUFFER_SEARCH:
		{
			int left = 0;
			int right = iLastIndex[iUnit];
			int middle = 0;
			int bsearch = 1;
			//struct timeval tvStart, tvEnd;
			
			// Perform a binary search of the index to find the order entry
			//gettimeofday( &tvStart, NULL );
			while(bsearch == 1 && left <= right) 
			{
				middle = (left + right) / 2;
				if(iOrderNum == sBuffer[iUnit][middle].OrderNumber) 
				{
					// We found a match - return the index #!
			   		bsearch = 0;
					if (sBuffer[iUnit][middle].data == NULL )
					{
						//snprintf( szBuffer, sizeof(szBuffer), 
						//		"Binary Search found a stale match: Index %i OrderNumber %u", middle, sBuffer[iUnit][middle].OrderNumber );
						//fnHandleError( "fnOrderBuffer", szBuffer );				
						return ORDER_BUFFER_FOUND_STALE_MATCH;
					}
					else
					{
						//gettimeofday( &tvEnd, NULL );
						//snprintf( szBuffer, sizeof(szBuffer), 
						//		"Binary Search found a match: Index %i OrderNumber %u search time %i usec", middle, sBuffer[iUnit][middle].OrderNumber,  (int)tvEnd.tv_usec-(int)tvStart.tv_usec );
						//fnDebug( szBuffer );				
						return middle;
					}
				} 
				else 
				{
			   		if(iOrderNum < sBuffer[iUnit][middle].OrderNumber) right = middle - 1;
			   		if(iOrderNum > sBuffer[iUnit][middle].OrderNumber) left = middle + 1;
			  	}
			}			
			//gettimeofday( &tvEnd, NULL );
			//snprintf( szBuffer, sizeof(szBuffer), 
			//			"Binary Search did not find a match: OrderNumber %u search time %i usec", sBuffer[iUnit][middle].OrderNumber,  (int)tvEnd.tv_usec-(int)tvStart.tv_usec );
			//fnDebug( szBuffer );				
			return ORDER_NOT_FOUND;
		}
			
		case ORDER_BUFFER_REMOVE_BY_INDEX:
		{
			// Make sure this is a valid index #
			if( iOrderNum < 0 || iOrderNum > iLastIndex[iUnit] )
			{
				fnHandleError( "fnOrderBuffer", "Attempted to remove an out of bounds index" );
				return ORDER_INDEX_OUT_OF_BOUNDS;
			}
			if( sBuffer[iUnit][iOrderNum].data != NULL )
			{
				// Free the allocated data and zero out the pointer
				free( sBuffer[iUnit][iOrderNum].data);
				sBuffer[iUnit][iOrderNum].data = NULL;
				iBATSOrderBufferSize[iUnit]--;
				if( iBATSOrderBufferSize[iUnit] < 0 )
					iBATSOrderBufferSize[iUnit] = 0;
				return iOrderNum;
			}
			else
			{
				fnHandleError( "fnOrderBuffer", "No data found during Remove by Index" );
				return ORDER_BUFFER_NO_DATA_FOUND;
			}
		}
			
			
		case ORDER_BUFFER_MODIFY_BY_INDEX:
		{
			// Make sure this is a valid index #
			if( iOrderNum < 0 || iOrderNum > iLastIndex[iUnit] )
			{
				fnHandleError( "fnOrderBuffer", "Attempted to update an out of bounds index" );
				return ORDER_INDEX_OUT_OF_BOUNDS;
			}
			if( sBuffer[iUnit][iOrderNum].data != NULL )
			{
				// Update the data in the buffer
				sBuffer[iUnit][iOrderNum].data->Price = pOrder->Price;
				sBuffer[iUnit][iOrderNum].data->Shares = pOrder->Shares;
				return iOrderNum;
			}
			else
			{
				fnHandleError( "fnOrderBuffer", "No data found during Modify by Index" );
				return ORDER_BUFFER_NO_DATA_FOUND;
			}
		}

		case ORDER_BUFFER_GET_BY_INDEX:
		{
			// Make sure this is a valid index #
			if( iOrderNum < 0 || iOrderNum > iLastIndex[iUnit] )
			{
				fnHandleError( "fnOrderBuffer", "Attempted to get an out of bounds index" );
				return ORDER_INDEX_OUT_OF_BOUNDS;
			}
			if( pOrder == NULL )
			{
				fnHandleError( "fnOrderBuffer", "NULL pointer passed during GET_BY_INDEX operation" );
				return ORDER_NULL_POINTER_FOUND;
			}
			if( sBuffer[iUnit][iOrderNum].data != NULL )
			{
				// Update the data in the buffer
				pOrder->OrderNumber = sBuffer[iUnit][iOrderNum].OrderNumber;
				pOrder->Price = sBuffer[iUnit][iOrderNum].data->Price;
				pOrder->Shares = sBuffer[iUnit][iOrderNum].data->Shares;
				pOrder->StockIndex = sBuffer[iUnit][iOrderNum].data->StockIndex;
				strcpy( pOrder->Stock, sBuffer[iUnit][iOrderNum].data->Stock );
				return iOrderNum;
			}
			else
			{
				fnHandleError( "fnOrderBuffer", "No data found during get by Index" );
				return ORDER_BUFFER_NO_DATA_FOUND;
			}
		}

		case ORDER_BUFFER_SAVE_TO_DISK:
		{
			int i;
			FILE *fp;
			struct tm *tLocalTime;
			time_t tTime;
			char szOrderBuffer[BUFFER_SIZE];
			

			tTime = time(NULL);
			tLocalTime = localtime(&tTime);
			snprintf( szOrderBuffer, sizeof(szOrderBuffer), "%s%02i-%04d-%02d-%02d.csv", BATS_ORDER_BUFFER_DUMP_FILE, iUnit, tLocalTime->tm_year+1900, tLocalTime->tm_mon+1, tLocalTime->tm_mday);
			
			
			fp = fopen( szOrderBuffer, "w" ); 			
			if( fp != NULL )
			{
				fnDebug( "Dumping Order Buffer to disk" );
				for( i=0; i<BATS_ORDER_BUFFER_SIZE; i++ )
				{
					if( sBuffer[iUnit][i].data != NULL )
					{
						fprintf( fp, "%u,%i,%i,%s,\n", sBuffer[iUnit][i].OrderNumber, sBuffer[iUnit][i].data->Shares, sBuffer[iUnit][i].data->Price, sBuffer[iUnit][i].data->Stock );
					}
				}
				fclose( fp );
				return ORDER_BUFFER_SUCCESS;
			}
			return ORDER_BUFFER_FILE_SAVE_ERROR;
		}

		case ORDER_BUFFER_SORT:
		{
			snprintf( szBuffer, sizeof(szBuffer), "Sort of %i elements", iLastIndex[iUnit] );
			fnDebug( szBuffer );
			qsort( sBuffer[iUnit], iLastIndex[iUnit], sizeof(sBuffer[iUnit][0]), fnBATSOrderCompare );
			fnDebug( "Sort Complete!" );

			return ORDER_BUFFER_SUCCESS;
		}

		case ORDER_BUFFER_LOAD_FROM_DISK:
		{
			int i;
			FILE *fp;
			char *szTempBuffer;
			struct tm *tLocalTime;
			time_t tTime;
			char szOrderBuffer[BUFFER_SIZE];	

			tTime = time(NULL);
			tLocalTime = localtime(&tTime);
			snprintf( szOrderBuffer, sizeof(szOrderBuffer), "%s%02i-%04d-%02d-%02d.csv", BATS_ORDER_BUFFER_DUMP_FILE, iUnit, tLocalTime->tm_year+1900, tLocalTime->tm_mon+1, tLocalTime->tm_mday);
			if( fnFileExists( szOrderBuffer ) == FALSE )
				return ORDER_BUFFER_NO_DUMP_FILE;	
			
			fp = fopen( szOrderBuffer, "r" );
			if( fp != NULL )
			{
				fnDebug( "Loading Order Buffer from File" );
				for( i=0; i<BATS_ORDER_BUFFER_SIZE; i++ )
				{
					memset( szBuffer, 0, sizeof(szBuffer) );
					if( fgets( szBuffer, sizeof(szBuffer), fp ) == NULL )
						break;
					sBuffer[iUnit][i].data = (struct OrderData *)malloc( sizeof(struct OrderData) );
					if( sBuffer[iUnit][i].data != NULL )
					{
						//fnDebug( szBuffer );
						if( strlen(szBuffer) < 10 ) // filter for poorly formatted lines
							continue;
						szTempBuffer = strtok( szBuffer, "," );
						sBuffer[iUnit][i].OrderNumber = (unsigned)atoi( szTempBuffer );
						szTempBuffer = strtok( NULL, "," );
						sBuffer[iUnit][i].data->Shares = atoi( szTempBuffer );
						szTempBuffer = strtok( NULL, "," );
						sBuffer[iUnit][i].data->Price = atoi( szTempBuffer );
						szTempBuffer = strtok( NULL, "," );
						strcpy( sBuffer[iUnit][i].data->Stock, szTempBuffer );
						iLastIndex[iUnit] = i;
						iBATSOrderBufferSize[iUnit]++;
					}
					if( feof(fp) != 0 )
					   break;
				}
				fclose( fp );
				return ORDER_BUFFER_SUCCESS;
			}
			return ORDER_BUFFER_READ_ERROR;
		}
			
			
		default:
		{
			snprintf( szBuffer, sizeof(szBuffer), "Invalid Operation %i", iOperation );
			fnHandleError( "fnOrderBuffer", szBuffer );
			return ORDER_BUFFER_INVALID_OPERATION;
		}
	}
	return ORDER_BUFFER_CRITICAL_ERROR;
}

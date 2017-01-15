#include "main.h"
#include "itchreader.h"

// Multicast Defaults
#define MULTICAST_DEFAULT_GROUP	0xe0027fff
#define MULTICAST_DEFAULT_PORT		9876
#define MULTICAST_MAXPDU 4096
#define MULTICAST_WIDTH 16

pthread_t pMulticastEngine;
pthread_t pMessageParser;
pthread_t pDeleteMessageEngineParser;

long iDeleteLoopCount=0;

void fnStartITCH( void )
{
	pthread_attr_t	paMulticastEngine;
	pthread_attr_t paMessageParser;
	pthread_attr_t paDeleteMessageEngineParser;

	// Initialize the message queues
	fnQinit( &qITCHMessages );
	fnQinit( &qDeleteMessages );
			
			// Initialize the ITCH Order Buffer
			pthread_mutex_lock( &qBufferSystem_mutex );
			fnOrderBuffer( ORDER_BUFFER_INITIALIZE, NULL, 0 );
			fnOrderBuffer( ORDER_BUFFER_LOAD_FROM_DISK, NULL, 0 );
			fnOrderBuffer( ORDER_BUFFER_SORT, NULL, 0 );
			pthread_mutex_unlock( &qBufferSystem_mutex );
			fnDebug("ITCH Initialized Order Buffer");

			// start the Message Parser Thread
			pthread_attr_init( &paMessageParser);
			pthread_attr_setdetachstate( &paMessageParser, PTHREAD_CREATE_DETACHED );
			pthread_create( &pMessageParser, &paMessageParser, (void *)fnMessageParser, NULL );
			fnDebug( "ITCH Message Parser Thread Initialized" );

			// start the Delete Message Parser Thread
			pthread_attr_init( &paDeleteMessageEngineParser);
			pthread_attr_setdetachstate( &paDeleteMessageEngineParser, PTHREAD_CREATE_DETACHED );
			pthread_create( &pDeleteMessageEngineParser, &paDeleteMessageEngineParser, (void *)fnDeleteMessageEngineParser, NULL );
			fnDebug( "ITCH Delete Message Parser Thread Initialized" );
			usleep(100);
	
			// start the Engine Reader Thread
			pthread_attr_init( &paMulticastEngine);
			pthread_attr_setdetachstate( &paMulticastEngine, PTHREAD_CREATE_DETACHED );
			pthread_create( &pMulticastEngine, &paMulticastEngine, (void *)fnMulticastEngineReader, NULL );
			fnDebug( "ITCH Multicast reader started" );

}

void fnStopITCH( void )
{
		// Shutdown the threads
		pthread_cancel( pMulticastEngine );
		usleep( 100 );
		pthread_cancel( pDeleteMessageEngineParser );
		usleep( 100 );
		pthread_cancel( pMessageParser );
		usleep( 100 );

		// Save and Free all the memory from the Order Buffer
		pthread_mutex_lock( &qBufferSystem_mutex );
		fnOrderBuffer( ORDER_BUFFER_SAVE_TO_DISK, NULL, 0 );
		fnOrderBuffer( ORDER_BUFFER_INITIALIZE, NULL, 0 );
		pthread_mutex_unlock( &qBufferSystem_mutex );

		// Clean up the message queues
		fnQinit( &qITCHMessages );
		fnQinit( &qDeleteMessages );
		fnDebug( "ITCH reader functions shut down" );
}	


//-----------------------------------------------------------------------------
// This thread reads the Multicast packets and pushes the ITCH messages off into a new thread
// there is the absolute minimum processing here so no ITCH messages are missed
//-----------------------------------------------------------------------------
void *fnMulticastEngineReader( void )
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
	int iErrorCount = 0;
	int iMaxErrorCount = 10;

	char *szBuffer;
	char *szMessage;
	int iMsgSize;
	int iNumMessages;
	int m;

	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting( byBuffer, "ITCHMAXMULTICASTERRORCOUNT", "10" );
	iMaxErrorCount = atoi( byBuffer);

	fnGetConfigSetting( multicastIP, "ITCHMULTICASTSERVER", "233.54.12.119");
	groupaddr = inet_addr(multicastIP);

	fnGetConfigSetting( byBuffer, "ITCHMULTICASTPORT", "26477" );
	groupport = (u_short)atoi(byBuffer);
	
	fnGetConfigSetting( multicastIP, "ITCHMULTICASTINTERFACE", "10.210.105.37");
	interface=multicastIP;
	pthread_mutex_unlock( &config_mutex );
	
    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        fnHandleError ("fnITCHMulticastReader", "socket() failed");

	/* Create multicast structure */
    imr.imr_multiaddr.s_addr = groupaddr;
    imr.imr_multiaddr.s_addr = htonl(imr.imr_multiaddr.s_addr);
    if (interface!=NULL) 
        imr.imr_interface.s_addr = inet_addr(interface);
	else 
		imr.imr_interface.s_addr = htonl(INADDR_ANY);
    imr.imr_interface.s_addr = htonl(imr.imr_interface.s_addr);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq)) < 0 ) 
        fnHandleError ("fnITCHMulticastReader", "setsockopt failed - IP_ADD_MEMBERSHIP");

	/* Bind Socket */
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(groupaddr);
    name.sin_port = htons(groupport);
    if (bind(sock, (struct sockaddr *)&name, sizeof(name))) 
        fnHandleError ("fnITCHMulticastReader", "bind failed");


	// Zero out the entire receive buffer
   	memset( byBuffer, 0, sizeof(byBuffer) );

	while ( TRUE )
	{

		//fnDebug( "Get ITCH multicast data" );

		// Receive a datagram from the server
		if ((recvStringLen = recvfrom(sock, byBuffer, sizeof(byBuffer)-1, 0, NULL, 0)) < 0)
		{
			fnHandleError( "fnMulticastEngineReader", "recvfrom() failed" );
			iErrorCount++;
			if (iErrorCount == MAX_RECIEVE_ERRORS )
			{
				// Set a maximum error count so the program will die if something is not working
				fnHandleError( "fnMulticastEngineReader", "Maximum Consecutive Recieve Errors" );
				close( sock );
				pthread_exit(NULL);
			}
			else
				continue;	
		}
		else
			iErrorCount = 0;
				
		// OK we should have a good data packet now - let's filter it 
		// then send it off for processing
		//fnDebug( "Got ITCH Message" );

		szBuffer = byBuffer;

		// Grab the number of messages in this buffer
		iNumMessages = (int)szBuffer[19];
		
		// Jump past the multicast header and start splitting up messages
		szMessage = szBuffer + MULTICAST_MSG_HEADER_SIZE;
		//fnDebug( "fnFilter message" );

		
		//iMsgSize = MULTICAST_MSG_HEADER_SIZE;	
		for ( m=0; m<=iNumMessages; m++ )
		{
			//memcpy( szTempBuffer, byMessage, byMessage[0]+1 );
			iMsgSize = (int)szMessage[0];
			if ( iMsgSize == 0 )
				break;
			//snprintf( szTmpBuffer, sizeof(szTmpBuffer), "Msg num: %d Msg size: %.2X  Msg type: %.2X", m, szMessage[0], szMessage[1] );
			//fnDebug( szTmpBuffer );

			// Filter out messages by type
			switch( szMessage[1] )
			{
				// Messages that require action
				case ( ITCH_ADD_ORDER_MESSAGE ):
				case ( ITCH_ADD_ORDER_MPID_MESSAGE ):
				case ( ITCH_EXECUTED_ORDER_MESSAGE ):
				case ( ITCH_EXECUTED_ORDER_PRICE_MESSAGE ):
				case ( ITCH_ORDER_REPLACE_MESSAGE ):           
				case ( ITCH_TRADE_MESSAGE ):
				case ( ITCH_ORDER_CANCEL_MESSAGE ):
					pthread_mutex_lock( &qITCHMessages_mutex );
					fnQput( &qITCHMessages, szMessage );	// Push this message onto the queue for processing
					iITCHQueueSize++;
					// Set queuing Flag
					if ( iITCHQueueSize > ITCH_MESSAGE_BUFFERING_COUNT )
						iMessagesQueued = TRUE;
					else
						iMessagesQueued = FALSE;
					pthread_mutex_unlock( &qITCHMessages_mutex );
					break;
				case ( ITCH_ORDER_DELETE_MESSAGE ):
					pthread_mutex_lock( &qDeleteMessages_mutex );
					fnQput( &qDeleteMessages, szMessage );	// Push this message onto the delete queue for processing
					iDeleteQueueSize++;
					pthread_mutex_unlock( &qDeleteMessages_mutex );
					break;
				// Messages that can be discarded for now
				case ( ITCH_BROKEN_TRADE_MESSAGE ):
				case ( ITCH_NET_ORDER_INBALANCE_MESSAGE ):	// pre/after market inbalances
				case ( ITCH_TIME_MESSAGE ):					// NASDAQ timestamps **** need to handle this later
				case ( ITCH_SYSTEM_MESSAGE ):				// System Alerts **** need to handle this later
				case ( ITCH_DIRECTORY_MESSAGE ):			// Stock Directory Premarket
				case ( ITCH_STOCK_TRADING_MESSAGE ):		// Stock Trading Premarket
				case ( ITCH_STOCK_PARTICIPANT_MESSAGE ):	// Marketmaker books Premarket		
				case ( ITCH_CROSS_TRADE_MESSAGE ):
				default:
					//fnDebug( "fnFilter discarded message" );
					break;
			}
			// Move to the next message in the buffer 
			szMessage = szMessage + iMsgSize +2;
		}

		// Zero out the part of the buffer that was used
		memset( byBuffer, 0, sizeof(byBuffer) );
	}		

    
    close(sock);
    pthread_exit(NULL);
}


//-----------------------------------------------------------------------------
//	Order Storage
//-----------------------------------------------------------------------------
// ITCH Data Structure
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
	unsigned long OrderNumber;
	struct OrderData *data;
};

int fnOrderCompare(const void* p1, const void* p2)
{
	return ((struct OrderBuffer*)p1)->OrderNumber - ((struct OrderBuffer*)p2)->OrderNumber;
}

int fnOrderBuffer( int iOperation, struct ITCHData *pOrder, unsigned long iOrderNum )
{
	static struct OrderBuffer sBuffer[ORDER_BUFFER_SIZE];
	static int iLastIndex;
	static char szBuffer[BUFFER_SIZE];
	
	switch (iOperation)
	{
		// Scan the order buffer, set values to zero and free any memory
		case ORDER_BUFFER_INITIALIZE:
		{
			int i;

			iOrderBufferSort=0;
			iLastIndex=0;
			iOrderBufferSize=0;
			for( i=0; i<ORDER_BUFFER_SIZE; i++ )
			{
				sBuffer[i].OrderNumber = 0;
				if( sBuffer[i].data != NULL )
				{
					free (sBuffer[i].data);
					sBuffer[i].data = NULL;
				}
			}
			return ORDER_BUFFER_SUCCESS;
		}
		// Add a new item into the buffer - check for buffer overflow and handle
		//	out of sequence order #'s
		case ORDER_BUFFER_ADD:
		{
			if ( iLastIndex >= ORDER_BUFFER_SIZE_MINUS_1 )
				return ORDER_BUFFER_FULL;
			if ( sBuffer[iLastIndex].OrderNumber > pOrder->OrderNumber )
			{
				// We found an out of sequence entry - flag the buffer for a sort and add the entry into the buffer
				snprintf( szBuffer, sizeof(szBuffer), "Order Buffer Sequence Error: Last Entry %lu  New Entry %lu", sBuffer[iLastIndex].OrderNumber, pOrder->OrderNumber );
				fnHandleError( "fnOrderBuffer", szBuffer );
				iOrderBufferSort++;						
				
			} 	// end of sequence error handling code
			
			sBuffer[iLastIndex+1].data = (struct OrderData *)malloc( sizeof(struct OrderData) );
			if( sBuffer[iLastIndex+1].data != NULL )
			{
				// OK, we have the memory to add the new entry - copy the data into the buffer
				iLastIndex++;
				iOrderBufferSize++;
				sBuffer[iLastIndex].OrderNumber = pOrder->OrderNumber;
				sBuffer[iLastIndex].data->Shares = pOrder->Shares;
				sBuffer[iLastIndex].data->Price = pOrder->Price;
				sBuffer[iLastIndex].data->StockIndex = pOrder->StockIndex;
				strcpy( sBuffer[iLastIndex].data->Stock, pOrder->Stock);
				return iLastIndex;
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
			int right = iLastIndex;
			int middle = 0;
			int bsearch = 1;
			//struct timeval tvStart, tvEnd;
			
			// Perform a binary search of the index to find the order entry
			//gettimeofday( &tvStart, NULL );
			while(bsearch == 1 && left <= right) 
			{
				middle = (left + right) / 2;
				if(iOrderNum == sBuffer[middle].OrderNumber) 
				{
					// We found a match - return the index #!
			   		bsearch = 0;
					if (sBuffer[middle].data == NULL )
					{
						//snprintf( szBuffer, sizeof(szBuffer), 
						//		"Binary Search found a stale match: Index %i OrderNumber %u", middle, sBuffer[middle].OrderNumber );
						//fnHandleError( "fnOrderBuffer", szBuffer );				
						return ORDER_BUFFER_FOUND_STALE_MATCH;
					}
					else
					{
						//gettimeofday( &tvEnd, NULL );
						//snprintf( szBuffer, sizeof(szBuffer), 
						//		"Binary Search found a match: Index %i OrderNumber %u search time %i usec", middle, sBuffer[middle].OrderNumber,  (int)tvEnd.tv_usec-(int)tvStart.tv_usec );
						//fnDebug( szBuffer );				
						return middle;
					}
				} 
				else 
				{
			   		if(iOrderNum < sBuffer[middle].OrderNumber) right = middle - 1;
			   		if(iOrderNum > sBuffer[middle].OrderNumber) left = middle + 1;
			  	}
			}			
			//gettimeofday( &tvEnd, NULL );
			//snprintf( szBuffer, sizeof(szBuffer), 
			//			"Binary Search did not find a match: OrderNumber %u search time %i usec", sBuffer[middle].OrderNumber,  (int)tvEnd.tv_usec-(int)tvStart.tv_usec );
			//fnDebug( szBuffer );				
			return ORDER_NOT_FOUND;
		}
			
		case ORDER_BUFFER_REMOVE_BY_INDEX:
		{
			// Make sure this is a valid index #
			if( iOrderNum < 0 || iOrderNum > iLastIndex )
			{
				fnHandleError( "fnOrderBuffer", "Attempted to remove an out of bounds index" );
				return ORDER_INDEX_OUT_OF_BOUNDS;
			}
			if( sBuffer[iOrderNum].data != NULL )
			{
				// Free the allocated data and zero out the pointer
				free( sBuffer[iOrderNum].data);
				sBuffer[iOrderNum].data = NULL;
				iOrderBufferSize--;
				if( iOrderBufferSize < 0 )
					iOrderBufferSize = 0;
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
			if( iOrderNum < 0 || iOrderNum > iLastIndex )
			{
				fnHandleError( "fnOrderBuffer", "Attempted to update an out of bounds index" );
				return ORDER_INDEX_OUT_OF_BOUNDS;
			}
			if( sBuffer[iOrderNum].data != NULL )
			{
				// Update the data in the buffer
				sBuffer[iOrderNum].data->Price = pOrder->Price;
				sBuffer[iOrderNum].data->Shares = pOrder->Shares;
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
			if( iOrderNum < 0 || iOrderNum > iLastIndex )
			{
				fnHandleError( "fnOrderBuffer", "Attempted to get an out of bounds index" );
				return ORDER_INDEX_OUT_OF_BOUNDS;
			}
			if( pOrder == NULL )
			{
				fnHandleError( "fnOrderBuffer", "NULL pointer passed during GET_BY_INDEX operation" );
				return ORDER_NULL_POINTER_FOUND;
			}
			if( sBuffer[iOrderNum].data != NULL )
			{
				// Update the data in the buffer
				pOrder->OrderNumber = sBuffer[iOrderNum].OrderNumber;
				pOrder->Price = sBuffer[iOrderNum].data->Price;
				pOrder->Shares = sBuffer[iOrderNum].data->Shares;
				pOrder->StockIndex = sBuffer[iOrderNum].data->StockIndex;
				strcpy( pOrder->Stock, sBuffer[iOrderNum].data->Stock );
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
			snprintf( szOrderBuffer, sizeof(szOrderBuffer), "%s-%04d-%02d-%02d.csv", ORDER_BUFFER_DUMP_FILE, tLocalTime->tm_year+1900, tLocalTime->tm_mon+1, tLocalTime->tm_mday);
			
			
			fp = fopen( szOrderBuffer, "w" ); 			
			if( fp != NULL )
			{
				fnDebug( "Dumping Order Buffer to disk" );
				for( i=0; i<ORDER_BUFFER_SIZE; i++ )
				{
					if( sBuffer[i].data != NULL )
					{
						fprintf( fp, "%lu,%i,%i,%s,\n", sBuffer[i].OrderNumber, sBuffer[i].data->Shares, sBuffer[i].data->Price, sBuffer[i].data->Stock );
					}
				}
				fclose( fp );
				return ORDER_BUFFER_SUCCESS;
			}
			return ORDER_BUFFER_FILE_SAVE_ERROR;
		}

		case ORDER_BUFFER_SORT:
		{
			snprintf( szBuffer, sizeof(szBuffer), "Sort of %i elements", iLastIndex );
			fnDebug( szBuffer );
			qsort( sBuffer, iLastIndex, sizeof(sBuffer[0]), fnOrderCompare );
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
			snprintf( szOrderBuffer, sizeof(szOrderBuffer), "%s-%04d-%02d-%02d.csv", ORDER_BUFFER_DUMP_FILE, tLocalTime->tm_year+1900, tLocalTime->tm_mon+1, tLocalTime->tm_mday);
			if( fnFileExists( szOrderBuffer ) == FALSE )
				return ORDER_BUFFER_NO_DUMP_FILE;	
			
			fp = fopen( szOrderBuffer, "r" );
			if( fp != NULL )
			{
				fnDebug( "Loading Order Buffer from File" );
				for( i=0; i<ORDER_BUFFER_SIZE; i++ )
				{
					memset( szBuffer, 0, sizeof(szBuffer) );
					if( fgets( szBuffer, sizeof(szBuffer), fp ) == NULL )
						break;
					sBuffer[i].data = (struct OrderData *)malloc( sizeof(struct OrderData) );
					if( sBuffer[i].data != NULL )
					{
						//fnDebug( szBuffer );
						if( strlen(szBuffer) < 10 ) // filter for poorly formatted lines
							continue;
						szTempBuffer = strtok( szBuffer, "," );
						sBuffer[i].OrderNumber = (unsigned)atol( szTempBuffer );
						szTempBuffer = strtok( NULL, "," );
						sBuffer[i].data->Shares = atoi( szTempBuffer );
						szTempBuffer = strtok( NULL, "," );
						sBuffer[i].data->Price = atoi( szTempBuffer );
						szTempBuffer = strtok( NULL, "," );
						strcpy( sBuffer[i].data->Stock, szTempBuffer );
						// Look up the stock index (this may have changed during the restart)
						sBuffer[i].data->StockIndex = fnStockFilter( sBuffer[i].data->Stock );
						iLastIndex = i;
						iOrderBufferSize++;
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

//-----------------------------------------------------------------------------
//	typedef's for the message parser
//-----------------------------------------------------------------------------
typedef union {
        int   value;
        char  string[5];
} bindata;

typedef union {
        unsigned long value;
        char  string[9];
} longbindata;

struct ADD_ORDER_MESSAGE_DATA {
	char Size;
	char Type;
	int	 Time;
	long Order;
	char Indicator;
	int	 Shares;
	char Stock[6];
	int  Price;
};

struct EXECUTED_ORDER_MESSAGE_DATA {
	char Size;
	char Type;
	int  Time;
	long Order;
	int  Shares;
	long Match;
};

struct EXECUTED_ORDER_PRICE_MESSAGE_DATA {
	char Size;
	char Type;
	int  Time;
	long Order;
	int  Shares;
	long Match;
	char Printable;	
	int  Price;
};

struct ORDER_CANCEL_MESSAGE_DATA {
	char Size;
	char Type;
	int  Time;
	long Order;
	int  Shares;
};

struct ORDER_DELETE_MESSAGE_DATA {
	char Size;
	char Type;
	int  Time;
	int  drop1;
	unsigned  Order;
};

struct ORDER_REPLACE_MESSAGE_DATA {
	char Size;
	char Type;
	int  Time;
	long Order;
	long NewOrderNumber;
	int  Shares;
	int  Price;
};

struct NON_CROSS_TRADE_MESSAGE_DATA {
	char Size;
	char Type;
	int  Time;
	unsigned long Order;
	char Indicator;
	int  Shares;
	char Stock[6];
	int  Price;
	long Match;
};

struct CROSS_TRADE_MESSAGE_DATA {
	char Size;
	char Type;
	int  Time;
	int  Shares;
	char Stock[6];
	int  Price;
	long Match;
	char CrossType;
};
	
typedef union {
	struct ADD_ORDER_MESSAGE_DATA a;
	struct EXECUTED_ORDER_MESSAGE_DATA e;
	struct EXECUTED_ORDER_PRICE_MESSAGE_DATA c;
	struct ORDER_CANCEL_MESSAGE_DATA x;
	struct ORDER_DELETE_MESSAGE_DATA d;	
	struct ORDER_REPLACE_MESSAGE_DATA u;
	struct NON_CROSS_TRADE_MESSAGE_DATA p;
	struct CROSS_TRADE_MESSAGE_DATA q;
	char bin[MULTICAST_BUFFER_SIZE];
} ITCH_Message;



//-----------------------------------------------------------------------------
//		Message Parcer - to be run in its own thread - pulls the MDS messages 
//		off the queue and parses them into the HUBB readable text format
//-----------------------------------------------------------------------------
void *fnMessageParser( void )
{
	struct ITCHData sId, tmpOrder;							// ITCH Order Structure
	union binary_int binInt;								// 4 byte union for converting Integers
	union binary_long binLong;								// 8 byte union for converting long integers (64 bit only)
 	char szBuffer[MULTICAST_BUFFER_SIZE];					// Temporary message buffer
	long iFound;
	int n, c;
	int iReturn;
	char buffer[MULTICAST_BUFFER_SIZE];
	unsigned long iOrder;
	unsigned long iNewOrder;
	double fPrice=0;
	unsigned long iMatchNumber=0;
	bindata b;
	longbindata longb;
	struct ITCHData dOrder;
	//struct timeval tv;

	fnDebug( "fnITCHEngineParser Started" );
	
	// Initialize the buffers
	memset( &sId, 0, sizeof(sId));
	memset( &tmpOrder, 0, sizeof(tmpOrder));
	memset( binInt.string, 0, sizeof(binInt.string) );
	memset( binLong.string, 0, sizeof(binLong.string) );
	
	
	// Grab ITCH messages off the queue and process them
	while ( TRUE )
	{
		if ( iITCHQueueSize > 0 )
		{
			//printf("get messages\n");

			// There are entries on the queue so start pulling them off
			memset( buffer, 0, sizeof(buffer) );

			pthread_mutex_lock( &qITCHMessages_mutex );
			iFound = fnQget( &qITCHMessages, buffer );
			if (iFound)
				iITCHQueueSize--;
			else
				iITCHQueueSize = 0;
			pthread_mutex_unlock( &qITCHMessages_mutex );

			//snprintf( szBuffer, sizeof(szBuffer), "fnITCHEngineParser processing message:  type %c ", buffer[1] );
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
							case 'A':	// Add an Order 
							case 'F':  	// Add a Flas Order (same format) 		   
							{           
								for( c=19, n=0; n<ITCH_STOCK_SYMBOL_SIZE; c++, n++ )
									 if( buffer[c] != ' ' )
										 dOrder.Stock[n]=buffer[c];
									 else
									     dOrder.Stock[n]=0;
								
								dOrder.StockIndex = fnStockFilter( dOrder.Stock );
								if( dOrder.StockIndex == STOCK_NOT_FOUND )
									break;
								
								for( c=6, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								dOrder.OrderNumber = longb.value;
								
								for( c=15, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Shares = b.value;
										 
								for( c=25, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Price = b.value;
							
								dOrder.BuySell = buffer[14];

								if( dOrder.Price == 0 || dOrder.OrderNumber == 0 )
									fnHandleError( "fnITCHEngineParser","ITCH Error Detected - price or order number is 0 - discarding packet" );
								else
								{
									pthread_mutex_lock( &qBufferSystem_mutex );
									fnOrderBuffer( ORDER_BUFFER_ADD, &dOrder, dOrder.OrderNumber );
									pthread_mutex_unlock( &qBufferSystem_mutex );
									//snprintf( szBuffer, sizeof(szBuffer), "Add ITCH Order: %s %d @ %f %u", dOrder.Stock, dOrder.Shares, (float)dOrder.Price/10000.00, dOrder.OrderNumber );
									//fnDebug( szBuffer );
								}
								
								break;                  
					
							}
						
							case 'E': //Execute an Order
							{		
								// Grab the order number
								for( c=6, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								dOrder.OrderNumber = longb.value;

								// Check if this is an order we are tracking
								iFound = fnOrderBuffer(ORDER_BUFFER_SEARCH, &tmpOrder, dOrder.OrderNumber );
								if( iFound <= ORDER_NOT_FOUND )
									break;

								// OK we are tracking this order so get the rest of the data
								for( c=14, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Shares = b.value;

								// Check that we have a valid # of shares
								if( dOrder.Shares == 0 )
								{
									fnHandleError( "fnITCHEngineParser", "ITCH Data error detected - 0 Shares - discarding message" );
									break;
								}
								
								for( c=18, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								iMatchNumber = longb.value;
																
								// OK - we have a valid order with good data, grab all the order details!
								pthread_mutex_lock( &qBufferSystem_mutex );
								iReturn = fnOrderBuffer(ORDER_BUFFER_GET_BY_INDEX, &tmpOrder, iFound);
								pthread_mutex_unlock( &qBufferSystem_mutex );
								if(  iReturn > ORDER_NOT_FOUND )
								{
									// Check if we have a valid stock price
									if ( tmpOrder.Price == 0 )
									{
										pthread_mutex_lock( &qBufferSystem_mutex );
										fnOrderBuffer(ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
										pthread_mutex_unlock( &qBufferSystem_mutex );
										fnHandleError ( "fnMessageParser", "Data Error Detected - 0 price - order may have been modified while processing the execute" );
										break;
									}

									//  Good price and data - let's store the price and build the trade
									fPrice = (double)tmpOrder.Price/10000.00;
									
									
									if( dOrder.Shares < tmpOrder.Shares )
									{
										// Found a partial Fill - execute the partial order, update the share count
										tmpOrder.Shares = tmpOrder.Shares - dOrder.Shares;
										pthread_mutex_lock( &qBufferSystem_mutex );
										fnOrderBuffer(ORDER_BUFFER_MODIFY_BY_INDEX, &tmpOrder, iFound );
										pthread_mutex_unlock( &qBufferSystem_mutex );
										snprintf( szBuffer, sizeof(szBuffer), "TU %s %lu %.2f @ Q %i E P %lu ITCH\n", 
													 tmpOrder.Stock, iMatchNumber, fPrice, dOrder.Shares, dOrder.OrderNumber );  
									}
									else
									{
										// All the shares were executed - remove the original order
										pthread_mutex_lock( &qBufferSystem_mutex );
										fnOrderBuffer(ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
										pthread_mutex_unlock( &qBufferSystem_mutex );
										snprintf( szBuffer, sizeof(szBuffer), "TU %s %lu %.2f @ Q %i E A %lu ITCH\n", 
												 tmpOrder.Stock, iMatchNumber, fPrice, dOrder.Shares, dOrder.OrderNumber );  
									}
									if (iClientConnected == TRUE && iSendTU == TRUE )
										fnSrvPush( &qOutMessages, szBuffer );
									if ( tmpOrder.StockIndex == QQQQ )
										fnSaveQVolume( dOrder.Shares );	
									fnMDSLog( szBuffer );

								}
								else
									fnHandleError( "fnMessageParser", "Data error - order may have been deleted during parsing of Execute message" );
								break;                  
					
							}
				
							case 'C': // Execute Partial Order
							{                 										 
								memset( b.string, 0, sizeof(b.string) );
								for( c=6, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								dOrder.OrderNumber = longb.value;
								
								// Check if this is an order we are tracking
								iFound = fnOrderBuffer(ORDER_BUFFER_SEARCH, &tmpOrder, dOrder.OrderNumber );
								if( iFound <= ORDER_NOT_FOUND )
									break;

								// We are tracking this order so get the shares and price
								memset( b.string, 0, sizeof(b.string) );
								for( c=14, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Shares = b.value;

								memset( b.string, 0, sizeof(b.string) );
								for( c=27, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Price = b.value;

								if( dOrder.Shares == 0 || dOrder.Price == 0 )
								{
									fnHandleError( "fnMessageParser", "ITCH Error detected - 0 Price or share count in execute partial message" );
									break;
								}
								
								memset( b.string, 0, sizeof(b.string) );
								for( c=18, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								iMatchNumber = longb.value;
								
								pthread_mutex_lock( &qBufferSystem_mutex );
								iReturn = fnOrderBuffer( ORDER_BUFFER_GET_BY_INDEX, &tmpOrder, iFound);
								pthread_mutex_unlock( &qBufferSystem_mutex );
								if( iReturn > ORDER_NOT_FOUND )
								{
									// Calculate the share price and update the index
									fPrice = (double)dOrder.Price/10000.00;

									/*if( fPrice != fnGetStockPrice(tmpOrder.StockIndex) )
									{
										fnSaveStockPrice( tmpOrder.StockIndex, fPrice );
										fnCalculateIndex();
									}*/
									
									// Check if this is a partial execute
									if( dOrder.Shares < tmpOrder.Shares )
									{
										// Only partial shares executed, update the order to remove the executed shares
										tmpOrder.Shares = tmpOrder.Shares - dOrder.Shares;
										pthread_mutex_lock( &qBufferSystem_mutex );
										fnOrderBuffer( ORDER_BUFFER_MODIFY_BY_INDEX, &tmpOrder, iFound );
										pthread_mutex_unlock( &qBufferSystem_mutex );
										if( dOrder.Price == 0 );
											dOrder.Price = tmpOrder.Price;
										snprintf( szBuffer, sizeof(szBuffer), "TU %s %lu %.3f @ Q %i C P %lu ITCH\n", 
											tmpOrder.Stock, iMatchNumber, fPrice, dOrder.Shares, dOrder.OrderNumber );  
									}
									else
									{
										// The entire order was exected to remove the order
										pthread_mutex_lock( &qBufferSystem_mutex );
										fnOrderBuffer(ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
										pthread_mutex_unlock( &qBufferSystem_mutex );
										if( dOrder.Price == 0 );
											dOrder.Price = tmpOrder.Price;
										snprintf( szBuffer, sizeof(szBuffer), "TU %s %lu %.3f @ Q %i C A %lu ITCH\n", 
											tmpOrder.Stock, iMatchNumber, fPrice, dOrder.Shares, dOrder.OrderNumber );
									}
									// Check if there is a client connected and send this update to the socket server
									if (iClientConnected == TRUE && iSendTU == TRUE )
										fnSrvPush( &qOutMessages, szBuffer );
									if ( tmpOrder.StockIndex == QQQQ )
										fnSaveQVolume( dOrder.Shares );
									
									fnMDSLog( szBuffer );
								} 
								break;                  
							}
							case 'X': // Cancel Shares
							{                             
								memset( b.string, 0, sizeof(b.string) );
								for( c=6, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								dOrder.OrderNumber = longb.value;

								// Check if this is an order we are tracking
								iFound = fnOrderBuffer(ORDER_BUFFER_SEARCH, &tmpOrder, dOrder.OrderNumber );
								if( iFound <= ORDER_NOT_FOUND )
									break;

								// OK we are tracking this order so get the canceled share count
								memset( b.string, 0, sizeof(b.string) );
								for( c=14, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Shares = b.value;
								
								pthread_mutex_lock( &qBufferSystem_mutex );
								iReturn = fnOrderBuffer( ORDER_BUFFER_GET_BY_INDEX, &tmpOrder, iFound );
								pthread_mutex_unlock( &qBufferSystem_mutex );
								if ( iReturn > ORDER_NOT_FOUND )
								{
									// Check if the entire order is canceled or just part of the order
									if( dOrder.Shares < tmpOrder.Shares )
									{
										// This is a partial cancel - remove the canceled shares
										tmpOrder.Shares = tmpOrder.Shares - dOrder.Shares;
										pthread_mutex_lock( &qBufferSystem_mutex );
										fnOrderBuffer(ORDER_BUFFER_MODIFY_BY_INDEX, &tmpOrder, iFound );
										pthread_mutex_unlock( &qBufferSystem_mutex );
									}
									else
									{
										// All the shares were canceled so remove the order
										pthread_mutex_lock( &qBufferSystem_mutex );
										fnOrderBuffer(ORDER_BUFFER_REMOVE_BY_INDEX, &tmpOrder, iFound );
										pthread_mutex_unlock( &qBufferSystem_mutex );
									}
								}	
								break;                  
					
							}
			 
							case 'D':  // Delete an order (moved to another thread)
							{
								memset( b.string, 0, sizeof(b.string) );
								for( c=10, n=0; n<4; c++, n++ )
									 longb.string[n]=buffer[c];
								dOrder.OrderNumber = longb.value;
								
								iFound = fnOrderBuffer(ORDER_BUFFER_SEARCH, NULL, dOrder.OrderNumber );
								if( iFound > ORDER_NOT_FOUND )
								{
									pthread_mutex_lock( &qBufferSystem_mutex );
									fnOrderBuffer(ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
									pthread_mutex_unlock( &qBufferSystem_mutex );
								}
								break;                  
					
							}
								 
							case 'U':	// Update an order
							{
								memset( b.string, 0, sizeof(b.string) );
								for( c=6, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								iOrder = longb.value;

								// Check if this is an order we are tracking
								iFound = fnOrderBuffer(ORDER_BUFFER_SEARCH, &tmpOrder, dOrder.OrderNumber );
								if( iFound <= ORDER_NOT_FOUND )
									break;
								
								// OK we are tracking this order so get the rest of the information on the order
								memset( b.string, 0, sizeof(b.string) );
								for( c=14, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								iNewOrder = longb.value;

								memset( b.string, 0, sizeof(b.string) );
								for( c=22, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Shares = b.value;
								
								memset( b.string, 0, sizeof(b.string) );
								for( c=26, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Price = b.value;

								// Check for bad data
								if( iNewOrder==0 || dOrder.Shares==0 || dOrder.Price==0 )
								{
									fnHandleError ( "fnMessageParser", "ITCH data error - 0 price or order number found in update order message - discarding" );
									break;
								}
								
								pthread_mutex_lock( &qBufferSystem_mutex );
								iReturn = fnOrderBuffer( ORDER_BUFFER_GET_BY_INDEX, &tmpOrder, iFound );
								pthread_mutex_unlock( &qBufferSystem_mutex );
								if( iReturn > ORDER_NOT_FOUND )
								{
									// Added some tracking here to figure out how the new order number works in the sequence of orders
									snprintf( szBuffer, sizeof(szBuffer), "ITCH Update Order Message: old order %lu  new order %lu", tmpOrder.OrderNumber, iNewOrder );
									fnDebug( szBuffer );
									// Update the order then save it into the order buffer - delete the old order
									tmpOrder.OrderNumber = iNewOrder;
									tmpOrder.Shares = dOrder.Shares;
									tmpOrder.Price = dOrder.Price;
									pthread_mutex_lock( &qBufferSystem_mutex );
									fnOrderBuffer(ORDER_BUFFER_ADD, &tmpOrder, tmpOrder.OrderNumber );
									fnOrderBuffer(ORDER_BUFFER_REMOVE_BY_INDEX, &tmpOrder, iFound );
									pthread_mutex_unlock( &qBufferSystem_mutex );
								}
			 
								break;                  
					
							}
							case 'P':	// Execute order with Price   		   
							{           										 
								memset( dOrder.Stock, 0, sizeof(dOrder.Stock) );
								for( c=19, n=0; n<ITCH_STOCK_SYMBOL_SIZE; c++, n++ )
									 if( buffer[c] != ' ' )
										 dOrder.Stock[n]=buffer[c];
								
								// Check if this is a stock we are tracking
								dOrder.StockIndex = fnStockFilter( dOrder.Stock );
								if( dOrder.StockIndex == STOCK_NOT_FOUND )
									break;

								// We are tracking this stock so get the rest of the data
								memset( b.string, 0, sizeof(b.string) );
								for( c=6, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								dOrder.OrderNumber = longb.value;
								
								memset( b.string, 0, sizeof(b.string) );
								for( c=15, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Shares = b.value;								 
												 
								memset( b.string, 0, sizeof(b.string) );
								for( c=25, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Price = b.value;
							
								// Check for bad data
								if( dOrder.OrderNumber==0 || dOrder.Shares==0 || dOrder.Price==0 )
								{
									fnHandleError ( "fnMessageParser", "ITCH data error - 0 price or order number found in execute with price message - discarding" );
									break;
								}

								memset( b.string, 0, sizeof(b.string) );
								for( c=29, n=0; n<8; c++, n++ )
									 longb.string[n]=buffer[c];
								iMatchNumber = longb.value;

								fPrice = (double)dOrder.Price/10000.00 ;
								/*if( fPrice != fnGetStockPrice(dOrder.StockIndex) )
								{
									fnSaveStockPrice( dOrder.StockIndex, fPrice );
									fnCalculateIndex();
								}*/
								snprintf( szBuffer, sizeof(szBuffer), "TU %s %lu %.3f @ Q %i P A %lu ITCH\n", 
										 dOrder.Stock, iMatchNumber, fPrice, dOrder.Shares, dOrder.OrderNumber );  
								if (iClientConnected == TRUE && iSendTU == TRUE )
									fnSrvPush( &qOutMessages, szBuffer );
								if ( dOrder.StockIndex == QQQQ )
									fnSaveQVolume( dOrder.Shares );
								fnMDSLog ( szBuffer );
								break;                  
					
							}
							case 'Q':   // Execute Hidden Order		   
							{           
								memset( dOrder.Stock, 0, sizeof(dOrder.Stock) );
								for( c=14, n=0; n<ITCH_STOCK_SYMBOL_SIZE; c++, n++ )
									 if( buffer[c] != ' ' )
										 dOrder.Stock[n]=buffer[c];

								// Check if this is a stock we are tracking
								dOrder.StockIndex = fnStockFilter( dOrder.Stock );
								if( dOrder.StockIndex == STOCK_NOT_FOUND )
									break;

								// OK - we are tracking this so get the rest of the data
								memset( b.string, 0, sizeof(b.string) );
								for( c=10, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Shares = b.value;		 
												 
								memset( b.string, 0, sizeof(b.string) );
								for( c=20, n=0; n<4; c++, n++ )
									 b.string[n]=buffer[c];
								dOrder.Price = b.value;
							
								// Check for bad data
								if( dOrder.Shares==0 || dOrder.Price==0 )
								{
									fnHandleError ( "fnMessageParser", "ITCH data error - 0 price or order number found in execute hidden order - discarding" );
									break;
								}

								memset( b.string, 0, sizeof(b.string) );
								for( c=24, n=0; n<8; c++, n++ )
									 b.string[n]=buffer[c];
								iMatchNumber = b.value;

								fPrice = (double)dOrder.Price/10000.00;
								/*if( fPrice != fnGetStockPrice(dOrder.StockIndex) )
								{
									fnSaveStockPrice( dOrder.StockIndex, fPrice );
									fnCalculateIndex();									
								}*/
								snprintf( szBuffer, sizeof(szBuffer), "TU %s %lu %.3f @ Q %i H A %lu ITCH\n", 
										 dOrder.Stock, iMatchNumber, fPrice, dOrder.Shares, dOrder.OrderNumber );  
								if ( iClientConnected == TRUE && iSendTU == TRUE )
									fnSrvPush( &qOutMessages, szBuffer );
								if ( dOrder.StockIndex == QQQQ )
									fnSaveQVolume( dOrder.Shares );
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
								printf( "Unknown Msg from Queue: Time: %2d:%6d : ", (int)tv.tv_sec, (int)tv.tv_usec );
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
	}		

    pthread_exit(NULL);

}

//-----------------------------------------------------------------------------
//		Delete Message Parcer - to be run in its own thread - pulls the 
//		ITCH delete messages off the queue and processes them
//-----------------------------------------------------------------------------
void *fnDeleteMessageEngineParser( void )
{
 	char szBuffer[MULTICAST_BUFFER_SIZE];					// Temporary message buffer
	long iFound;
	ITCH_Message ITCH;
	int n, c;
	longbindata longb;
	
	fnDebug( "fnDelteOrderEngineParser Started" );
			
	// Grab Delete ITCH messages off the queue and process them
	while ( TRUE )
	{
		// If the Order Buffer is out of order then don't process any delete messages
		if ( iDeleteQueueSize > 0 && iOrderBufferSort == 0 )
		{
				// There are entries on the queue so start pulling them off
				//fnDebug( "fnDelteMessageEngineParser processing message" );

				// Lock the delete message queue and pull off the top entry
				pthread_mutex_lock( &qDeleteMessages_mutex );
				iFound = fnQget( &qDeleteMessages, ITCH.bin );
				if ( iFound )
					iDeleteQueueSize--;
				pthread_mutex_unlock( &qDeleteMessages_mutex );
				sched_yield();

				if( iFound )
				{					
					// Get the order from the message - find it in the order buffer
					// then delete the message
					for( c=6, n=0; n<8; c++, n++ )
						 longb.string[n]=ITCH.bin[c];
					sched_yield();
					
					iFound = fnOrderBuffer(ORDER_BUFFER_SEARCH, NULL, longb.value);
					sched_yield();
					
					if( iFound > ORDER_NOT_FOUND )
					{
						pthread_mutex_lock( &qBufferSystem_mutex );
						iFound = fnOrderBuffer(ORDER_BUFFER_REMOVE_BY_INDEX, NULL, iFound );
						pthread_mutex_unlock( &qBufferSystem_mutex );
						sched_yield();
					}
					else
						sched_yield();
				}
				else
				{
					// There were no messages on the queue so allow the other threads to execute
					sched_yield();
					usleep(100);
				}
		}	
		else
		{
			// There were no messages on the queue so allow the other threads to execute
			usleep(100);
			sched_yield();
		}
	}		
    pthread_exit(NULL);
}
	

//-----------------------------------------------------------------------------
// Filter the multicast messages into ITCH messages
//-----------------------------------------------------------------------------
void fnFilterMessages( char* szBuffer )
{
	static char *szMessage;
	static int iMsgSize;
	static int iNumMessages;
	static int m;
	//static char szTmpBuffer[BUFFER_SIZE];

	// Grab the number of messages in this buffer
	iNumMessages = (int)szBuffer[19];
	
	// Jump past the multicast header and start splitting up messages
	szMessage = szBuffer + MULTICAST_MSG_HEADER_SIZE;
	//fnDebug( "fnFilter message" );

	
	//iMsgSize = MULTICAST_MSG_HEADER_SIZE;	
	for ( m=0; m<=iNumMessages; m++ )
	{
		//memcpy( szTempBuffer, byMessage, byMessage[0]+1 );
		iMsgSize = (int)szMessage[0];
		if ( iMsgSize == 0 )
			break;
		//snprintf( szTmpBuffer, sizeof(szTmpBuffer), "Msg num: %d Msg size: %.2X  Msg type: %.2X", m, szMessage[0], szMessage[1] );
		//fnDebug( szTmpBuffer );

		// Filter out messages by type
		switch( szMessage[1] )
		{
			// Messages that require action
			case ( ITCH_ADD_ORDER_MESSAGE ):
			case ( ITCH_ADD_ORDER_MPID_MESSAGE ):
			case ( ITCH_EXECUTED_ORDER_MESSAGE ):
			case ( ITCH_EXECUTED_ORDER_PRICE_MESSAGE ):
			case ( ITCH_ORDER_REPLACE_MESSAGE ):           
			case ( ITCH_TRADE_MESSAGE ):
			case ( ITCH_ORDER_CANCEL_MESSAGE ):
				pthread_mutex_lock( &qITCHMessages_mutex );
				fnQput( &qITCHMessages, szMessage );	// Push this message onto the queue for processing
				iITCHQueueSize++;
				// Set queuing Flag
				if ( iITCHQueueSize > ITCH_MESSAGE_BUFFERING_COUNT )
					iMessagesQueued = TRUE;
				else
					iMessagesQueued = FALSE;
				pthread_mutex_unlock( &qITCHMessages_mutex );
				break;
			case ( ITCH_ORDER_DELETE_MESSAGE ):
				pthread_mutex_lock( &qDeleteMessages_mutex );
				fnQput( &qDeleteMessages, szMessage );	// Push this message onto the delete queue for processing
				iDeleteQueueSize++;
				pthread_mutex_unlock( &qDeleteMessages_mutex );
				break;
			// Messages that can be discarded for now
			case ( ITCH_BROKEN_TRADE_MESSAGE ):
			case ( ITCH_NET_ORDER_INBALANCE_MESSAGE ):	// pre/after market inbalances
			case ( ITCH_TIME_MESSAGE ):					// NASDAQ timestamps **** need to handle this later
			case ( ITCH_SYSTEM_MESSAGE ):				// System Alerts **** need to handle this later
			case ( ITCH_DIRECTORY_MESSAGE ):			// Stock Directory Premarket
			case ( ITCH_STOCK_TRADING_MESSAGE ):		// Stock Trading Premarket
			case ( ITCH_STOCK_PARTICIPANT_MESSAGE ):	// Marketmaker books Premarket		
			case ( ITCH_CROSS_TRADE_MESSAGE ):
			default:
				//fnDebug( "fnFilter discarded message" );
				break;
		}
		// Move to the next message in the buffer 
		szMessage = szMessage + iMsgSize +2;
	}
	return;
}

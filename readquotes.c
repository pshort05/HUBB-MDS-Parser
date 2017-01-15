#include "main.h"
#include "readquotes.h"

#define QUOTE_THREADS	6

#define MAXRECVSTRING 	4096  /* Longest string to receive */

#define DEFAULT_GROUP	0xe0027fff
#define DEFAULT_PORT	9876
#define MAXPDU 			4096
#define WIDTH 			16

#define SHORT_MSG_SIZE	44
#define LONG_MSG_SIZE	66

#define STRING_SIZE		32
#define BUFFER_SIZE		4096

#define READ_BINARY		1

double divisor[3] = {100.0, 1000.0, 10000.0};

struct q_head qQuoteMessages;
pthread_mutex_t qQuoteMessages_mutex = PTHREAD_MUTEX_INITIALIZER;
int iQuoteQueueSize;

// Local Function Prototypes
void *fnQuoteMulticastReader( void* data );
void *fnQuoteMessageParser( void );

// Local Data
pthread_t pReadQuotes[QUOTE_THREADS];
pthread_t pParseQuotes;
pthread_attr_t paReadQuotes;
int iQuotesRunning;

// ----------------------------------------------------------------------------
//		Function to start all the Quotes threads
// ----------------------------------------------------------------------------
void fnStartQuotes( void )
{
	int i=1;
	
	iQuotesRunning = TRUE;
	fnQinit( &qQuoteMessages );

	pthread_attr_init( &paReadQuotes);
	pthread_attr_setdetachstate( &paReadQuotes, PTHREAD_CREATE_DETACHED );
	pthread_create( &pParseQuotes, &paReadQuotes, (void *)fnQuoteMessageParser, NULL );

	for( i=0; i<QUOTE_THREADS; i++ )
	{	
		pthread_attr_init( &paReadQuotes);
		pthread_attr_setdetachstate( &paReadQuotes, PTHREAD_CREATE_DETACHED );
		pthread_create( &pReadQuotes[i], &paReadQuotes, (void *)fnQuoteMulticastReader,  (void*)&i );
		usleep(10000);	
	}

	fnDebug("Quote Engine Started");
}

// ----------------------------------------------------------------------------
//		Function to shutdown Quotes threads
// ----------------------------------------------------------------------------
void fnShutdownQuotes( void )
{
	int i;
	
	//iQuotesRunning = FALSE;
	usleep(10000);
	for( i=0; i<QUOTE_THREADS; i++ )
	{
		pthread_cancel( pReadQuotes[i] );
		usleep(10);
	}

	pthread_cancel( pParseQuotes );
	fnQinit( &qQuoteMessages );
	
	fnDebug("Quote Engine Stopped");
}


// ----------------------------------------------------------------------------
// 		Function that will read the raw multicast packets off the port and break them
// 		into individual packets
// ----------------------------------------------------------------------------
void *fnQuoteMulticastReader( void* data )
{
    int sock;                         	/* Socket */
    struct sockaddr_in name; 			/* Multicast Address */
    int recvStringLen;                	/* Length of received string */
    struct ip_mreq imr;  				/* Multicast address join structure */
	char byBuffer[MULTICAST_BUFFER_SIZE];
	char szBuffer[MULTICAST_BUFFER_SIZE];
	//char szIP[BUFFER_SIZE];
	//char szPort[BUFFER_SIZE];
	//char szInterface[BUFFER_SIZE];
	char *interface = NULL;
	char *msg;
	short int n, c;

	int me = *((int*)data);     /* thread identifying number */
	//char szFName[BUFFER_SIZE];
	//char szTemp[BUFFER_SIZE];

	u_long groupaddr = DEFAULT_GROUP;
	u_short groupport = DEFAULT_PORT;	


	// Grab the Multicast configuration from the CFG file
	/*
	pthread_mutex_lock( &config_mutex );
	memset( szIP, 0, sizeof(szIP) );
	snprintf( szTemp, sizeof(szTemp), "QUOTE%iMULTICASTSERVER", me);
	fnGetConfigSetting( szIP, szTemp, "224.0.17.48");
	groupaddr = inet_addr( szIP );
	
	snprintf( szTemp, sizeof(szTemp), "QUOTE%iMULTICASTPORT", me);
	memset( szPort, 0, sizeof(szPort) );
	fnGetConfigSetting( szPort, szTemp, "55530" );
	groupport = (u_short)atoi(szPort);
	
	snprintf( szTemp, sizeof(szTemp), "QUOTE%iINTERFACE", me);
	memset( szInterface, 0, sizeof(szInterface) );
	fnGetConfigSetting( szInterface, szTemp, "10.210.35.37");
	interface=szInterface;
	pthread_mutex_unlock( &config_mutex );
	*/


	switch( me )
	{
		case 0:
		{
			groupaddr = inet_addr("224.0.17.48");
			groupport = (u_short)55530;
			break;
		}
		case 1:
		{
			groupaddr = inet_addr("224.0.17.50");
			groupport = (u_short)55532;
			break;
		}
		case 2:
		{
			groupaddr = inet_addr("224.0.17.52");
			groupport = (u_short)55534;
			break;
		}
		case 3:
		{
			groupaddr = inet_addr("224.0.17.54");
			groupport = (u_short)55536;
			break;
		}
		case 4:
		{
			groupaddr = inet_addr("224.0.17.56");
			groupport = (u_short)55538;
			break;
		}
		case 5:
		{
			groupaddr = inet_addr("224.0.17.58");
			groupport = (u_short)55540;
			break;
		}
		default:
		{
			fnHandleError( "fnQuoteMulticastReader", "Unknown thread number - exiting thread" );
			pthread_exit(NULL);	
			break;
		}
	}
	interface="10.210.105.109";
			
    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        fnHandleError( "fnQuotesMulticastReader", "socket() failed");
	else
		fnDebug( "Quotes Create socket success" );

	/* Create multicast structure */
    imr.imr_multiaddr.s_addr = groupaddr;
    imr.imr_multiaddr.s_addr = htonl(imr.imr_multiaddr.s_addr);
    if (interface!=NULL) 
        imr.imr_interface.s_addr = inet_addr(interface);
	else 
		imr.imr_interface.s_addr = htonl(INADDR_ANY);
    imr.imr_interface.s_addr = htonl(imr.imr_interface.s_addr);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq)) < 0 ) 
		fnHandleError( "fnQuotesMulticastReader", "setsockopt - IP_ADD_MEMBERSHIP");
	else
		fnDebug( "Quotes Set socket options success" );

	/* Bind Socket */
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(groupaddr);
    name.sin_port = htons(groupport);
    if (bind(sock, (struct sockaddr *)&name, sizeof(name))) 
		fnHandleError( "fnQuotesMulticastReader", "bind failed");
	else
		fnDebug( "Quotes Bind socket success" );

	snprintf( szBuffer, sizeof(szBuffer), "Quote Reader Thread %i Initialized - reading data", me );
	fnDebug( szBuffer );
	
	while ( iQuotesRunning )
	{

    	// Receive a datagram from the server
		//fnDebug( "Get Quote" );
		if ((recvStringLen = recv(sock, (char *)byBuffer, sizeof(byBuffer)-1, 0)) < 0)
        	fnHandleError( "fnQuotesMulticastReader", "recv() failed");
		//fnDebug( "Got Quote" );



		// Break the TCP message into individual quotes and push them onto the queue.
		// Filter out any restransmission requests and update the first byte with the
		// message size.
		msg = byBuffer;
		for( n=0, c=0; n<recvStringLen; n++, c++ )
		{
			// Check for the end of a quote
			if( byBuffer[n] == 0x1F )
			{
				// Filter out messages that we don't want to evaluate
				if ( msg[1] == 0x51 && msg[4] == 0x4F )
				{
					msg[0] = (char)c;
					//fnDebug( "Pushed Message onto Queue" );
					pthread_mutex_lock( &qQuoteMessages_mutex );
					fnQput( &qQuoteMessages, msg );
					iQuoteQueueSize++;
					pthread_mutex_unlock( &qQuoteMessages_mutex );
				}
				//else
				//	fnDebug( "Message Skipped" );
				c=0;
				msg = byBuffer+n;
			}
			// Check for the end of the TCP packet
			else if( byBuffer[n] == 0x03 )
			{
				// Filter out messages that we don't want to evaluate
				if ( msg[1] == 0x51 && msg[4] == 0x4F )
				{
					msg[0] = (char)c;
					//fnDebug( "Pushed Message onto Queue" );
					pthread_mutex_lock( &qQuoteMessages_mutex );
					fnQput( &qQuoteMessages, msg );
					iQuoteQueueSize++;
					pthread_mutex_unlock( &qQuoteMessages_mutex );
				}
				//else
				//	fnDebug( "Message Skipped" );
				break;
			}	
		}

		memset( byBuffer, 0, recvStringLen+1);
		
	}

	snprintf( szBuffer, sizeof(szBuffer), "Quote Reader Thread %i closed", me );
	fnDebug( szBuffer );
	
    close(sock);
	pthread_exit(NULL);		
}

// ----------------------------------------------------------------------------
// 		This will read the queue of quotes and process them
// ----------------------------------------------------------------------------
void *fnQuoteMessageParser( void )
{
	char *stock=NULL;
	char *price=NULL;
	char *time=NULL;
	char *size=NULL;
	//char *next=NULL;

	char buffer[MULTICAST_BUFFER_SIZE];
	char szBuffer[MULTICAST_BUFFER_SIZE];
	char szPrice[STRING_SIZE];
	char szSize[STRING_SIZE];
	int iStockIndex;

	double fbidPrice=0.00;
	double faskPrice=0.00;
	int iAskSize=0;
	int iBidSize=0;
	int c;
	int iFound;
	int iDelayQuotes;

	char *header = NULL;
	char *trademsg = NULL;
	char *appendage=NULL;
	//struct timeval tv;

	fnDebug ( "Quotes Message Parser Started" );

	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting ( szBuffer, "DELAYQUOTES", "1" );
	iDelayQuotes = atoi( szBuffer );
	pthread_mutex_unlock( &config_mutex );

	
	// Grab BATS messages off the queue and process them
	while ( iQuotesRunning )
	{
		if ( iQuoteQueueSize > 0 )
		{

			// There are entries on the queue so start pulling them off
			memset( buffer, 0, sizeof(buffer) );

			pthread_mutex_lock( &qQuoteMessages_mutex );
			iFound = fnQget( &qQuoteMessages, buffer );
			if ( iFound )
				iQuoteQueueSize--;
			else
				iQuoteQueueSize = 0;
			pthread_mutex_unlock( &qQuoteMessages_mutex );
			if ( !iFound )
			{
				sched_yield();
				continue;
			}

			fbidPrice=0.00;
			faskPrice=0.00;
			iAskSize=0;
			iBidSize=0;
            memset( szPrice, 0, sizeof(szPrice) );
            memset( szSize, 0, sizeof(szSize) );
			
			header = buffer;

			switch ( buffer[2] )
			{
				// -----------------------------	
				// Process Short Form Quotes Here
				// -----------------------------	
				case 0x43:
				{
						trademsg = header+25;
						stock = trademsg;
					
						// Get the stock symbol first
						for( c=0; c<6; c++ )
						{
							if ( stock[c] == 0x20 )
							{
								stock[c]=0x00;
								break;
							}
						}
						header[30] = 0x00;

						// See if this is a stock we are tracking
						iStockIndex = fnStockFilter( stock );
						if( iStockIndex != STOCK_NOT_FOUND )
						{
							memset( szPrice, 0, STRING_SIZE );
							memset( szSize, 0, STRING_SIZE );
					
							// Only grab the NBBO quotes and filter out all the rest
							switch ( trademsg[24] )
							{	
								// The NBBO is in the short appendage
								case '2':		
								{
									appendage = trademsg+26;
								
									time = header+15;
									price = appendage+3;
									size = appendage+9;

									strncpy( szPrice, price, 6 );
									strncpy( szSize, size, 2 );
									iBidSize = atoi( szSize ) * 100;			
								
									fbidPrice = atof( szPrice ) /divisor[appendage[2]-66];

                                    price = appendage+14;
									size = appendage+20;

									strncpy( szPrice, price, 6 );
									strncpy( szSize, size, 2 );
									iAskSize = atoi( szSize ) * 100;
									faskPrice = atof( szPrice ) /divisor[appendage[13]-66];

								    snprintf( szBuffer, sizeof(szBuffer), "IU %s S S %.2f %d %.2f %d U %c\n", stock, fbidPrice, iBidSize, faskPrice, iAskSize, header[14] );
								    if (iClientConnected == TRUE && iSendIU == TRUE )
									    fnSrvPush( &qOutMessages, szBuffer );
								    fnMDSLog( szBuffer );
									break;
								}
								
								// The NBBO is in the Long Appendage
								case '3':
								{	
									appendage = trademsg+26;
								
									time = header+15;
									price = appendage+3;
									size = appendage+13;

									strncpy( szPrice, price, 10 );
									strncpy( szSize, size, 7 );
									iBidSize = atoi( szSize );			
								
									fbidPrice = atof( szPrice ) /divisor[appendage[2]-66];
							
									price = appendage+23;
									size = appendage+33;

									strncpy( szPrice, price, 10 );
									strncpy( szSize, size, 7 );
									iAskSize = atoi( szSize );			
								
									faskPrice = atof( szPrice ) /divisor[appendage[22]-66];
								
								    snprintf( szBuffer, sizeof(szBuffer), "IU %s S L %.2f %d %.2f %d U %c\n", stock, fbidPrice, iBidSize, faskPrice, iAskSize, header[14] );
								    if (iClientConnected == TRUE && iSendIU == TRUE )
									    fnSrvPush( &qOutMessages, szBuffer );
								    fnMDSLog( szBuffer );
									break;
								}

								// NBBO Price is in the quote
								case '4':
								{

									time = header+15;
									price = trademsg+7;
									size = trademsg+13;

									memset( szPrice, 0, STRING_SIZE );
									memset( szSize, 0, STRING_SIZE );

									strncpy( szPrice, price, 6 );
									strncpy( szSize, size, 2 );
									iBidSize = atoi( szSize )*100;			

									fbidPrice = atof( szPrice ) /divisor[trademsg[6]-66];
							
									memset( szPrice, 0, STRING_SIZE );
									memset( szSize, 0, STRING_SIZE );

									price = trademsg+16;
									size = trademsg+22;
									strncpy( szPrice, price, 6 );
									strncpy( szSize, size, 2 );
									iAskSize = atoi( szSize )*100;			

									faskPrice = atof( szPrice ) /divisor[trademsg[15]-66];

								    snprintf( szBuffer, sizeof(szBuffer), "IU %s S B %.2f %d %.2f %d U %c\n", stock, fbidPrice, iBidSize, faskPrice, iAskSize, header[14]);
								    if (iClientConnected == TRUE && iSendIU == TRUE )
									    fnSrvPush( &qOutMessages, szBuffer );
								    fnMDSLog( szBuffer );

									break;
								}
								default:
			                        if( iDelayQuotes == 1 )
				                        sched_yield();
				                    break;
							}
						}
			        if( iDelayQuotes == 1 )
				        sched_yield();
					break;
				} // end of short form quote processing

				// -----------------------------	
				// Process Long Form Quotes Here
				// -----------------------------	
				case 0x44:
				{
						trademsg = header+25;
						stock = trademsg;

						// Grab the stock symbol
						for( c=0; c<6; c++ )
						{
							if ( stock[c] == 0x20 )
							{
								stock[c]=0x00;
								break;
							}
						}

						// See if this is a stock we are tracking
						iStockIndex = fnStockFilter( stock );
						if( iStockIndex == STOCK_NOT_FOUND )
						{
							switch ( trademsg[51] )
							{
								// The NBBO is in the short appendage
								case '2':
								{
									appendage = trademsg+53;
								
									time = header+15;
									price = appendage+3;
									size = appendage+9;

									strncpy( szPrice, price, 6 );
									strncpy( szSize, size, 2 );
									iBidSize = atoi( szSize )*100;			
								
									fbidPrice = atof( szPrice ) /divisor[appendage[2]-66];
							
									price = appendage+14;
									size = appendage+20;

									strncpy( szPrice, price, 6 );
									strncpy( szSize, size, 2 );
									iAskSize = atoi( szSize )*100;			
								
									faskPrice = atof( szPrice ) /divisor[appendage[22]-66];

								    snprintf( szBuffer, sizeof(szBuffer), "IU %s L S %.2f %d %.2f %d U %c\n", stock, fbidPrice, iBidSize, faskPrice, iAskSize, header[14] );
								    if (iClientConnected == TRUE && iSendIU == TRUE )
									    fnSrvPush( &qOutMessages, szBuffer );
								    fnMDSLog( szBuffer );
									break;
								}

								// The NBBO is in the long appendage	
								case '3':	
								{	
									appendage = trademsg+53;

									time = header+15;
									price = appendage+3;
									size = appendage+13;

									strncpy( szPrice, price, 10 );
									strncpy( szSize, size, 7 );
									iBidSize = atoi( szSize );			
								
									fbidPrice = atof( szPrice ) /divisor[appendage[2]-66];
							
									price = appendage+23;
									size = appendage+33;

									strncpy( szPrice, price, 10 );
									strncpy( szSize, size, 7 );
									iAskSize = atoi( szSize );			
								
									faskPrice = atof( szPrice ) /divisor[appendage[22]-66];
								
								    snprintf( szBuffer, sizeof(szBuffer), "IU %s L L %.2f %d %.2f %d U %c\n", stock, fbidPrice, iBidSize, faskPrice, iAskSize, header[14] );
								    if (iClientConnected == TRUE && iSendIU == TRUE )
									    fnSrvPush( &qOutMessages, szBuffer );
								    fnMDSLog( szBuffer );
									break;
								}

                                // The Quote is the NBBO
								case '4' :
								{
                                    // Update the string pointers
                                    time = header+15;
									price = trademsg+13;
									size = trademsg+23;

									memset( szPrice, 0, STRING_SIZE );
									memset( szSize, 0, STRING_SIZE );

                                    // grab the bid price and size
									strncpy( szPrice, price, 10 );
									strncpy( szSize, size, 7 );
									iBidSize = atoi( szSize );			

									fbidPrice = atof( szPrice ) /divisor[trademsg[12]-66];

                                    // grab the ask price and size
									price = trademsg+31;
									size = trademsg+41;
									strncpy( szPrice, price, 10 );
									strncpy( szSize, size, 7 );
									iAskSize = atoi( szSize );			

									faskPrice = atof( szPrice ) /divisor[trademsg[30]-66];

                                    // Format the MDS string and send to the socker server and the MDS log file
								    snprintf( szBuffer, sizeof(szBuffer), "IU %s L B %.2f %d %.2f %d U %c\n", stock, fbidPrice, iBidSize, faskPrice, iAskSize, header[14] );
								    if (iClientConnected == TRUE && iSendIU == TRUE )
									    fnSrvPush( &qOutMessages, szBuffer );
								    fnMDSLog( szBuffer );
                                    break;
                                }
								default:
			                        if( iDelayQuotes == 1 )
				                        sched_yield();
				                    break;
							}
						}
			            if( iDelayQuotes == 1 )
				            sched_yield();
						break;
				} // end of long form processing
					
				default:
				{
					snprintf( szBuffer, sizeof(szBuffer), "Invalid quote type: %2X  Message: %.48s", buffer[2], buffer );
					fnHandleError ( "fnQuoteMessageParser", szBuffer );
                    sched_yield();
					break;
				}

			}
            // Zero out the used part of the buffer
            memset( buffer, 0, buffer[0] );
			if( iDelayQuotes == 1 )
				sched_yield();
			
		}
		// No messages found to process
		else
		{
			sched_yield();
			if( iDelayQuotes == 1 )
				usleep(100);
		}
	}

	fnDebug( "Quote Message Parser Thread shutdown" );
	pthread_exit(NULL);	
}


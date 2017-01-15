#include "main.h"
#include "gidsreader.h"
#include "arcareader.h"
#include <errno.h>

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


// Thread pointers for ARCA
pthread_t pGIDSMulticastReader;

void fnStartGIDS( void )
{
	pthread_attr_t	paGIDSMulticastReader;

	pthread_attr_init( &paGIDSMulticastReader);
	pthread_attr_setdetachstate( &paGIDSMulticastReader, PTHREAD_CREATE_DETACHED );
	pthread_create( &pGIDSMulticastReader, &paGIDSMulticastReader, (void *)fnGIDSMulticastReader, NULL );
	fnDebug( "GIDS Message Parser Thread Initialized" );
}



void fnShutdownGIDS( void )
{
	pthread_cancel( pGIDSMulticastReader );
	fnDebug( "GIDS reader functions shut down" );
}



// ----------------------------------------------------------------------------
// 		Function that will read the raw multicast packets off the port and break them
// 		into individual packets
// ----------------------------------------------------------------------------
void *fnGIDSMulticastReader( void )
{
    int sock;                         	/* Socket */
    struct sockaddr_in name; 			/* Multicast Address */
    int recvStringLen;                	/* Length of received string */
    struct ip_mreq imr;  				/* Multicast address join structure */
	char byBuffer[MULTICAST_BUFFER_SIZE];
	char szBuffer[MULTICAST_BUFFER_SIZE];
	char szIP[BUFFER_SIZE];
	char szPort[BUFFER_SIZE];
	char szInterface[BUFFER_SIZE];
	char *interface = NULL;
	short int n, c;
	char *msg;
	int err=0;

	u_long groupaddr = DEFAULT_GROUP;
	u_short groupport = DEFAULT_PORT;	


	pthread_mutex_lock( &config_mutex );
	memset( szIP, 0, sizeof(szIP) );
	fnGetConfigSetting( szIP, "GIDSMULTICASTSERVER ", "224.3.0.26");
	groupaddr = inet_addr( szIP );
	
	memset( szPort, 0, sizeof(szPort) );
	fnGetConfigSetting( szPort, "GIDSMULTICASTPORT", "55368" );
	groupport = (u_short)atoi(szPort);
	
	memset( szInterface, 0, sizeof(szInterface) );
	fnGetConfigSetting( szInterface, "GIDSINTERFACE", "10.210.105.37");
	interface=szInterface;
	pthread_mutex_unlock( &config_mutex );

	
    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        fnHandleError( "fnGIDSMulticastReader", "socket() failed");
	else
		fnDebug( "GIDS Create socket success" );

	/* Create multicast structure */
    imr.imr_multiaddr.s_addr = groupaddr;
    imr.imr_multiaddr.s_addr = htonl(imr.imr_multiaddr.s_addr);
    if (interface!=NULL) 
        imr.imr_interface.s_addr = inet_addr(interface);
	else 
		imr.imr_interface.s_addr = htonl(INADDR_ANY);
    imr.imr_interface.s_addr = htonl(imr.imr_interface.s_addr);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq)) < 0 ) 
		fnHandleError( "fnGIDSMulticastReader", "setsockopt - IP_ADD_MEMBERSHIP");
	else
		fnDebug( "GIDS Set socket options success" );

	/* Bind Socket */
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(groupaddr);
    name.sin_port = htons(groupport);
    if (bind(sock, (struct sockaddr *)&name, sizeof(name))) 
	{
		err = errno;
		snprintf( szBuffer, sizeof(szBuffer), "GIDS bind failed with errno: %i.  Terminating thread", err );
		fnHandleError( "fnGIDSMulticastReader", szBuffer );
		close( sock );
		pthread_exit( NULL );
	}
	else
		fnDebug( "GIDS Bind socket success" );

	fnDebug( "GIDS Reader Thread Initialized - reading data" );
	
	while ( TRUE )
	{

    	// Receive a datagram from the server
		//fnDebug( "Get Quote" );
		if ((recvStringLen = recv(sock, (char *)byBuffer, sizeof(byBuffer)-1, 0)) < 0)
        	fnHandleError( "fnGIDSMulticastReader", "recv() failed");
		//fnDebug( "Got Quote" );



		// Break the TCP message into individual ticks and push them onto the queue.
		// Filter out any restransmission requests and update the first byte with the
		// message size and second byte with the message type
		msg = byBuffer;
		for( n=0, c=0; n<recvStringLen; n++, c++ )
		{
			// Check for the end of a quote
			if( byBuffer[n] == 0x1F )
			{
				// Filter out messages that we don't want to evaluate
				if ( msg[1] == 0x50 )
				{
					msg[0] = (char)c;
					msg[1] = GIDS_MESSAGE_TYPE;
					//fnDebug( "Pushed Message onto Queue" );
					pthread_mutex_lock( &qARCAMessages_mutex );
					fnQput( &qARCAMessages, msg );	// Push this message onto the queue for processing
					iARCAQueueSize++;
					pthread_mutex_unlock( &qARCAMessages_mutex );
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
				if ( msg[1] == 0x50 )
				{
					msg[0] = (char)c;
					msg[1] = GIDS_MESSAGE_TYPE;
					//fnDebug( "Pushed Message onto Queue" );
					pthread_mutex_lock( &qARCAMessages_mutex );
					fnQput( &qARCAMessages, msg );	// Push this message onto the queue for processing
					iARCAQueueSize++;
					pthread_mutex_unlock( &qARCAMessages_mutex );
				}
				//else
				//	fnDebug( "Message Skipped" );
				break;
			}	
		}

		memset( byBuffer, 0, recvStringLen+1);
		
	}

	fnDebug( "GIDS Reader Thread closed" );
	
    close(sock);
	pthread_exit(NULL);		
}

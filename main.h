#ifndef __main_h
#define __main_h

#include <stdio.h>      
#include <sys/socket.h> // for socket(), connect(), sendto(), and recvfrom()
#include <sys/poll.h>   // for poll() to get socket status
#include <arpa/inet.h>  // for sockaddr_in and inet_addr()
#include <stdlib.h>     
#include <string.h> 
#include <ctype.h>    
#include <unistd.h>     // for close() 
#include <pthread.h>    // POSIX multithreading libary
#include <sched.h>		// Needed to change scheduling priority on threads
#include <stdarg.h>
#include <sys/time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>        /*  socket types              */

#include "queue.h"

// Compile with DEBUG options on or off
#define DEBUG
//#define NDEBUG
#include <assert.h>

// Can be used for processor specific compilation
//#define PROCESSOR_X86
#define PROCESSOR_POWER6

// Global Defines
#define TRUE                    1
#define FALSE                   0

// Sizes
#define MULTICAST_BUFFER_SIZE   8128
#define LINE_BUFFER_SIZE        4096
#define BUFFER_SIZE             4096
#define SHORT_BUFFER_SIZE		1024
#define CONFIG_BUFFER_SIZE      4096
#define MAX_RECIEVE_ERRORS      100

// Default Multicast settings
#define MULTICAST_DEFAULT_GROUP		0xe0027fff
#define MULTICAST_DEFAULT_PORT		9876
#define MULTICAST_MAXPDU 				4096
#define MULTICAST_WIDTH 				16

// Stock info
#define TRUE                1
#define FALSE               0

// Configuration settings
#define CONFIG_COMMENT_CHAR                     '#'
#define CONFIG_TOKEN                            '='
#define DEFAULT_CONFIG_FILE                     "tmmdsreader.cfg"
int fnGetConfigSetting( char * szValue, char *szSetting, char *szDefault );


// Configuration settings that will control the start, end, and idle time of da
#define STATUS_TRADING		0
#define STATUS_IDLE			1
#define READER_START_HOUR	6
#define READER_END_HOUR		20


// Multicast Engine Defines - to run each in its own thread
// Filtering of messages has been moved into this thread

union binary_long
{
        unsigned long value;
        char string[8];
};

union binary_int
{
        unsigned int value;
        char string[4];
};


// Utility Functions
int fnFileExists(const char *);


// Socket Engine - to be run in its own thread
#define SOCKET_BUFFER_SIZE		265
#define LISTENQ        			(256)   /*  Backlog for listen()   */
void *fnSocketEngine( void );
inline int fnWriteSocket(int, char *, size_t);
inline int fnReadSocket(int, char *, size_t);

// Custom Includes
#include "swapendian.h"
#include "errormanager.h"
#include "calcengine.h"
#include "global.h"
#include "arcareader.h"
#include "batsreader.h"
#include "itchreader.h"
#include "volume.h"
#include "readquotes.h"
#include "calcengine.h"
#include "gidsreader.h"
#include "queue.h"


#endif

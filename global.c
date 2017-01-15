#include "main.h"

// Global Variables

// ********************
// 		Global Data
// ********************
// Queues for each thread:
// struct q_head qMulticastMessages;
struct q_head qITCHMessages;
struct q_head qDeleteMessages;
struct q_head qOutMessages;
struct q_head qMDSHandler;
struct q_head qErrorMessages;
struct q_head qBinaryData;
struct q_head qARCAMessages;

int iClientConnected;
int iMessagesQueued;

// Tracking Variables
int iITCHQueueSize=0;
int iARCAQueueSize=0;
int iDeleteQueueSize=0;
int iErrorQueueSize=0;
int iOutboundQueueSize=0;
int iOrderBufferSize=0;

int iSocket;

int iOrderBufferSort;

int iSendVU;
int iSendIU; 
int iSendTU;

// Various Mutexes for locking data for concurrancy
pthread_mutex_t qITCHMessages_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t qDeleteMessages_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t qErrorMessages_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t qOutMessages_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t qBufferPrint_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t qBufferSystem_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t qARCAMessages_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t config_mutex = PTHREAD_MUTEX_INITIALIZER; 

char gszConfigFile[CONFIG_BUFFER_SIZE];

// Array of stocks for the index
// store the ETF at +1, store the printed price at +2
char gszStocks[MAXIMUM_STOCKS][STOCK_SYMBOL_SIZE_PLUS_2];
int iStocksLoaded;
double gfLastSale[MAXIMUM_STOCKS];
double gfAsk[MAXIMUM_STOCKS];
double gfBid[MAXIMUM_STOCKS];
double gfWeights[MAXIMUM_STOCKS];
double gfQLastSale=0.0;
double gfNDXLast=0.0;
double gfMilliseconds=0.0;
double gfNDX=0.0;
double gfNDXDivisor=0.0;
double gfCalcNDX=0.0;
double gfAskNDX=0.0;
double gfBidNDX=0.0;

int	QQQQ=0;

double gfFV[FV_COUNT];
double gfQLast;
double gfNDXLast;
double gfQBid;
double gfQAsk; 
double gfCurrentTimeMillis;
double gfLastNDXUpdateMillis;  
int iTradingFlag;

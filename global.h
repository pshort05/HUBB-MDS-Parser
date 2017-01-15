#ifndef __global_h
#define __global_h

#include "main.h"

extern struct q_head qITCHMessages;
extern struct q_head qDeleteMessages;
extern struct q_head qOutMessages;
extern struct q_head qMDSHandler;
extern struct q_head qErrorMessages;
extern struct q_head qBinaryData;
extern struct q_head qARCAMessages;

extern int iReadBinary;
extern int iClientConnected;
extern int iMessagesQueued;

extern int iITCHQueueSize;
extern int iARCAQueueSize;
extern int iDeleteQueueSize;
extern int iErrorQueueSize;
extern int iOutboundQueueSize;
extern int iOrderBufferSize;
extern int iSocket;

extern int iOrderBufferSort;
extern int iSendVU;
extern int iSendIU; 
extern int iSendTU;

// Various Mutexes for locking data for concurrancy
extern pthread_mutex_t qITCHMessages_mutex;
extern pthread_mutex_t qDeleteMessages_mutex;
extern pthread_mutex_t qErrorMessages_mutex;
extern pthread_mutex_t qOutMessages_mutex;
extern pthread_mutex_t qBufferPrint_mutex;
extern pthread_mutex_t qBufferSystem_mutex;
extern pthread_mutex_t qARCAMessages_mutex;
extern pthread_mutex_t config_mutex;

extern char gszConfigFile[BUFFER_SIZE];

// Array of stocks for the index
// store the ETF at +1, store the printed price at +2
extern char gszStocks[MAXIMUM_STOCKS][STOCK_SYMBOL_SIZE_PLUS_2];
extern int iStocksLoaded;
extern double gfLastSale[MAXIMUM_STOCKS];
extern double gfAsk[MAXIMUM_STOCKS];
extern double gfBid[MAXIMUM_STOCKS];
extern double gfWeights[MAXIMUM_STOCKS];
extern double gfQLastSale;
extern double gfNDXLast;
extern double gfMilliseconds;
extern double gfNDX;
extern double gfNDXDivisor;
extern double gfCalcNDX;
extern double gfAskNDX;
extern double gfBidNDX;

extern int	QQQQ;

extern double gfFV[FV_COUNT];
extern double gfQLast;
extern double gfNDXLast;
extern double gfQBid;
extern double gfQAsk; 
extern double gfCurrentTimeMillis;
extern double gfLastNDXUpdateMillis;  
extern int iTradingFlag;

#endif

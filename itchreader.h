#ifndef __itchreader_h
#define __itchreader_h

// ITCH Message Types
#define ITCH_TIME_MESSAGE                       'T'
#define ITCH_SYSTEM_MESSAGE                     'S'
#define ITCH_DIRECTORY_MESSAGE                  'R'
#define ITCH_STOCK_TRADING_MESSAGE              'H'
#define ITCH_STOCK_PARTICIPANT_MESSAGE          'L'
#define ITCH_ADD_ORDER_MESSAGE                  'A'
#define ITCH_ADD_ORDER_MPID_MESSAGE             'F'
#define ITCH_EXECUTED_ORDER_MESSAGE             'E'
#define ITCH_EXECUTED_ORDER_PRICE_MESSAGE       'C'
#define ITCH_ORDER_CANCEL_MESSAGE               'X'
#define ITCH_ORDER_DELETE_MESSAGE               'D'
#define ITCH_ORDER_REPLACE_MESSAGE              'U'
#define ITCH_TRADE_MESSAGE                      'P'
#define ITCH_CROSS_TRADE_MESSAGE                'Q'
#define ITCH_BROKEN_TRADE_MESSAGE               'B'
#define ITCH_NET_ORDER_INBALANCE_MESSAGE        'I'

#define ITCH_MSG_TYPE_INDEX                     22
#define MULTICAST_MSG_HEADER_SIZE               21
#define ITCH_STOCK_SYMBOL_SIZE                  8

// Maximum number of messages that can queue before sending out queuing messages
#define ITCH_MESSAGE_BUFFERING_COUNT			100

// Maximum price jump (in %) that is allowed before being filtered out
#define MAXIMUM_STOCK_PRICE_MOVE				.02

// Minimum size the delete buffer must become before the delete thread works it
#define MINIMUM_DELETE_BUFFER_SIZE_ACTION		1000	
// Maximum size the ITCH buffer can grow before the delete thread idles
#define MAXIMUM_ITCH_BUFFER_SIZE_ACTION			100

// Order Buffer System
#define ORDER_BUFFER_SIZE               20000000
#define ORDER_BUFFER_SIZE_MINUS_1       19999999

// Commands
#define ORDER_BUFFER_INITIALIZE               0
#define ORDER_BUFFER_DESTROY                  0
#define ORDER_BUFFER_ADD                      1
#define ORDER_BUFFER_SEARCH                   2
#define ORDER_BUFFER_REMOVE_BY_INDEX          3
#define ORDER_BUFFER_REMOVE_BY_ORDER_NUM      4
#define ORDER_BUFFER_MODIFY_BY_INDEX          5
#define ORDER_BUFFER_MODIFY_BY_ORDER_NUM      6
#define ORDER_BUFFER_GET_BY_INDEX             7
#define ORDER_BUFFER_GET_BY_ORDER_NUM         8
#define ORDER_BUFFER_SORT					  10
#define ORDER_BUFFER_SAVE_TO_DISK			  11
#define ORDER_BUFFER_LOAD_FROM_DISK			  12

#define ORDER_BUFFER_SUCCESS				0
#define ORDER_BUFFER_NO_DUMP_FILE			0

// Error messages
#define ORDER_NOT_FOUND                 	-1
#define ORDER_BUFFER_SEQUENCE_ERROR			-2
#define ORDER_BUFFER_MEMORY_ERROR			-3
#define ORDER_BUFFER_FOUND_STALE_MATCH		-4
#define ORDER_BUFFER_OUT_OF_BOUNDS			-5
#define ORDER_INDEX_OUT_OF_BOUNDS			-5
#define ORDER_BUFFER_NO_DATA_FOUND			-6
#define ORDER_BUFFER_FILE_SAVE_ERROR		-7
#define ORDER_BUFFER_READ_ERROR				-8
#define ORDER_BUFFER_SEQUENCE_FIX_ERROR		-9
#define ORDER_NULL_POINTER_FOUND			-10
#define ORDER_BUFFER_INVALID_OPERATION		-20
#define ORDER_BUFFER_ERROR_FULL         	-99
#define ORDER_BUFFER_FULL					-99
#define ORDER_BUFFER_CRITICAL_ERROR     	-999


#define ORDER_BUFFER_DUMP_FILE			"orderbuffer.dump"

// Diagnostic Calls
#define ORDER_BUFFER_DEBUG_COUNT        999
#define ORDER_BUFFER_DEBUG_MAX          998
#define ORDER_BUFFER_DEBUG_DUMP			997

// ITCH Data Structure
struct ITCHData
{
        unsigned long OrderNumber;        		// Unique Order ID
        char BuySell;           				// Buy or Sell indicator
        int Shares;             				// Number of Shares
        int Price;              				// Price in Integer Format (divide by 10000 for float)
        char Stock[ITCH_STOCK_SYMBOL_SIZE+1];   // Stock Symbol
		int StockIndex;							// Stock index used for lookup
};


extern void fnStartITCH( void );
extern void fnStopITCH( void );
extern void *fnMessageParser( void );
extern void *fnDeleteMessageEngineParser( void );
extern void *fnMulticastEngineReader( void );

extern int fnOrderBuffer( int , struct ITCHData *, unsigned long );



#endif

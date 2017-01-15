#ifndef __batsreader_h
#define __batsreader_h

#define BATS_UNITS		12

#define IP_STRING_LEN							32
#define NAME_STRING_SIZE						128
#define BATS_MSG_HEADER_SIZE					8
#define BATS_MULTICAST_ERROR_COUNT				20

#define BATS_TIME_MESSAGE						0x20
#define BATS_ADD_ORDER_MESSAGE_LONG				0x21
#define BATS_ADD_ORDER_MESSAGE_SHORT			0x22
#define BATS_ORDER_EXECUTED_MESSAGE				0x23
#define BATS_ORDER_EXECUTED_AT_PRICE_MESSAGE	0x24
#define BATS_REDUCE_SIZE_MESSAGE_LONG			0x25
#define	BATS_REDUCE_SIZE_MESSAGE_SHORT			0x26
#define BATS_MODIFY_ORDER_MESSAGE_LONG			0x27
#define	BATS_MODIFY_ORDER_MESSAGE_SHORT			0x28
#define BATS_DELETE_ORDER_MESSAGE				0x29
#define	BATS_TRADE_MESSAGE_LONG					0x2A
#define BATS_TRADE_MESSAGE_SHORT				0x2B
#define BATS_TRADE_BREAK_MESSAGE				0x2C
#define BATS_SYMBOL_MAP_MESSAGE					0x2E

#define BATS_ORDER_BUFFER_SIZE					2000000
#define BATS_ORDER_BUFFER_SIZE_MINUS_1			1999999
#define BUFFER_UNITS							13
#define BATS_STOCK_SYMBOL_SIZE					8
#define BATS_ORDER_BUFFER_DUMP_FILE				"BATS_ORDER_BUFFER"

extern void fnShutdownBATSReaderThreads( void );
extern void fnStartBATSReaderThreads( void );
extern void fnCheckBATSOrderBuffer( void );
extern void fnDisplayBATSStats( void );

#endif

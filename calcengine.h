#ifndef __calcengine_h
#define __calcengine_h

// Standard defines for the calcengine

#define STOCK_SYMBOL_SIZE   			8
#define STOCK_SYMBOL_SIZE_PLUS_2		10
#define INDEX_FACTOR        			1685056377
#define INDEX_SYMBOL        			"QQQQ"
#define NDX_SYMBOL          			"NDX"
#define NDX_SYMBOL_PREMARKET			"QMI"
#define NDX_SYMBOL_AFTERMARKET			"QIV"
#define INDEX_COUNT         			100
#define INDEX_COUNT_NDX					101
#define INDEX_COUNT_PLUS_2				102
#define CALC_BA_NDX         			1
#define CALC_NDX            			2
#define CALC_BOTH           			3
//#define QQQQ                			INDEX_COUNT
#define PRINTED_NDX         			INDEX_COUNT+1
#define DEFAULT_WEIGHT_FILE				"T_INDEX_WEIGHT.txt"
#define DEFAULT_WEIGHT_DATE				"2009-12-23"
#define DEFAULT_DIVISOR_FILE 			"T_TIMEMACHINE_CONFIG.txt"
#define DIVISOR_STRING					"indexFactor"
#define DEFAULT_STOCK_LIST				"stocks.list"

#define MAXIMUM_STOCKS					1000

// Fair Value Defines
#define FV_COUNT				13
#define NUM_FV_CALCS        	14
#define CALC_FV					0
#define ASK_FV					1
#define BID_FV					2
#define CALC_RATIO_FV			3
#define	ASK_RATIO_FV			4
#define BID_RATIO_FV			5
#define MODIFIED_ASK_RATIO_FV	6
#define MODIFIED_BID_RATIO_FV	7
#define ASK_SPREAD_FV			8
#define BID_SPREAD_FV			9
#define BIDASK_SPREAD_FV		10
#define NEW_CALC_FV				11
#define NEW_CALC_FV2			12


#define	STOCK_NOT_FOUND		-1

// Calc Engine - to be run in its own thread
extern int fnCalculateIndex( void );
extern int fnCalculateBidAskIndex( void );
extern int fnCalcFVs( void );
extern int fnLoadWeights( void );
extern double fnLoadDivisor( void );
extern int fnLoadStocks( void );
int fnStockFilter( char * );


// Helpful Macros
#define fnSaveStockPrice(a,b) 	if(a>=0 && b>0) gfLastSale[a]=b	
#define fnSaveBidPrice(a,b)		if(a>=0 && b>0) gfBid[a]=b
#define fnSaveAskPrice(a,b)		if(a>=0 && b>0) gfAsk[a]=b
#define fnGetStockPrice(a) 		gfLastSale[a]
#define	fnGetBidPrice(a)		gfBid[a]
#define fnGetAskPrice(a)		gfAsk[a]
#define fnGetStockSymbol(a)		gszStocks[a]
#define fnSaveETFPrice(a)	    gfLastSale[QQQQ]=a
#define fnGetETFPrice()			gfLastSale[QQQQ]



#endif

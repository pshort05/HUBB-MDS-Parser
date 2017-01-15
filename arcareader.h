#ifndef __arcareader__h
#define __arcareader__h

#define ARCA_MULTICAST_HEADER_SIZE	16
#define ARCA_TRADE_MESSAGE_SIZE		56

extern void fnStartARCA( void );
extern void fnStopARCA( void );
extern void *fnArcaMulticastEngineReader( void );
extern void *fnArcaETFMulticastEngineReader( void );
extern void *fnArcaOTCMulticastEngineReader( void );
extern void *fnARCAMessageParser( void );

#endif

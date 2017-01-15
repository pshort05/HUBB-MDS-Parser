#ifndef __gidsreader_h
#define __gidsreader_h

#define GIDS_MESSAGE_TYPE	0x19

extern void fnStartGIDS( void );
extern void fnShutdownGIDS( void );
extern void *fnGIDSMulticastReader( void );

#endif

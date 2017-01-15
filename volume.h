#ifndef __volume_h
#define __volume_h

// Function Prototypes
void fnStartVolumeThreads( void );
void fnStopVolumeThreads( void );
void fnSaveQVolume( long iQShares );
long fnGetQVolume( void );
void *fnQVol1( void );
void *fnQVol5( void );

#endif

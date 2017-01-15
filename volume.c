/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Paul Short 2009 <pshort@timemachinetrading.com>
 * 
 */

#include "volume.h"
#include "main.h"

// Local Defines
#define  SLEEP_ONE_MINUTE       5
#define  SLEEP_FIVE_MINUTE      30
#define  VOLUME_INDEX           120
#define  CYCLE_INDEX			60
#define  CYCLE_START            0
#define  CYCLE_END              4

// Local variables
long iQVol=0;
pthread_mutex_t iQVol_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t pQVol1;
pthread_t pQVol5;


void fnStartVolumeThreads( void )
{
	pthread_attr_t paQVol1;
	pthread_attr_t paQVol5;
	
	pthread_attr_init( &paQVol1 );
	pthread_attr_setdetachstate( &paQVol1, PTHREAD_CREATE_DETACHED );
	pthread_create( &pQVol1, &paQVol1, (void *)fnQVol1, NULL );
	pthread_attr_init( &paQVol5 );
	pthread_attr_setdetachstate( &paQVol5, PTHREAD_CREATE_DETACHED );
	pthread_create( &pQVol5, &paQVol5, (void *)fnQVol5, NULL );
	fnDebug( "Started volume tracking threads" );
}	

void fnStopVolumeThreads( void )
{
	iSendVU = FALSE;
	pthread_cancel( pQVol1 );
	pthread_cancel( pQVol5 );
	fnDebug ( "Volume Threads shutdown" );
}


// Function to save the NASDAQ volume for the QQQQ Index
void fnSaveQVolume( long iQShares )
{	
	//fnDebug( "fnSaveQVolume" );
	
	if ( iSendVU == FALSE )
		return;
	// Lock the running totals and update with new share count
	pthread_mutex_lock( &iQVol_mutex );
	iQVol = iQVol + iQShares;               
	pthread_mutex_unlock( &iQVol_mutex );
}

void fnPushVolumeUpdate( char* szStock, long iVolume, long iInterval )
{
	char szBuffer[BUFFER_SIZE];

	//fnDebug( "fnPushVolumeUpdate" );

	snprintf( szBuffer, sizeof(szBuffer), "VU %s %li %li \n", szStock, iVolume, iInterval );		

	if (iClientConnected == TRUE && iSendVU == TRUE )
	{
		fnSrvPush( &qOutMessages, szBuffer );
	}

	fnVolumeLog( szBuffer );

}

long fnGetQVolume( void )
{
	long iTemp;

	pthread_mutex_lock( &iQVol_mutex );
	iTemp = iQVol;               
	pthread_mutex_unlock( &iQVol_mutex );
		
	return iTemp;
}


// *************************************************************************
// Function to calculate and push the 1 minute Q volume out to the TM Client
//    This function will run in it's own thread
//    The function will calculate approximately 1 minute index volumes every
//    second.  There will be some jitter from the sleep and calculations. 
//    This will not impact individual calculations but over the day the number
//    of times this is generated will not be equal to the number of seconds
// *************************************************************************
void *fnQVol1( void )
{
     long iStartVol[CYCLE_INDEX];
     long iCurrentVol;
     long iCycle;
     
	 fnDebug( "fnQVol1" );

		// Initialize the start of cycles
     for( iCycle=CYCLE_START; iCycle<CYCLE_INDEX; iCycle++ )
          iStartVol[iCycle] = 0;
     iCycle=CYCLE_START;

	//fnDebug( "fnQVol1 initialized" );
     
          // Start and endless loop to calculate the volume
     while ( TRUE )
     { 

			//fnDebug( "fnQVol1 going to sleep" );
		 
			// Put this thread to sleep for 1 second
		   sched_yield();
		   sleep( 1 );      

		   //fnDebug( "Getting volume in fnQVol1" );
		 
				  // grab the accumulated volume 
		   pthread_mutex_lock( &iQVol_mutex );
		   iCurrentVol = iQVol;
		   pthread_mutex_unlock( &iQVol_mutex );
		   
				  // push the volume to the server if clients are connected 
		   fnPushVolumeUpdate( "QQQQ", iCurrentVol-iStartVol[iCycle], (long)1 );

			  // Save the volume from this last calculation - reset every 60 seconds
		   iStartVol[iCycle] = iCurrentVol;
		   if( iCycle == SLEEP_ONE_MINUTE-1 )
			   iCycle = CYCLE_START;
		   else
			   iCycle++;
     }
}

            
// *************************************************************************
// Function to calculate and push the 5 minute Q volume out to the TM Client
//    This function will run in it's own thread
// *************************************************************************
void *fnQVol5( void )
{
     long iStartVol[CYCLE_INDEX];
     long iCurrentVol;
     long iCycle;

	 fnDebug( "fnQVol5" );
     
          // Initialize the start of cycles
     for( iCycle=CYCLE_START; iCycle<CYCLE_INDEX; iCycle++ )
          iStartVol[iCycle] = 0;
     iCycle=CYCLE_START;
     
          // Start and endless loop to calculate the volume
     while ( TRUE )
     { 
           // Put this thread to sleep for 1 second
		   sched_yield();
	       sleep( 1 );      

		   //fnDebug( "Getting volume in fnQVol5" );
			 
		          // grab the accumulated volume 
		   pthread_mutex_lock( &iQVol_mutex );
		   iCurrentVol = iQVol;
		   pthread_mutex_unlock( &iQVol_mutex );
		   
		          // push the volume to the server if clients are connected 
		   fnPushVolumeUpdate( "QQQQ", iCurrentVol-iStartVol[iCycle], (long)5 );
		   
		      // Save the volume from this last calculation - reset every 5 minutes
		   iStartVol[iCycle] = iCurrentVol;
		   if( iCycle == SLEEP_FIVE_MINUTE-1 )
		       iCycle = CYCLE_START;
		   else
		       iCycle++;
     }
}     

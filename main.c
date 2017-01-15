//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Paul Short 2009 <pshort@timemachinetrading.com>
 * 
 */
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#include "main.h"


//-----------------------------------------------------------------------------
//	the main program spawns the threads and looks out for the exit signal
//  none of the processing takes place here
//-----------------------------------------------------------------------------
int main( int argc, char *argv[] )
{
	char szTempString[LINE_BUFFER_SIZE];
	
	pthread_t pErrorEngine;
	pthread_attr_t paErrorEngine;

	pthread_t pSocketEngine;
	pthread_attr_t paSocketEngine;

	pthread_t pAgentControl;
	//pthread_attr_t paAgentControl;

	int iReadITCH = FALSE;
	int iReadBATS = FALSE;
	int iReadARCA = FALSE;
	int iReadQuotes = FALSE;
	int iReadGIDS = FALSE;
	
	int iSleepCycle = 3;
	int	iDisplayQueueSize = TRUE;
	char szBuffer[BUFFER_SIZE];
	    	

	iITCHQueueSize = 0;
	iDeleteQueueSize =0;

	// Start the error reporting thread
	pthread_attr_init( &paErrorEngine);
	pthread_attr_setdetachstate( &paErrorEngine, PTHREAD_CREATE_DETACHED );
	pthread_create( &pErrorEngine, &paErrorEngine, (void *)fnErrorManager, NULL );
	fnDebug("Error Engine Started");
	
	// Initialize the configuration
	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting ( szTempString, DEFAULT_CONFIG_FILE, DEFAULT_CONFIG_FILE );
	pthread_mutex_unlock( &config_mutex );

	// Load the weights and divisor
	//fnLoadWeights();
	fnLoadStocks();
	//fnLoadDivisor();

	// Setup the outbound socket server
	iClientConnected = FALSE;
	fnQinit( &qOutMessages );
	pthread_attr_init( &paSocketEngine);
	pthread_attr_setdetachstate( &paSocketEngine, PTHREAD_CREATE_DETACHED );
	pthread_create( &pSocketEngine, &paSocketEngine, (void *)fnSocketEngine, NULL );
	fnDebug( "Socket Engine Thread Initialized" );

	// Setup the Volume tracking functionality
	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting( szBuffer, "SENDVOLUMEUPDATES", "1" );
	pthread_mutex_unlock( &config_mutex );
	iSendVU = atoi( szBuffer );
	if ( iSendVU == TRUE )
		fnStartVolumeThreads();
		
	// Determine what type of updates will be sent to the outbound server
	// See if we are to stream Inside Updates
	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting( szBuffer, "SENDINSIDEUPDATES", "0" );
	iSendIU = atoi( szBuffer );
	// See if we are to stream Trade Updates
	fnGetConfigSetting( szBuffer, "SENDTRADEUPDATES", "0" );
	iSendTU = atoi( szBuffer );
	pthread_mutex_unlock( &config_mutex );
	

	// Determine which feeds will be Read
	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting ( szTempString, "READITCH", "1" );
	iReadITCH = atoi( szTempString );
	fnGetConfigSetting ( szTempString, "READBATS", "1" );
	iReadBATS = atoi( szTempString );
	fnGetConfigSetting ( szTempString, "READARCA", "1" );
	iReadARCA = atoi( szTempString );
	fnGetConfigSetting ( szTempString, "READQUOTES", "1" );
	iReadQuotes = atoi( szTempString );
	fnGetConfigSetting ( szTempString, "READGIDS", "1" );
	iReadGIDS = atoi( szTempString );
	//fnGetConfigSetting ( szTempString, "RUNAGENTS", "1" );
	//iRunAgents = atoi( szTempString );
	pthread_mutex_unlock( &config_mutex );

	// Start the Quotes Functionality
	if( iReadQuotes == TRUE )
		fnStartQuotes();

	// Setup the BATS functionality first since it has the longest startup time
	if ( iReadBATS == TRUE )
		fnStartBATSReaderThreads();
	
	// Setup the ITCH functionality
	if ( iReadITCH == TRUE )
		fnStartITCH();

	// Setup the ARCA functionality
	if ( iReadARCA == TRUE )
		fnStartARCA();

	// Setup the GIDS functionality
	if ( iReadGIDS == TRUE )
	{
		if( iReadARCA == FALSE )
			fnHandleError ( "Main", "The GIDS feed requires the ARCA feed to be started.  Please start the ARCA feed" );
		else
			fnStartGIDS();
	}
		
	// Setup the main program checking cycle
	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting( szTempString, "SLEEPCYCLE", "2" );
	iSleepCycle = atoi( szTempString );
	fnGetConfigSetting( szTempString, "DISPLAYQUEUESIZE", "1" );
	iDisplayQueueSize = atoi( szTempString );
	pthread_mutex_unlock( &config_mutex );

	// This is the main processing loop that wakes up every few seconds to check
	// for a shutdown or other state
	while ( TRUE )
	{
		sleep(iSleepCycle);

		// Check if the order buffer needs to be sorted
		if ( iReadITCH == TRUE && iOrderBufferSort > 0 )
		{
			pthread_mutex_lock( &qBufferSystem_mutex );
			fnOrderBuffer ( ORDER_BUFFER_SORT, NULL, 0 );
			pthread_mutex_unlock( &qBufferSystem_mutex );
			sched_yield();
			iOrderBufferSort = 0;
			usleep(10);
		}

		// Display the Queue Sizes
		if ( iDisplayQueueSize == TRUE )
		{
			snprintf( szBuffer, sizeof(szBuffer), "Queue Sizes: ITCH = %i Delete = %i ARCA = %i Error = %i Outbound = %i ITCH BufferSize %i QVolume %li", 
				 iITCHQueueSize, iDeleteQueueSize, iARCAQueueSize, iErrorQueueSize, iOutboundQueueSize, iOrderBufferSize, fnGetQVolume() );
			fnDebug(szBuffer);
			if( iReadBATS )
				fnDisplayBATSStats();
			sched_yield();
			usleep(10);
		}

		// Check for a system shutdown request
		if( fnFileExists( "shutdown_reader" ) == TRUE )
		{
			fnDebug( "Shutdown request found" );		
			break;
		}
	}	

	fnDebug( "Shutting Down" );

	// Stop GIDS
	if( iReadGIDS == TRUE )
		fnShutdownGIDS();
	
	// Stop the Quotes Functionality
	if( iReadQuotes == TRUE )
		fnShutdownQuotes();

	// Shutdown ITCH
	if( iReadITCH == TRUE )
		fnStopITCH();
		
	// Shutdown ARCA
	if ( iReadARCA == TRUE )
		fnStopARCA();
	
	// Shutdown BATS
	if ( iReadBATS == TRUE )
		fnShutdownBATSReaderThreads();

	// Shutdown the volume threads if necessary
	if ( iSendVU == TRUE )
		fnStopVolumeThreads();

	// Shutdown the Socket Engine
	iSendIU = FALSE;
	iSendTU = FALSE;
	iSendVU = FALSE;
	pthread_cancel( pSocketEngine );
	fnQinit( &qOutMessages );
	fnDebug( "Socket engine shutdown" );
	
	// Wait for the Error Queue to fully flush
	usleep(100);
	while ( iErrorQueueSize > 0 )
		sched_yield();

	pthread_cancel( pErrorEngine );
	fnErrorInit ( &qErrorMessages );
	
	// Close the error log
	fnCloseErrorLog();
	
	// Exit and destroy all the threads
	exit(TRUE);
}

//-----------------------------------------------------------------------------
// 	Get a configuration setting for the program
//	The first time this function is called the CFG file must be supplied
//
//	entires in the config file are as follows:
//		entry=setting
//		#entry=setting	(use # to comment out lines)
//
//-----------------------------------------------------------------------------
int fnGetConfigSetting( char * szValue, char *szSetting, char *szDefault )
{
	//char szConfigValue[CONFIG_BUFFER_SIZE];
    char szLineBuffer[CONFIG_BUFFER_SIZE];
    static int bInitialized=FALSE;

	char *szEqual;
	FILE *fpConfigFile;
	int bFound = FALSE;

	//pthread_mutex_lock( &config_mutex );

	// Initialize the first time through
	if ( bInitialized == FALSE )
	{
			fnDebug( "ConfigSettings Initialized" );
			strcpy( gszConfigFile, szSetting );
			bInitialized = TRUE;
			return TRUE;
	}

	//strcpy(szValue, szDefault);
	//return TRUE;

	fpConfigFile = fopen( gszConfigFile, "r" );
	if ( fpConfigFile == NULL )
	{
			fnHandleError ( "fnGetConfigSetting", "Cannot open config file, trying default" );
			fpConfigFile = fopen( szDefault, "r" );
			if( fpConfigFile == NULL )
			{
				fnHandleError ( "fnGetConfigSetting", "Cannot open config file, found default" );
				strcpy( gszConfigFile, szDefault );
			}
			else
				return FALSE;
	}	
	// Start reading through the config file looking for the setting
	while ( fgets( szLineBuffer, sizeof(szLineBuffer)-1, fpConfigFile ) != NULL )
	{
			if ( feof(fpConfigFile) )
				break;
			//if ( szLineBuffer[0] == EOF )
			//	break;

			// Skip the line if its a comment
			if ( szLineBuffer[0] == CONFIG_COMMENT_CHAR )
					continue;
			szEqual = strchr( szLineBuffer, CONFIG_TOKEN  );
			if ( szEqual != NULL )
			{
					// Found an equal sign, look for the required setting
					if( strncmp( szLineBuffer, szSetting, (size_t)(szEqual-szLineBuffer) ) == 0 )
					{
							// Found the setting copy the value into the buffer
							szEqual++;
							strncpy( szValue, szEqual, strlen(szEqual)-1 );
							bFound = TRUE;
							break;
					}
			}
	}
	
	fclose(fpConfigFile);
	if ( bFound == TRUE )
	{	
		snprintf( szLineBuffer, sizeof(szLineBuffer)-1, "Found %s for config setting %s in %s", 
				 szValue, szSetting, gszConfigFile );
		fnDebug( szLineBuffer );
		//strcpy( szValue, szConfigValue );
		return TRUE;
	}
	else
	{	
		snprintf( szLineBuffer, sizeof(szLineBuffer)-1, "Did not find setting %s in %s returning default %s", 
				 szSetting, gszConfigFile, szDefault );
		fnDebug( szLineBuffer );
		strcpy( szValue, szDefault );
		return FALSE;
	}
	//pthread_mutex_unlock( &config_mutex );

}

//-----------------------------------------------------------------------------
// Check to see if a file exists by opening it for reading
//-----------------------------------------------------------------------------
int fnFileExists(const char * szFilename)
{
	FILE *fp;

	fp = fopen(szFilename, "r");
    if ( fp != NULL ) 
    {
        fclose(fp);
        return TRUE;
    }
    return FALSE;
}


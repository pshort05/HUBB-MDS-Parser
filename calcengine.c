#include "main.h"
#include "calcengine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// ****************************************************************************
// Function that will load the weights from the data file for any specific day
// ****************************************************************************
int fnLoadWeights( void )
{
    // Function Removed and no longer in use - left for backwards compatability
    return TRUE;
}

int fnStockCompare( const void* p1, const void* p2 )
{
	return strcmp( (char *)p1, (char *)p2 );
}
	
// *****************************************************************************
// 		Function to load the stock watch list into memory
// *****************************************************************************
int fnLoadStocks( void )
{
    char szBuffer[LINE_BUFFER_SIZE];
	char szListFile[BUFFER_SIZE];
    char szStock[BUFFER_SIZE];
    int iStockCount=0;
	int iSkip=FALSE;
    FILE *fpListFile;
	int i;
	//char szDate[BUFFER_SIZE];
    //int iBufferSize=0;  
	//struct tm *tLocalTime;
	//time_t tTime;

	fnDebug( "Loading Stock List" );

	// Get the list file from the config
	pthread_mutex_lock( &config_mutex );
	fnGetConfigSetting( szListFile, "STOCKLISTFILE", DEFAULT_STOCK_LIST );
	pthread_mutex_unlock( &config_mutex );
    
    fpListFile = fopen( szListFile, "r" );
	if( fpListFile == NULL )
		return 0;
        
    while ( fgets(szBuffer, LINE_BUFFER_SIZE-2, fpListFile) != NULL)
    {
		bzero( szStock, sizeof(szStock) );
		strcpy( szStock, szBuffer );
		// Remove any trailing CR/LF and replace any . with a 0x00
		if( szStock[strlen(szStock)-1] == 0x0A || szStock[strlen(szStock)-1] == 0x0D )
			szStock[strlen(szStock)-1] = 0x00;

		// Check if this stock has already been loaded
		iSkip=FALSE;
		for( i=0; i<iStockCount; i++ )
		{
			if( strcmp( szStock, gszStocks[i] ) == 0 )
			{
				iSkip=TRUE;
				break;
			}
		}
		if( iSkip )
		{
			// Duplicate stock found - don't add this
			snprintf( szBuffer, sizeof(szBuffer), "Skipping duplicate %s", szStock );
			fnDebug( szBuffer );
		}
		else
		{
			// New stock found - add to list
			strcpy( gszStocks[iStockCount], szStock );
			snprintf( szBuffer, sizeof(szBuffer), "%d Loaded stock %s", iStockCount, gszStocks[iStockCount] );
			fnDebug( szBuffer );				
		    iStockCount++;  		
		    if (iStockCount == MAXIMUM_STOCKS )
			{
				fnHandleError( "fnLoadStocks", "Maximum stocks reached" );
		        break;               
			}
		}
    }
	fclose(fpListFile);

	iStocksLoaded = iStockCount;

	QQQQ = fnStockFilter( "QQQQ" );
    if( QQQQ == STOCK_NOT_FOUND )
        fnHandleError( "fnLoadStocks", "QQQQ Index tracking stock not found" );
    else
	{
		snprintf( szBuffer, sizeof(szBuffer), "Found QQQQ Index tracking Stock %d", QQQQ );
		fnDebug( szBuffer );
	}
    

	//qsort ( gszStocks, iStockCount, sizeof(gszStocks[0]), fnStockCompare );
	
    snprintf( szBuffer, sizeof(szBuffer), "%i stocks loaded", iStockCount );
	fnDebug( szBuffer );
	
    return iStockCount;
}	

// ****************************************************************************
//		Function to load the divisor for the index calculation
// ****************************************************************************
double fnLoadDivisor( void )
{
    // Function Removed and no longer in use - left for backwards compatability
    return 0.00;
}

// ****************************************************************************
//		Stock Filter:  fnStockFilter( Stock Name )
//			Return the index # of the located stock
//			Return STOCK_NOT_FOUND ( -1 ) if no match is found
// ****************************************************************************
int fnStockFilter( char * szStock )
{
	int i;

	for( i=0; i<iStocksLoaded; i++ )
		if( strcmp( szStock, gszStocks[i] ) == 0 )
			return i;
	
	return STOCK_NOT_FOUND;
}  

// ****************************************************************************
// Function to recalcuation the NASDAQ 100 indexes - 3 indexes are kept up to date:
//          The actual index, and the indexes based on the bid and ask prices
//          The Bid/Ask index value was added to generate additional complexity
// ****************************************************************************
int fnCalculateIndex( void )
{
    // Function Removed and no longer in use - left for backwards compatability
    return TRUE;
}

// ****************************************************************************
// Function to recalcuation the NASDAQ 100 indexes - 3 indexes are kept up to date:
//          The actual index, and the indexes based on the bid and ask prices
//          The Bid/Ask index value was added to generate additional complexity
// ****************************************************************************
int fnCalculateBidAskIndex( void )
{
    // Function Removed and no longer in use - left for backwards compatability
    return TRUE;
}

//-----------------------------------------------------------------------------
//	This function will recalculate all the FV's when there is a change
//-----------------------------------------------------------------------------
void fnCalculateFV( void )
{
    // Function Removed and no longer in use - left for backwards compatability
}

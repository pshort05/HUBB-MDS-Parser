// Error Management Funcations
#ifndef __ErrorManager_h
#define __ErrorManager_h

#include <pthread.h>
#include "main.h"

#define ERROR_BUFFER_SIZE       1024
#define PROGRAM_LOG_FILE		"tmmdsreader"


extern int iErrorQueueSize;

// Error Handling
extern void *fnErrorManager( void );
extern inline void fnHandleError( char*, char* );
extern inline void fnDebug( char *);
extern inline void fnMDSLog( char *);
extern inline void fnVolumeLog( char *);
extern inline void fnAgentLog( char *);
extern int fnErrPush( struct q_head *, char *  );
extern int fnErrGet( struct q_head *, char * );
extern int fnErrStatus( struct q_head * );
extern int fnErrorInit( struct q_head * );
extern void fnCloseErrorLog( void );

#endif

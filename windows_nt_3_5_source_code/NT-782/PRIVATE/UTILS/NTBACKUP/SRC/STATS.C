/*****************************************************************************
Copyright(c) Maynard Electronics, Inc. 1984-89

     Name:         stats.c

     Description:

     $Log:   G:/UI/LOGFILES/STATS.C_V  $

   Rev 1.5   26 Jul 1993 19:32:06   MARINA
enable c++

   Rev 1.4   07 Oct 1992 15:01:56   DARRYLP
Precompiled header revisions.

   Rev 1.3   04 Oct 1992 19:40:50   DAVEV
Unicode Awk pass

   Rev 1.2   29 May 1992 16:03:44   JOHNWT
PCH updates

   Rev 1.1   29 Dec 1991 11:35:50   MIKEP
remove msassert we were hitting

   Rev 1.0   20 Nov 1991 19:34:34   SYSTEM
Initial revision.

*****************************************************************************/

#include "all.h"

#ifdef SOME
#include "some.h"
#endif

/*****************************************************************************

     Name:         ST_StartOperation()

     Description:

     Returns:

*****************************************************************************/
VOID ST_StartOperation(

STATS_PTR stats_ptr )
{
     time_t t_time ;

     memset( stats_ptr, 0, sizeof( STATS ) ) ;
     time( &t_time ) ;
     ST_SetOPStartTime( stats_ptr, t_time ) ;

     return ;

}
/*****************************************************************************

     Name:         ST_EndOperation()

     Description:

     Returns:

*****************************************************************************/
VOID ST_EndOperation(

STATS_PTR stats_ptr )
{
     time_t t_time ;

     time( &t_time ) ;
     ST_SetOPEndTime( stats_ptr, t_time ) ;

     return ;

}
/*****************************************************************************

     Name:         ST_StartOperationIdle()

     Description:

     Returns:

*****************************************************************************/
VOID ST_StartOperationIdle( 

STATS_PTR stats_ptr )
{
     time_t t_time ;

     if( ST_OPIdleLevel( stats_ptr ) == 0 ) {
          time( &t_time ) ;
          ST_SetOPStartIdle( stats_ptr, t_time ) ;
     }
     else {
          msassert( ST_GetOPStartIdle( stats_ptr ) != 0L ) ;
     }

     ST_PushOPIdleLevel( stats_ptr ) ;

     return ;

}
/*****************************************************************************

     Name:         ST_EndOperationIdle()

     Description:

*****************************************************************************/
VOID ST_EndOperationIdle( 

STATS_PTR stats_ptr )
{
     time_t t_time ;

     if( ST_OPIdleLevel( stats_ptr ) == 1 ) {
          time( &t_time ) ;
          ST_AddOPIdle( stats_ptr, ( UINT32 )( t_time - ST_GetOPStartIdle( stats_ptr ) ) ) ;
          ST_SetOPStartIdle( stats_ptr, 0L ) ;
     }

     ST_PopOPIdleLevel( stats_ptr ) ;

     return ;

}
/*****************************************************************************

     Name:         ST_StartBackupSet()

     Description:

     Returns:

*****************************************************************************/
VOID ST_StartBackupSet( 

STATS_PTR stats_ptr )
{
     time_t t_time ;

     memset( stats_ptr, 0, sizeof( STATS ) ) ;
     time( &t_time ) ;
     ST_SetBSStartTime( stats_ptr, t_time ) ;

     return ;

}
/*****************************************************************************

     Name:         ST_EndBackupSet()

     Description:

     Returns:

*****************************************************************************/
VOID ST_EndBackupSet(

STATS_PTR stats_ptr )
{
     time_t t_time ;

     time( &t_time ) ;
     ST_SetBSEndTime( stats_ptr, t_time ) ;

     return ;

}
/*****************************************************************************

     Name:         ST_StartBackupSetIdle()

     Description:

     Returns:
*****************************************************************************/
VOID ST_StartBackupSetIdle(

STATS_PTR stats_ptr )
{
     time_t t_time ;

     if( ST_BSIdleLevel( stats_ptr ) == 0 ) {
          time( &t_time ) ;
          ST_SetBSStartIdle( stats_ptr, t_time ) ;
     }
     else {
          msassert( ST_GetBSStartIdle( stats_ptr ) != 0L ) ;
     }

     ST_PushBSIdleLevel( stats_ptr ) ;

     return ;

}
/*****************************************************************************

     Name:         ST_EndBackupSetIdle()

     Description:

     Returns:

*****************************************************************************/
VOID ST_EndBackupSetIdle(

STATS_PTR stats_ptr )
{
     time_t t_time ;

     if( ST_BSIdleLevel( stats_ptr ) == 1 ) {
          time( &t_time ) ;
          ST_AddBSIdle( stats_ptr, ( UINT32 )( t_time - ST_GetBSStartIdle( stats_ptr ) ) ) ;
          ST_SetBSStartIdle( stats_ptr, 0L ) ;
     }

     ST_PopBSIdleLevel( stats_ptr ) ;

     return ;

}


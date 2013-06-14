/*****************************************************************************

   (C) COPYRIGHT 2010 TECHNOLUTION B.V., GOUDA NL
   =======          I                   ==          I    =
     I             I                    I          I
 |    I   ===   === I ===  I ===   ===   I  I    I ====  I   ===  I ===
 |    I  /   \ I    I/   I I/   I I   I  I  I    I  I    I  I   I I/   I
 |    I  ===== I    I    I I    I I   I  I  I    I  I    I  I   I I    I
 |    I  \     I    I    I I    I I   I  I  I   /I  \    I  I   I I    I
 |    I   ===   === I    I I    I  ===  ===  === I   ==  I   ===  I    I
 |                 +---------------------------------------------------+
 +----+            |  +++++++++++++++++++++++++++++++++++++++++++++++++|
 |            |             ++++++++++++++++++++++++++++++++++++++|
 +------------+                          +++++++++++++++++++++++++|
 ++++++++++++++|
 +++++|

   -----------------------------------------------------------------------------
   Title      :  AM3_Audio.c
   Author     :  E.Uijlenbroek (erno.uijlenbroek@technolution.nl)
   -----------------------------------------------------------------------------

   Description:  AM3 API Client for the Audio category

 ******************************************************************************/

/* ************************************************************************* */
/* Includes.                                                                 */
/* ************************************************************************* */
#include <stdint.h>
#include <string.h>

#include "VDK.h"

#include "manifest.h"

#include "error.h"
#include "tl_log.h"

#include "api.h"
#include "api_common.h"
#include "api_client.h"
#include "api_supervisor.h"
#include "api_audio.h"

/* ************************************************************************* */
/* Defines.                                                                  */
/* ************************************************************************* */


/* ************************************************************************* */
/* Typedefs.                                                                 */
/* ************************************************************************* */


/* ************************************************************************* */
/* Prototypes module private routines.                                       */
/* ************************************************************************* */


/* ************************************************************************* */
/* Module static variables.                                                  */
/* ************************************************************************* */
static const EModuleID _logId = TL_LOG_API_CLIENT_ID;


/* ************************************************************************* */
/* Routines.                                                                 */
/* ************************************************************************* */

int AM3_Audio_Start(char *function)
{
    int result = APIClientSend(API_AUDIO, API_AUDIO_CMD_START, function);

    if (result != OK) {
        TlLogError("Could not start function %s at AM3" ,function);
    }
    
    return result;
}

int AM3_Audio_Stop(char *function)
{
    int result = APIClientSend(API_AUDIO, API_AUDIO_CMD_STOP, function);

    if (result != OK) {
        TlLogError("Could not stop function %s at AM3" ,function);
    }
    
    return result;
}

int AM3_Audio_SetInMute(char *channel, bool on)
{
    int result = APIClientSend(API_AUDIO, API_AUDIO_CMD_SETINMUTE, "%s %d", channel, on ? 1 : 0);

    if (result != OK) {
        TlLogError("Could not set mute channel %s at AM3" ,channel);
    }
    
    return result;
}


int AM3_Alive(void)
{
    int result = APIClientSend(API_SUPERVISOR, API_SUPERVISOR_CMD_ALIVE, "AIBo");

    if (result != OK) {
        TlLogError("Could not send alive to AM3");
    }
    
    return result;
}


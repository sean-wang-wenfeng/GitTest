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
   Title      :  api_aibo.c
   Author     :  E.Uijlenbroek (erno.uijlenbroek@technolution.nl)
   -----------------------------------------------------------------------------

   Description:  composes the API specific to the AIBO

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

#include "memorymap.h"
#include "prodinfo.h"

#include "api_common.h"
#include "api_exceptions.h"
#include "api_configuration.h"
#include "api_audio.h"
#include "api_supervisor.h"
#include "api_sensors.h"
#include "api_server.h"
#include "api_client.h"
#include "api_guard.h"
#include "api_uart.h"
#include "api_events.h"

#include "cmd.h"

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

static VDK_ThreadID apiServerThread;
static VDK_ThreadID apiClientThread;
static VDK_ThreadID apiGuardThread;


/* ************************************************************************* */
/* Routines.                                                                 */
/* ************************************************************************* */


/******************************************************************************
    SetAPIFolders
    =================

    Set up the monitor folder structure.

    Parameters:
    IN:     none
    OUT:    none
    INOUT:  none

    Return: true
            false
******************************************************************************/
static bool SetAPIFolders(void)
{
    bool r = true;

    if (r) {    
        r = APIBistInit(NULL);
    }

    if (r) {
        r = APIUpdateInit(NULL);
    }

    if (r) {
        r = APIConfigurationInit(NULL, NULL);
    }

    if (r) {
        r = APIUARTInit(NULL);
    }
    
    if (r) {
        r = APISensorsInit(NULL);
    }

    
    if (r) {
        r = APIAudioInit(NULL);
    }
    
    if (r) {
        r = APISupervisorInit(NULL);
    }    

    if (r) {
        r = APIEventsInit(NULL);
    }
    
    if (r) {
        APIEthInit(NULL);
    }
        
    return r;
}

/******************************************************************************
    APIInit
    =======

    API main process.

    Parameters:
    IN:     none
    OUT:    none
    INOUT:  none

    Return: OK
            ERROR
******************************************************************************/
int APIInit(void)
{
    bool r = true;
    
    TAPIClientPar apiClientPar;
    VDK_ThreadCreationBlock tcb;

    if (r) {
        r = CmdHdlAddException(NULL, ARGUMENT_EXCEPT);
    }

    if (r) {
        r = CmdHdlAddException(NULL, ERROR_EXCEPT);
    }

    if (r) {
        r = SetAPIFolders();
    }

    APIGuardInit();

    apiGuardThread = VDK_CreateThread(kapiGuard_threadType);   
    if (apiGuardThread == UINT_MAX) {
    	r = false;
    }

    if (ProdInfoRead(FLASH_CFG_BLOCK, PIBCAT_NETWORK, PIBITM_AMIP, apiClientPar.ip , sizeof(apiClientPar.ip)) != E_OK) {
        r = false;
    }    
    else {
        APIClientInit();
        
        apiClientPar.port = API_SERVER_PORT;
        
        tcb.template_id                    = kapiClient_threadType;
        tcb.thread_stack_size              = 0;                                 // use kernel setting
        tcb.thread_priority                = (VDK_Priority)0;                   // use kernel setting
        tcb.user_data_ptr                  = &apiClientPar;
    
        apiClientThread = VDK_CreateThreadEx(&tcb);   
        if (apiClientThread == UINT_MAX) {
            r = false;
        }
    }    
    
    APIServerInit();    
    
    apiServerThread = VDK_CreateThread(kapiServer_theadType);   
    if (apiServerThread == UINT_MAX) {
        r = false;
    }
    
    return r ? OK : ERROR;

}


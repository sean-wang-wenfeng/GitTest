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
   Title      :  api_supervisor.c
   Author     :  E.Uijlenbroek (erno.uijlenbroek@technolution.nl)
   -----------------------------------------------------------------------------

   Description:  api to the supervisor module.

 ******************************************************************************/

/* ************************************************************************* */
/* Includes.                                                                 */
/* ************************************************************************* */
#include <stdint.h>
#include <string.h>

#include "VDK.h"

#include "manifest.h"

#include "error.h"
#include "tl_assert.h"

#include "cmd.h"

#include "api_common.h"
#include "api_configuration.h"
#include "api_exceptions.h"

#include "api_supervisor.h"

#include "gpio.h"
#include "timer.h"
#include "player.h"
#include "am3.h"
#include "patcom.h"
#include "event.h"

#include "sysmon.h"

/* ************************************************************************* */
/* Defines.                                                                  */
/* ************************************************************************* */
#define OP_POLLRATE 200


#define OP_1500MS ((int)(15000/OP_POLLRATE))
#define OP_4000MS ((int)(40000/OP_POLLRATE))


/* ************************************************************************* */
/* Typedefs.                                                                 */
/* ************************************************************************* */


/* ************************************************************************* */
/* Prototypes module private routines.                                       */
/* ************************************************************************* */


/* ************************************************************************* */
/* Module static variables.                                                  */
/* ************************************************************************* */
static bool ignoreSupErr = false;
static VDK_SemaphoreID opLock;
static bool opFakeButtonState = false;

static VDK_ThreadID    opThread;


/* ************************************************************************* */
/* Routines.                                                                 */
/* ************************************************************************* */


static void CallActive(void)
{
    int result = OK; // For debugging

    result |= EventOperatorCall(EV_ACTIVE);

    // Ringtone at AM3
    result |= AM3_Audio_Start(API_AUDIO_FUNC_NC);
    // Ringtone at AIBo
    result |= PlayerPlayStream(PLAY_RINGTONE);
}

static void CallInactive(void)
{
    int result = OK; // For debugging
    
    result |= EventOperatorCall(EV_INACTIVE);

    // Stop Ringtone at AM3
    result |= AM3_Audio_Stop(API_AUDIO_FUNC_NC);
    // Stop Ringtone at AIBo
    result |= PlayerStopStream(PLAY_RINGTONE);   
}

void APISupervisorFakeOpCall(bool active)
{
    VDK_PendSemaphore(opLock, ADI_SEM_TIMEOUT_FOREVER);

    opFakeButtonState = active;
    VDK_PostSemaphore(opLock);
}    

static bool GetButtonState(void)
{

    bool opButtonState;
    bool state;
    VDK_PendSemaphore(opLock, ADI_SEM_TIMEOUT_FOREVER);
#if defined(__ADSP_BRAEMAR__)       /* peripheral IDs for BF534, BF536, BF537 */
// NO GPIO left

#else
    GPIOGet(GPIO_OPERATORCALL, &opButtonState);            
#endif                
    state = opButtonState || opFakeButtonState;
    VDK_PostSemaphore(opLock);

    return state;
}


void APISupervisorOperatorCallProc(void)
{
    int pressedTime = 0;
    int upTime = 0;
    bool buttonShort = false;
    bool prevOpButtonState = false;
    bool opButtonState = false;
    bool operatorCallActive = false;
    
    while (true) {
         // Polling mode
        SysMonReport(SYSMON_REPORTSOON);

        opButtonState = GetButtonState();

        if (opButtonState) {                // Button down             	
            if (!prevOpButtonState) {
                pressedTime = 0;
                CallActive();
            }
            else {
                pressedTime++;
            }
            
            if (pressedTime == OP_1500MS)  { // Reached 1500ms in down state  
                SupervisorSignalEv(EvOperatorCall);   // signal EVENT
                buttonShort = false;                   
            } 
        }
        else {                              // Button up
            if (prevOpButtonState) {
                upTime = 0;
                CallInactive();
                     
            	if (pressedTime < OP_1500MS) {
                	if (buttonShort) {      // The previous button was too short aswell                                                                                                       
                    	SupervisorSignalEv(EvOperatorCall);   // signal EVENT                       
                        buttonShort = false;         
	                }
                    else {
    	                buttonShort = true;         // This button was too short. Remain alert!
                	}
            	}
            }
            else {
                upTime++;
            }

            
            if (upTime == OP_4000MS) { // Reached 4000ms in up state 
                buttonShort = false;                   
            }
        }
         
        prevOpButtonState = opButtonState;     

        VDK_Sleep(OP_POLLRATE);
    }   
}

static void DoSetRq(TCmdResult *result, int argc, char *argv[])
{
    int rq;

    if (APINameToNumber(argv[1], patcomRequests, &rq) != OK) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }
    
    if (SupervisorSetRq((EPatComRequests)rq, strcmp(argv[2], API_TRUE) == 0) != OK && !ignoreSupErr) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }
    
    CmdResultAddNoError(result, true);
}

static void DoGetRq(TCmdResult *result, int argc, char *argv[])
{
    bool status;
    int  rq;

    if (APINameToNumber(argv[1], patcomRequests, &rq) != OK) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }

    if (SupervisorGetRq((EPatComRequests)rq, &status) == OK) {
        CmdResultAddBOOL(result, status);
        return;
    }
    CmdResultAddNoError(result, false);
}

static void DoSetCd(TCmdResult *result, int argc, char *argv[])
{
    int cd;

    if (APINameToNumber(argv[1], patcomConditions, &cd) != OK) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }

    if (SupervisorSetCd((EPatComConditions)cd, strcmp(argv[2], API_TRUE) == 0) != OK && !ignoreSupErr) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }
    
    CmdResultAddNoError(result, true);
}

static void DoGetCd(TCmdResult *result, int argc, char *argv[])
{
    bool status;
    int  cd;

    if (APINameToNumber(argv[1], patcomConditions, &cd) != OK) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }

    if (SupervisorGetCd((EPatComConditions)cd, &status) == OK) {
        CmdResultAddBOOL(result, status);
        return;
    }
    CmdResultAddNoError(result, false);
}

static void DoSetInd(TCmdResult *result, int argc, char *argv[])
{
    int ind;

    if (APINameToNumber(argv[1], patcomIndicators, &ind) != OK) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }

    if (SupervisorSetInd((EPatComIndicators)ind, strcmp(argv[2], API_TRUE) == 0) != OK && !ignoreSupErr) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }
    
    CmdResultAddNoError(result, true);
}

static void DoGetInd(TCmdResult *result, int argc, char *argv[])
{
    bool status;
    int  ind;

    if (APINameToNumber(argv[1], patcomIndicators, &ind) != OK) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }

    if (SupervisorGetInd((EPatComIndicators)ind, &status) == OK) {
        CmdResultAddBOOL(result, status);
        return;
    }
    CmdResultAddNoError(result, false);
}

static void DoSigEv(TCmdResult *result, int argc, char *argv[])
{
    int ev;

    if (APINameToNumber(argv[1], patcomEvents, &ev) != OK) {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }
    
    if (SupervisorSignalEv((EPatComEvents)ev) != OK && !ignoreSupErr)  {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
        return;
    }
    
    CmdResultAddNoError(result, true);
}

static void DoLTP(TCmdResult *result, int argc, char *argv[])
{
    if (strcmp(argv[1], API_ON) == 0) {
        CmdResultAddNoError(result, SupervisorListenToPatient(true) == OK || ignoreSupErr);
    }
    else if (strcmp(argv[1], API_OFF) == 0) {
        CmdResultAddNoError(result, SupervisorListenToPatient(false) == OK || ignoreSupErr);
    }        
    else {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
    }
}

static void DoXRUITTP(TCmdResult *result, int argc, char *argv[])
{
    if (strcmp(argv[1], API_ON) == 0) {
        CmdResultAddNoError(result, SupervisorXRUITTP(true) == OK || ignoreSupErr);
    }
    else if (strcmp(argv[1], API_OFF) == 0) {
        CmdResultAddNoError(result, SupervisorXRUITTP(false) == OK || ignoreSupErr);
    }        
    else {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
    }
}

static void DoRecordingMode(TCmdResult *result, int argc, char *argv[])
{
    int res = ERROR;

    if (strcmp(argv[1], API_ON) == 0) {
        CmdResultAddNoError(result, SupervisorRecordingMode(true) == OK || ignoreSupErr);
    }
    else if (strcmp(argv[1], API_OFF) == 0) {
        CmdResultAddNoError(result, SupervisorRecordingMode(false) == OK || ignoreSupErr);        
    }        
    else {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
    }
}

static void DoAutovoice(TCmdResult *result, int argc, char *argv[])
{
    int res = ERROR;

    if (strcmp(argv[1], API_ON) == 0) {
        CmdResultAddNoError(result, SupervisorAutovoice(true) == OK || ignoreSupErr);
    }
    else if (strcmp(argv[1], API_OFF) == 0) {
        CmdResultAddNoError(result, SupervisorAutovoice(false) == OK || ignoreSupErr);
    }        
    else {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
    }
}

static void DoPuppet(TCmdResult *result, int argc, char *argv[])
{
    int res = ERROR;

    if (strcmp(argv[1], API_ON) == 0) {
        CmdResultAddNoError(result, SupervisorPuppet(true) == OK || ignoreSupErr);
    }
    else if (strcmp(argv[1], API_OFF) == 0) {
        CmdResultAddNoError(result, SupervisorPuppet(false) == OK || ignoreSupErr);
    }
    else {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
    }    
}

static void DoInitialize(TCmdResult *result, int argc, char *argv[])
{
    CmdResultAddNoError(result, (SupervisorReInitialize() == OK) || ignoreSupErr);
}

static void DoIgnoreSupErr(TCmdResult *result, int argc, char *argv[])
{
    if (strcmp(argv[1], API_ON) == 0) {
        ignoreSupErr = true;
        CmdResultAddNoError(result, true);
    }
    else if (strcmp(argv[1], API_OFF) == 0) {
        ignoreSupErr = false;
        CmdResultAddNoError(result, true);        
    }        
    else {
        CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, API_FAIL);
    }        


}

static void DoAlive(TCmdResult *result, int argc, char *argv[])
{    
    CmdResultAddNoError(result, true);
}

static void DoSysMonStart(TCmdResult *result, int argc, char *argv[])
{
    CmdResultAddNoError(result, SysMonStart() == OK);
}

static void DoSysMonStop(TCmdResult *result, int argc, char *argv[])
{
    CmdResultAddNoError(result, SysMonStop() == OK);
}



//! Table containing all available commands
static TCmds Cmds[] = {
    { "SetRq",           DoSetRq,			2,                 2,             "<request> <bool>",                "Set the request status"     },
    { "GetRq",           DoGetRq,			1,                 1,             "<request>",                       "Get the request status"     },
    { "SetCd",           DoSetCd,			2,                 2,             "<condition> <bool>",              "Set the condition status"   },
    { "GetCd",           DoGetCd,			1,                 1,             "<condition>",                     "Get the condition status"   },
    { "SetInd",          DoSetInd,			2,                 2,             "<indicator> <bool>",              "Set indicator status"       },
    { "GetInd",          DoGetInd,			1,                 1,             "<indicator>",                     "Get indicator status"       },
    { "SigEv",           DoSigEv,			1,                 1,             "<event>",                         "Signal event"               },
    { "ListenToPatient", DoLTP,	            1,                 1,             "<on>",                            "Set RqListenToPatient"      },
    { "XRUITTP",         DoXRUITTP,			1,                 1,             "<on>",                            "Set XRUITTP on/off"         },
    { "RecordingMode",   DoRecordingMode,	1,                 1,             "<on>",                            "Set recording mode"         },
    { "Autovoice",       DoAutovoice,       1,                 1,             "<on>",                            "Set Autovoice"              },

    { API_SUPERVISOR_CMD_PUPPET,          DoPuppet,			1,                 1,             "<on>",                            "Set puppet mode"            },
    { "Initialize",      DoInitialize,		0,                 0,             "",                                "Initialize to default"      },
    { "IgnoreSupErr",    DoIgnoreSupErr,    1,                 1,             "<on>",                            "Ignore supervisor errors"   },
    { API_SUPERVISOR_CMD_ALIVE,     DoAlive,0,                 1,             "[who]"                               ""            },
    { "SysMonStart",     DoSysMonStart,     0,                 0,             "",                                "Starts watchdog" },
    { "SysMonStop",      DoSysMonStop,      0,                 0,             "",                                "Stops watchdog" },
    { NULL,              0,					0,                 0,             NULL,                              NULL                         }
};

/*! \brief Load audio folder into CmdHdl
 *  \return true if successful
 */
bool APISupervisorInit(TCmdResult *result)
{
    bool r = true;

    opLock = VDK_CreateSemaphore(1, 1, 1, 0);

    opThread = VDK_CreateThread(kapiSupOperatorCall_threadType);

    if (r) {
        r = CmdHdlAddException(result, ARGUMENT_EXCEPT);
    }

    if (r) {
        r = CmdHdlAddException(result, ERROR_EXCEPT);
    }

    if (r) {
        r = CmdHdlAddFolderCmdSet(result, "/" API_SUPERVISOR, Cmds);
    }
    return r;
}


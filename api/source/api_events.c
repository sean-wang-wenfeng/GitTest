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
   Title      :  api_events.c
   Author     :  E.Uijlenbroek (erno.uijlenbroek@technolution.nl)
   -----------------------------------------------------------------------------

   Description: An api that is used to implement an event client interface.
                This is called using the event channel but may be called
                as regular API aswell.
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

#include "cmd.h" 
#include "api_common.h"
#include "api_audio.h"
#include "event.h"

#include "supervisor.h"

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


/* ************************************************************************* */
/* Routines.                                                                 */
/* ************************************************************************* */

static void DoButton(TCmdResult *result, int argc, char *argv[])
{
    int res = ERROR;

    if (!SupervisorGetPuppet()) {
     
        if (strcmp(argv[1], EV_BUTTON_OHS) == 0) {
            /* NO ACTION FOR THE OHS BUTTON */
            res = OK;
        }
        else if (strcmp(argv[1], EV_BUTTON_SPKUP) == 0) {
            /* NO ACTION FOR THE AM3 SPEAKER BUTTON */
            res = OK;
        }
        else if (strcmp(argv[1], EV_BUTTON_SPKDOWN) == 0) {
            /* NO ACTION FOR THE AM3 SPEAKER BUTTON */
            res = OK;        
        }
        else if (strcmp(argv[1], EV_BUTTON_MICUP) == 0) {
            /* NO ACTION FOR THE AM3 MIC BUTTON */
            res = OK;        
        }
        else if (strcmp(argv[1], EV_BUTTON_MICDOWN) == 0) {
            /* NO ACTION FOR THE AM3 MIC BUTTON */
            res = OK;        
        }
        else if (strcmp(argv[1], EV_BUTTON_MUSUP) == 0) {
            if (strcmp(argv[2], EV_BUTTON_PRESSED) == 0) {
                res  = APIAudioVolUp(TO_SPEAKERS, API_AUDIO_IN_MUSIC);
                res |= APIAudioVolUp(TO_SPEAKERS, API_AUDIO_IN_AMBIENT);
            }
            else if (strcmp(argv[2], EV_BUTTON_RELEASED) == 0) {
                /* No action on release */
                res = OK;            
            }
            else {
                res = ERROR;
            }
        }
        else if (strcmp(argv[1], EV_BUTTON_MUSDOWN) == 0) {
            if (strcmp(argv[2], EV_BUTTON_PRESSED) == 0) {
                res  = APIAudioVolDown(TO_SPEAKERS, API_AUDIO_IN_MUSIC);
                res |= APIAudioVolDown(TO_SPEAKERS, API_AUDIO_IN_AMBIENT);
            }
            else if (strcmp(argv[2], EV_BUTTON_RELEASED) == 0) {
                /* No action on release */
                res = OK;            
            }
            else {
                res = ERROR;
            }
        }
        else if (strcmp(argv[1], EV_BUTTON_LTP) == 0) {
            if (strcmp(argv[2], EV_BUTTON_PRESSED) == 0) {
                res = SupervisorSignalEv(EvListenToPatient);
            }
            else if (strcmp(argv[2], EV_BUTTON_RELEASED) == 0) {
                /* No action on release */
                res = OK;            
            }
            else {
                res = ERROR;
            }
        }
        else if (strcmp(argv[1], EV_BUTTON_TTP) == 0) {
            if (strcmp(argv[2], EV_BUTTON_PRESSED) == 0) {
                res = SupervisorSignalEv(EvTalkToPatientOpPressed);
            }
            else if (strcmp(argv[2], EV_BUTTON_RELEASED) == 0) {
                res = SupervisorSignalEv(EvTalkToPatientOpReleased);
            }
            else {
                res = ERROR;
            }
        }
        else if (strcmp(argv[1], EV_BUTTON_MUS) == 0) {
            if (strcmp(argv[2], EV_BUTTON_PRESSED) == 0) {
                res = SupervisorSignalEv(EvMusic);
            }
            else if (strcmp(argv[2], EV_BUTTON_RELEASED) == 0) {
                /* No action on release */
                res = OK;            
            }
            else {
                res = ERROR;
            }
        }
        else {
            res = ERROR;
        }
    }
    else {
        res = OK; // Ignore all when in puppet mode
    }
    
    CmdResultAddNoError(result, res == OK);
}

static void DoBluetooth(TCmdResult *result, int argc, char *argv[])
{
    int res = OK;
    int event;

    res = APINameToNumber(argv[1], patcomEvents, &event);

    if (res == OK) {
        switch (event) {
        case EvUseOpHeadSet:
        case EvStartHSPairing:
        case EvConnectHeadSet:
        case EvDisconnectHeadSet:
            res = SupervisorSignalEv((EPatComEvents)event);
            break;
        default:
            res = ERROR;
            break;
        }
    }
    CmdResultAddNoError(result, res == OK);
}

// Fake operator call button event.
static void DoFakeOpCall(TCmdResult *result, int argc, char *argv[])
{
    APISupervisorFakeOpCall(strcmp(argv[1], EV_ACTIVE) == 0);
        
    CmdResultAddNoError(result, true);
}    

static TCmds Cmds[] = {
    { EV_BUTTON,	DoButton,          2,                 2,                 "<button> <event>"                   ""               },
    { EV_BLUETOOTH, DoBluetooth,       1,                 1,                 "<event>"                            ""               },
    { EV_OPERATORCALL, DoFakeOpCall,   1,                 1,                 "<event>"                            ""               },
    { NULL,			0,                 0,                 0,                 NULL,                               NULL              }
};

/*! \brief Load audio folder into CmdHdl
 *  \return true if successful
 */
bool APIEventsInit(TCmdResult *result)
{
    bool r = true;

    if (r) {
        r = CmdHdlAddFolderCmdSet(result, "/EV", Cmds);
    }
    return r;
}


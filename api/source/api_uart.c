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
   Title      :  api_uart.c
   Author     :  E.Uijlenbroek (erno.uijlenbroek@technolution.nl)
   -----------------------------------------------------------------------------

   Description:  UART category

 ******************************************************************************/

/* ************************************************************************* */
/* Includes.                                                                 */
/* ************************************************************************* */
#include <stdint.h>
#include <string.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

#include <services/services.h>      // system service includes
#include "Adi_tool_chain.h"

#include "VDK.h"

#include "manifest.h"

#include "error.h"
#include "tl_assert.h"
#include "tl_log.h"

#include "cmd.h"

#include "api.h"
#include "api_common.h"
#include "api_exceptions.h"

#include "sysmon.h"

#include "quart.h"


/* ************************************************************************* */
/* Defines.                                                                  */
/* ************************************************************************* */
#define MAGIC_START_CHAR '+'

/* ************************************************************************* */
/* Typedefs.                                                                 */
/* ************************************************************************* */
#define UART_API_LATENCY    9  // Latency in ms

#define UART_POLL_INTERVAL 5000 

typedef struct {
    int dev;                        // UART device number
    TCmdResult *portOpenResult;     // The API Cmd result used for the open command
    VDK_ThreadID apiUartRxThread;   // Thread handle of the Rx thread
    VDK_SemaphoreID uartRxFinished;     // Signals the Rx thread has closed down
    bool        finished;           // indicates the host is finished with the uart
} TUARTADMIN;    

/* ************************************************************************* */
/* Prototypes module private routines.                                       */
/* ************************************************************************* */


/* ************************************************************************* */
/* Module static variables.                                                  */
/* ************************************************************************* */\
static const EModuleID _logId = TL_LOG_API_SERVER_ID;
static const char threadNameRx[] = "UARTRx";
static const char threadNameTx[] = "UARTTx";

static VDK_SemaphoreID uartLock;    // Lock the UART administration
static TUARTADMIN uartAdmin[UART_MAX_DEV];

/* ************************************************************************* */
/* Routines.                                                                 */
/* ************************************************************************* */

void APIUARTRxProc(TUARTADMIN *admin) 
{
    bool done = false;
    int dev = admin->dev;
    int charactersToSend;
    int charactersSend;
    int j;
    TConnectedAPI *connAPI = (TConnectedAPI *)admin->portOpenResult->outputInfo;

    TlLogAlways("APIUARTRxProc Opening");

    SysMonSetName(threadNameRx);

    TlLogAlways("APIUARTRxProc for dev %d", dev);

    SysMonReport(SYSMON_REPORTSOON);
    
    VDK_PendSemaphore(admin->uartRxFinished, ADI_SEM_TIMEOUT_FOREVER);

    charactersSend = 0;
    charactersToSend = 1;
    connAPI->outMessage[0] = MAGIC_START_CHAR;    
    while (!done) {
        if (charactersToSend == 0) {
        SysMonReport(SYSMON_REPORTSOON);
            if (UartCharAvailable(dev, UART_POLL_INTERVAL)) {
                // Sleep to let more pour in
                VDK_Sleep(UART_API_LATENCY * 10);            
                // Leech them from the uart
                while ((!done) && (UartCharAvailable(dev, 0)) && (charactersToSend < sizeof(connAPI->outMessage))) {             
                    if (UartGet(dev, (uint8_t*)&connAPI->outMessage[charactersToSend]) == OK) {
                        charactersToSend++;
                    }
                    else {
                        done = true;
                    }
                }  
                charactersSend = 0;
            }
        }

        if (!done) { // First!!! check for finished as the send is unneeded when the host has indicated finished.
            VDK_PendSemaphore(uartLock, ADI_SEM_TIMEOUT_FOREVER);
            done = admin->finished;            
            VDK_PostSemaphore(uartLock);
        } 

        
        if (!done && (charactersToSend != 0)) { // Characters are available
            SysMonReport(SYSMON_REPORT_TCPTIME);                  
            j = send(connAPI->connectedSocket, &connAPI->outMessage[charactersSend], charactersToSend, 0);
            if (j < 0) { // Error!
                done = true;
            }
            if (j == 0) { // Nothing sent, why?
                VDK_Sleep(1000);
            } 
            else {                    
                charactersSend = charactersSend + j;
                charactersToSend = charactersToSend - j;
            }                     
        }                
           
    }   
    
    VDK_PostSemaphore(admin->uartRxFinished);

    TlLogAlways("APIUARTRxProc Closing dev %d", dev);
    
    SysMonReport(SYSMON_NOREPORT);
    SysMonDelName();
}


static void DoUARTOpen(TCmdResult *result, int argc, char *argv[])
{
    int dev;
    int baud;
    int bits;
    int stop;
    int par;
    int recLen;
    int t0, t1;
    bool fault = false;
    bool done = false;
    TConnectedAPI *connAPI = (TConnectedAPI *)result->outputInfo;
    VDK_ThreadCreationBlock tcb;

    Assert (result->outputInfo != NULL);
    
    if (VDK_PendSemaphore(uartLock, ADI_SEM_TIMEOUT_FOREVER)) {      
        dev  = strtoul(argv[1], NULL, 0);
        if (dev < 0 || dev >= UART_MAX_DEV) {
            CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, "argument 1 error");
            fault = true;
        }            
        if (!fault) { 
            if (uartAdmin[dev].portOpenResult != NULL) {
                TlLogError("UART %d already open", dev);
                CmdResultAddEXCEPTION(result, ERROR_EXCEPT, "UART already open");                
                fault = true;
            }
        }
        if (!fault) { 
            baud = strtoul(argv[2], NULL, 0);
            if (baud < 0 || dev > UART_MAX_BAUD) {
                CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, "argument 2 error");
                fault = true;
            }            
        }            
        if (!fault) {         
            bits = strtoul(argv[3], NULL, 0);
            if (bits < 5 || bits > 8) {
                CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, "argument 3 error");
                fault = true;
            }                    
        }
        if (!fault) { 
            if (strcmp(argv[4], "none") == 0) {
                par  = 0;
            }            
            else if (strcmp(argv[4], "odd") == 0) {
                par  = 1;
            }
            else if (strcmp(argv[4], "even") == 0) {
                par  = 2;
            }            
            else {
                CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, "argument 4 error");
                fault = true;
            }            
        }
        if (!fault) { 
            stop = strtoul(argv[5], NULL, 0);
            if (stop < 1 || stop > 2) {
                CmdResultAddEXCEPTION(result, ARGUMENT_EXCEPT, "argument 5 error");
                fault = true;
            }            
        }
        if (!fault) { 
            if (UartConfigure(dev, baud, bits, stop, par) != OK) {
                CmdResultAddEXCEPTION(result, ERROR_EXCEPT, "Failed to open UART");                
                fault = true;
            }            
        }   
        if (!fault) { 
            if (UartFlush(dev) != OK) {
                CmdResultAddEXCEPTION(result, ERROR_EXCEPT, "Failed to flush UART");                
                fault = true;
            }            
        }   
            
        
        if (!fault) {  // If no fault then uarts are operable!
            uartAdmin[dev].portOpenResult = result; // First claim this port

            // Check if previous session has closed
            VDK_PostSemaphore(uartLock); // Remove possible deadlock
            if(VDK_GetSemaphoreValue(uartAdmin[dev].uartRxFinished) == 0) { // Zero count means an Rx Thread is still busy      
                TlLogAlways("DoUARTOpen Waiting for previous Rx Thread");
                VDK_Sleep(UART_POLL_INTERVAL * 4); // Wait for 4 * the poll interval
                if(VDK_GetSemaphoreValue(uartAdmin[dev].uartRxFinished) == 0) {
                    TlLogAlways("DoUARTOpen Previous Rx Thread NOT Closed");
                    CmdResultAddEXCEPTION(result, ERROR_EXCEPT, "Previous Session not closed"); 
                    fault = true;
                }
            }
            VDK_PendSemaphore(uartLock, ADI_SEM_TIMEOUT_FOREVER); // Re-protect
         }

         if (!fault) {                  
            uartAdmin[dev].finished = false;
            uartAdmin[dev].dev = dev;
            
            tcb.template_id         = kapiUartRx_threadType;
            tcb.thread_stack_size   = 0;                        // use kernel setting
            tcb.thread_priority     = (VDK_Priority)0;          // use kernel setting
            tcb.user_data_ptr       = &uartAdmin[dev];

            uartAdmin[dev].apiUartRxThread = VDK_CreateThreadEx(&tcb);
        
            VDK_PostSemaphore(uartLock);

            // Changing thread priority so small packets are handled quickly
            VDK_SetPriority(VDK_GetThreadID(), (VDK_Priority)2);

            SysMonSetName(threadNameTx); // Change name for better readability
            
            TlLogAlways("DoUARTOpen for dev %d", dev);
            while (!done ) {
                // Get things coming in from the host
                SysMonReport(SYSMON_REPORTLATER);
                
       			t0 = VDK_GetUptime(); // WORKAROUND FOR DEAD SOCKET 	        	                
                recLen = recv(connAPI->connectedSocket, &connAPI->inMessage[0], sizeof(connAPI->inMessage), 0);    
				t1 = VDK_GetUptime(); // WORKAROUND FOR DEAD SOCKET    
                
                APIServerLock(); // Lock the API server
                if (connAPI->disconnect) {
                    done = true; // We are done if told to disconnect or on error                    
                }                    
                APIServerUnlock();

                if (recLen <= 0) {
                    // Nothing to transmit to the uart.
                    
                	/* WORKAROUND FOR RECV ERROR Returm */
        			/* THE RECV DOES NOT RETURN AN ERROR ON DEADSOCKET BUT RETURNS TIMEOUT IMMEDIATELY */
        			/* SO t1 - t0 is zero on an error. This is no timeout! */
        			if (((t1 - t0) == 0) || (recLen < 0)) {
                        TlLogAlways("UARTTx network reception problem or socket closing on %d", dev);
 			       		done = true;
        			}  
                }
                else {
                    // Transmit received data to the uart regardless of done condition                    
                    SysMonReport(SYSMON_REPORTLATER);
                    if (UartPut(dev, (uint8_t*)&connAPI->inMessage[0], recLen) != OK) {
                        TlLogAlways("UARTTx Uart put failed %d", dev);
                        done = true;
                    }			                                                 
                }  
            }                                                                          
           
            VDK_PendSemaphore(uartLock, ADI_SEM_TIMEOUT_FOREVER);
            uartAdmin[dev].finished = true; // This will free the uart rx
            uartAdmin[dev].portOpenResult = NULL; 
            APIServerLock(); // Lock the API server
            connAPI->disconnect = true;            
            APIServerUnlock();                            
            VDK_PostSemaphore(uartLock);

            
            TlLogAlways("DoUARTOpen Finished for dev %d", dev);
            result->type = RT_NONE; // NO RESULT!!!!!
        }    
        else { // On fault uart is not operable
            VDK_PostSemaphore(uartLock);
            // No result update. Exception should be added above
        }            
    }
    
    SysMonReport(SYSMON_REPORTSOON);
}

static void DoUARTClose(TCmdResult *result, int argc, char *argv[])
{
    int dev = strtoul(argv[1], NULL, 0); 
    TConnectedAPI *connAPI;
    if (VDK_PendSemaphore(uartLock, ADI_SEM_TIMEOUT_FOREVER)) { 
        if (uartAdmin[dev].portOpenResult != NULL) {
            APIServerLock(); // Lock the API server
            connAPI = (TConnectedAPI *)uartAdmin[dev].portOpenResult->outputInfo;
            connAPI->disconnect = true;            
            APIServerUnlock(); // Lock the API server
        }
        
        VDK_PostSemaphore(uartLock);
    }        
    CmdResultAddNoError(result, true);
}   

static void DoUARTMux45(TCmdResult *result, int argc, char *argv[])
{
    CmdResultAddNoError(result, Uart45mux(strcmp(argv[1], API_ON) == 0) == OK);
}

extern int uartCallbackCalled;
static void DoUARTCb(TCmdResult *result, int argc, char *argv[])
{
    CmdResultAddINT32(result,uartCallbackCalled);
    uartCallbackCalled = 0;
}




//! Table containing all available commands
static TCmds Cmds[] = {
    { "Open", DoUARTOpen,  5,	5,  "<n> <baud> <bits> <par> <stop>", "Open UART"  },
    { "Close", DoUARTClose, 1,  1,  "<n>",                            "Force a uart close" },
    { "Mux45", DoUARTMux45, 1, 1, "<on>", "MUX B45 - B67" },
    { "Cb", DoUARTCb, 0,0,""},

    { NULL,   0,           0,	0,  NULL,                             NULL         }
};

bool APIUARTInit(TCmdResult *result)
{
    bool r = true;
    int dev;
    
    uartLock   = VDK_CreateSemaphore(1, 1, 1, 0);  
    for (dev = 0; dev < UART_MAX_DEV; dev++) {
        uartAdmin[dev].portOpenResult = NULL;
        uartAdmin[dev].uartRxFinished =  VDK_CreateSemaphore(1, 1, 1, 0);  
        uartAdmin[dev].finished = false;
        uartAdmin[dev].dev = dev;
    }  

    if (r) {
        r = CmdHdlAddException(result, ARGUMENT_EXCEPT);
    }

    if (r) {
        r = CmdHdlAddException(result, ERROR_EXCEPT);
    }

    if (r) {
        r = CmdHdlAddFolderCmdSet(result, "/UART", Cmds);
    }
    return r;
}



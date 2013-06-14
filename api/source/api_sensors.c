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
   Title      :  api_sensors.c
   Author     :  E.Uijlenbroek (erno.uijlenbroek@technolution.nl)
   -----------------------------------------------------------------------------

   Description:  Sensors category

 ******************************************************************************/


/* ************************************************************************* */
/* Includes.                                                                 */
/* ************************************************************************* */
#include <stdint.h>
#include <string.h>

#include "manifest.h"

#include "error.h"
#include "tl_assert.h"

#include "cmd.h"

#include "api_common.h"
#include "api_configuration.h"
#include "api_exceptions.h"

#include "api_sensors.h"
#include "memorymap.h"
#include "prodinfo.h"
#include "gpio.h"
#include "sensor_sht7x.h"

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


static void DoGetHumidity(TCmdResult *result, int argc, char *argv[])
{
    char sensmode[16];
    float hum;
    
    if (ProdInfoRead(FLASH_CFG_BLOCK, PIBCAT_SYSTEM, PIBITM_SENSMODE, sensmode, sizeof(sensmode)) != E_OK) {
        CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
        return; 
    }
    else {
        if (strcmp(sensmode, SENSOR_SHT7X_CFG_ID) == 0) {
            if (SensorSHT7xReadHum(&hum) != OK) {
                CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
                return; 
            }                
        }
        else {
            CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
            return;                    
        }
    }

    CmdResultAddSTRING(result, "%2.1f", hum);    
}

static void DoGetTemperature(TCmdResult *result, int argc, char *argv[])
{
    char sensmode[16];
    float temp;
    
    if (ProdInfoRead(FLASH_CFG_BLOCK, PIBCAT_SYSTEM, PIBITM_SENSMODE, sensmode, sizeof(sensmode)) != E_OK) {
        CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
        return; 
    }
    else {
        if (strcmp(sensmode, SENSOR_SHT7X_CFG_ID) == 0) {
            if (SensorSHT7xReadTemp(&temp) != OK) {
                CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
                return; 
            }                
        }
        else {
            CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
            return;                    
        }
    }

    CmdResultAddSTRING(result, "%2.1f", temp);   
}

static void DoTestmode(TCmdResult *result, int argc, char *argv[])
{
    char sensmode[16];
    
    if (ProdInfoRead(FLASH_CFG_BLOCK, PIBCAT_SYSTEM, PIBITM_SENSMODE, sensmode, sizeof(sensmode)) != E_OK) {
        CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
        return; 
    }
    else {
        if (strcmp(sensmode, SENSOR_SHT7X_CFG_ID) == 0) {
            if (SensorSHT7xTest(strtoul(argv[1], NULL, 0) == 1) != OK) {
                CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
                return; 
            }                
        }
        else {
            CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
            return;                    
        }
    }

    CmdResultAddNoError(result, true);   
}


static void DoGetOperatorCall(TCmdResult *result, int argc, char *argv[])
{
    int     res = OK;
    bool    state = false;

#if defined(__ADSP_BRAEMAR__)       /* peripheral IDs for BF534, BF536, BF537 */
// NO GPIO left

#else
    res |= GPIOGet(GPIO_OPERATORCALL, &state);
    if (res != OK) {
        CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
        return;
    }
#endif

    CmdResultAddSTRING(result, state ? "Pressed" : "Released");
}

static void DoGetDoorswitch(TCmdResult *result, int argc, char *argv[])
{
    int     res = OK;
    bool    state = false;
#if defined(__ADSP_BRAEMAR__)       /* peripheral IDs for BF534, BF536, BF537 */
// NO GPIO left

#else
    res |= GPIOGet(GPIO_DOORSWITCH, &state);
    if (res != OK) {
        CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
        return;
    }
#endif    
    // When the state is true the connection is open
    // AIBo circuit inverts the signal'    
    CmdResultAddSTRING(result, state ? "Open" : "Closed");
}


static void DoSetGPIO(TCmdResult *result, int argc, char *argv[])
{
    int     res = OK;
    uint8_t value = ~(uint8_t)strtoul(argv[1], NULL, 0);

#if defined(__ADSP_BRAEMAR__)       /* peripheral IDs for BF534, BF536, BF537 */    
    // THIS CANNOT BE TESTED USING THE DEV BOARD
    value = ~value;
    res |= GPIOSet(GPIO_RESET_PER, (value >> 0) & 0x01);    
#else
    res |= GPIOSet(GPIO_X5_OUT_1, (value >> 0) & 0x01);
    res |= GPIOSet(GPIO_X5_OUT_2, (value >> 1) & 0x01);
    res |= GPIOSet(GPIO_X5_OUT_3, (value >> 2) & 0x01);
    res |= GPIOSet(GPIO_X5_OUT_4, (value >> 3) & 0x01);
    res |= GPIOSet(GPIO_X6_OUT_1, (value >> 4) & 0x01);
    res |= GPIOSet(GPIO_X6_OUT_2, (value >> 5) & 0x01);    
#endif

    if (res != OK) {
        CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
        return;
    }

    CmdResultAddNoError(result, true);
}

static void DoGetGPIO(TCmdResult *result, int argc, char *argv[])
{

    int     res = OK;
    bool    state;
    uint8_t value = 0;
    
#if defined(__ADSP_BRAEMAR__)       /* peripheral IDs for BF534, BF536, BF537 */    
    // THIS CANNOT BE TESTED USING THE DEV BOARD
    res |= GPIOGet(GPIO_RESET_PER, &state);
    value |= (state ? 1 : 0) << 0;
    res |= GPIOGet(GPIO_RESET_CFG, &state);
    value |= (state ? 1 : 0) << 1;
#else
    res |= GPIOGet(GPIO_X5_IN_1, &state);
    value |= (state ? 0 : 1) << 0;
    res |= GPIOGet(GPIO_X5_IN_2, &state);
    value |= (state ? 0 : 1) << 1;
    res |= GPIOGet(GPIO_X5_IN_3, &state);
    value |= (state ? 0 : 1) << 2;
    res |= GPIOGet(GPIO_X5_IN_4, &state);
    value |= (state ? 0 : 1) << 3;
    res |= GPIOGet(GPIO_X6_IN_1, &state);
    value |= (state ? 1 : 0) << 4;          // Isolated inputs are inverted
    res |= GPIOGet(GPIO_X6_IN_2, &state);   
    value |= (state ? 1 : 0) << 5;          // Isolated inputs are inverted
#endif

    if (res != OK) {
        CmdResultAddEXCEPTION(result, ERROR_EXCEPT, API_FAIL);        
        return;
    }
    
    CmdResultAddUINT8(result, value);
}




//! Table containing all available commands
static TCmds Cmds[] = {
    { "GetHumidity",	DoGetHumidity,    0,				0,					"",                       "In percent xx.x"                        },
    { "GetTemperature", DoGetTemperature, 0,				0,					"",                       "In degree centigrade xx.x"              },
    { "Testmode",       DoTestmode,       1,                1,                  "<on>",                   "Sensor testmode on/off"                 },
    { "GetOperatorCall",DoGetOperatorCall,0,				0,					"",                       "Get the Operator call status Open|Closed"  },    
    { "GetDoorswitch",	DoGetDoorswitch,  0,				0,					"",                       "Get the doorswitch status Open|Closed"  },    
    { "SetGPIO",		DoSetGPIO,        1,				1,					"<value>",                "Set the GPIO"                           },
    { "GetGPIO",		DoGetGPIO,        0,				0,					"",                       "Get the GPIO"                           },
    { NULL,				0,                0,				0,					NULL,                     NULL                                     }
};

bool APISensorsInit(TCmdResult *result)
{
    bool r = true;

    if (r) {
        r = CmdHdlAddException(result, ARGUMENT_EXCEPT);
    }

    if (r) {
        r = CmdHdlAddException(result, ERROR_EXCEPT);
    }

    if (r) {
        r = CmdHdlAddFolderCmdSet(result, "/Sens", Cmds);
    }
    return r;
}


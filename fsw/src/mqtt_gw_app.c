/*
** Copyright 2022 bitValence, Inc.
** All Rights Reserved.
**
** This program is free software; you can modify and/or redistribute it
** under the terms of the GNU Affero General Public License
** as published by the Free Software Foundation; version 3 with
** attribution addendums as found in the LICENSE.txt
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Affero General Public License for more details.
**
** Purpose:
**   Manage an MQTT gateway between the cFS Software Bus and an MQTT
**   Broker.
**
** Notes:
**   1. This file only performs app-level functions. MQTT_MGR controls
**      the MQTT gateway functionality.
**
** References:
**   1. OpenSatKit Object-based Application Developer's Guide
**   2. cFS Application Developer's Guide
**
*/

/*
** Includes
*/

#include <string.h>
#include "mqtt_gw_app.h"
#include "mqtt_topic_tbl.h"

/***********************/
/** Macro Definitions **/
/***********************/

/* Convenience macros */
#define  INITBL_OBJ      (&(MqttGw.IniTbl))
#define  CMDMGR_OBJ      (&(MqttGw.CmdMgr))
#define  TBLMGR_OBJ      (&(MqttGw.TblMgr))    
#define  CHILDMGR_OBJ    (&(MqttGw.ChildMgr))
#define  MQTT_MGR_OBJ    (&(MqttGw.MqttMgr))

/*******************************/
/** Local Function Prototypes **/
/*******************************/

static int32 InitApp(void);
static int32 ProcessCommands(void);
static void SendHousekeepingPkt(void);


/**********************/
/** File Global Data **/
/**********************/

/* 
** Must match DECLARE ENUM() declaration in app_cfg.h
** Defines "static INILIB_CfgEnum IniCfgEnum"
*/
DEFINE_ENUM(Config,APP_CONFIG)  

static CFE_EVS_BinFilter_t  EventFilters[] =
{  
   /* Event ID                 Mask */
   {MQTT_CLIENT_YIELD_ERR_EID, CFE_EVS_FIRST_4_STOP}

};

/*****************/
/** Global Data **/
/*****************/

MQTT_GW_Class_t   MqttGw;


/******************************************************************************
** Function: MQTT_GW_AppMain
**
*/
void MQTT_GW_AppMain(void)
{

   uint32 RunStatus = CFE_ES_RunStatus_APP_ERROR;
   
   CFE_EVS_Register(EventFilters, sizeof(EventFilters)/sizeof(CFE_EVS_BinFilter_t),
                    CFE_EVS_EventFilter_BINARY);

   if (InitApp() == CFE_SUCCESS)      /* Performs initial CFE_ES_PerfLogEntry() call */
   {
      RunStatus = CFE_ES_RunStatus_APP_RUN; 
   }
   
   /*
   ** Main process loop
   */
   while (CFE_ES_RunLoop(&RunStatus))
   {
      
      /*
      ** Since the app is data driven pend on SB topic
      ** and poll the command pipe at a lower rate
      */ 
      MQTT_MGR_ProcessSbTopics(MqttGw.PerfId);
      if (++MqttGw.PollCmdCnt > MqttGw.PollCmdInterval)
      {
          MqttGw.PollCmdCnt = 0;
          RunStatus = ProcessCommands();      
      } 
      
   } /* End CFE_ES_RunLoop */

   CFE_ES_WriteToSysLog("MQTT Gateway App terminating, run status = 0x%08X\n", RunStatus);   /* Use SysLog, events may not be working */

   CFE_EVS_SendEvent(MQTT_GW_EXIT_EID, CFE_EVS_EventType_CRITICAL, "MQTT Gateway App terminating, run status = 0x%08X", RunStatus);

   CFE_ES_ExitApp(RunStatus);  /* Let cFE kill the task (and any child tasks) */


} /* End of MQTT_GW_AppMain() */



/******************************************************************************
** Function: MQTT_GW_NoOpCmd
**
*/

bool MQTT_GW_NoOpCmd(void* ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   CFE_EVS_SendEvent (MQTT_GW_NOOP_EID, CFE_EVS_EventType_INFORMATION,
                      "No operation command received for MQTT Gateway App version %d.%d.%d",
                      MQTT_GW_MAJOR_VER, MQTT_GW_MINOR_VER, MQTT_GW_PLATFORM_REV);

   return true;


} /* End MQTT_GW_NoOpCmd() */


/******************************************************************************
** Function: MQTT_GW_ResetAppCmd
**
*/

bool MQTT_GW_ResetAppCmd(void* ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   CMDMGR_ResetStatus(CMDMGR_OBJ);
   TBLMGR_ResetStatus(TBLMGR_OBJ);
   CHILDMGR_ResetStatus(CHILDMGR_OBJ);
   
   MQTT_MGR_ResetStatus();
	  
   return true;

} /* End MQTT_GW_ResetAppCmd() */


/******************************************************************************
** Function: SendHousekeepingPkt
**
*/
void SendHousekeepingPkt(void)
{
   
   /* Good design practice in case app expands to more than one table */
   //const TBLMGR_Tbl_t* LastTbl = TBLMGR_GetLastTblStatus(TBLMGR_OBJ);

   /*
   ** Framework Data
   */

   MqttGw.HkTlm.Payload.ValidCmdCnt    = MqttGw.CmdMgr.ValidCmdCnt;
   MqttGw.HkTlm.Payload.InvalidCmdCnt  = MqttGw.CmdMgr.InvalidCmdCnt;

   MqttGw.HkTlm.Payload.ChildValidCmdCnt    = MqttGw.ChildMgr.ValidCmdCnt;
   MqttGw.HkTlm.Payload.ChildInvalidCmdCnt  = MqttGw.ChildMgr.InvalidCmdCnt;

   /*
   ** Table Data 
   ** - Loaded with status from the last table action 
   */

   //OskCDemo.HkPkt.LastTblAction       = LastTbl->LastAction;
   //OskCDemo.HkPkt.LastTblActionStatus = LastTbl->LastActionStatus;
   
   /*
   ** MQTT Data
   */

   CFE_SB_TimeStampMsg(CFE_MSG_PTR(MqttGw.HkTlm.TelemetryHeader));
   CFE_SB_TransmitMsg(CFE_MSG_PTR(MqttGw.HkTlm.TelemetryHeader), true);
      
} /* End SendHousekeepingPkt() */


/******************************************************************************
** Function: InitApp
**
*/
static int32 InitApp(void)
{

   int32 RetStatus = OSK_C_FW_CFS_ERROR;
   
   CHILDMGR_TaskInit_t ChildTaskInit;


   /*
   ** Read JSON INI Table & class variable defaults defined in JSON  
   */

   if (INITBL_Constructor(INITBL_OBJ, MQTT_GW_INI_FILENAME, &IniCfgEnum))
   {
   
      /* Pool for a command every 2 seconds */
      MqttGw.PollCmdInterval = 2000 / INITBL_GetIntConfig(INITBL_OBJ, CFG_TOPIC_PIPE_PEND);
      MqttGw.PollCmdCnt = 0;
      MqttGw.PerfId  = INITBL_GetIntConfig(INITBL_OBJ, CFG_APP_MAIN_PERF_ID);
      CFE_ES_PerfLogEntry(MqttGw.PerfId);

      MqttGw.CmdMid     = CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_CMD_MID));
      MqttGw.SendHkMid  = CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_SEND_HK_MID));
   
      RetStatus = CFE_SUCCESS;
   
   } /* End if INITBL Constructed */
   
   if (RetStatus == CFE_SUCCESS)
   {

      TBLMGR_Constructor(TBLMGR_OBJ);
      MQTT_MGR_Constructor(MQTT_MGR_OBJ, INITBL_OBJ, TBLMGR_OBJ);

      /* Child Manager constructor sends error events */

      ChildTaskInit.TaskName  = INITBL_GetStrConfig(INITBL_OBJ, CFG_CHILD_NAME);
      ChildTaskInit.StackSize = INITBL_GetIntConfig(INITBL_OBJ, CFG_CHILD_STACK_SIZE);
      ChildTaskInit.Priority  = INITBL_GetIntConfig(INITBL_OBJ, CFG_CHILD_PRIORITY);
      ChildTaskInit.PerfId    = INITBL_GetIntConfig(INITBL_OBJ, CFG_CHILD_TASK_PERF_ID);
      RetStatus = CHILDMGR_Constructor(CHILDMGR_OBJ, ChildMgr_TaskMainCallback,
                                       MQTT_MGR_ChildTaskCallback, &ChildTaskInit); 

      /*
      ** Initialize app level interfaces
      */
 
      CFE_SB_CreatePipe(&MqttGw.CmdPipe, INITBL_GetIntConfig(INITBL_OBJ, CFG_CMD_PIPE_DEPTH), INITBL_GetStrConfig(INITBL_OBJ, CFG_CMD_PIPE_NAME));  
      CFE_SB_Subscribe(MqttGw.CmdMid,    MqttGw.CmdPipe);
      CFE_SB_Subscribe(MqttGw.SendHkMid, MqttGw.CmdPipe);

      CMDMGR_Constructor(CMDMGR_OBJ);
      CMDMGR_RegisterFunc(CMDMGR_OBJ, CMDMGR_NOOP_CMD_FC,   NULL, MQTT_GW_NoOpCmd,     0);
      CMDMGR_RegisterFunc(CMDMGR_OBJ, CMDMGR_RESET_CMD_FC,  NULL, MQTT_GW_ResetAppCmd, 0);
 
      CFE_MSG_Init(CFE_MSG_PTR(MqttGw.HkTlm.TelemetryHeader), CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_HK_TLM_MID)), sizeof(MQTT_GW_HkTlm_t));

      /*
      ** Application startup event message
      */
      CFE_EVS_SendEvent(MQTT_GW_INIT_APP_EID, CFE_EVS_EventType_INFORMATION,
                        "MQTT Gateway App Initialized. Version %d.%d.%d",
                        MQTT_GW_MAJOR_VER, MQTT_GW_MINOR_VER, MQTT_GW_PLATFORM_REV);
                        
   } /* End if CHILDMGR constructed */
   
   return RetStatus;

} /* End of InitApp() */


/******************************************************************************
** Function: ProcessCommands
**
** 
*/
static int32 ProcessCommands(void)
{
   
   int32  RetStatus = CFE_ES_RunStatus_APP_RUN;
   int32  SysStatus;

   CFE_SB_Buffer_t  *SbBufPtr;
   CFE_SB_MsgId_t   MsgId = CFE_SB_INVALID_MSG_ID;


   CFE_ES_PerfLogExit(MqttGw.PerfId);
   SysStatus = CFE_SB_ReceiveBuffer(&SbBufPtr, MqttGw.CmdPipe, CFE_SB_POLL);
   CFE_ES_PerfLogEntry(MqttGw.PerfId);

   if (SysStatus == CFE_SUCCESS)
   {
      SysStatus = CFE_MSG_GetMsgId(&SbBufPtr->Msg, &MsgId);

      if (SysStatus == CFE_SUCCESS)
      {

         if (CFE_SB_MsgId_Equal(MsgId, MqttGw.CmdMid))
         {
            CMDMGR_DispatchFunc(CMDMGR_OBJ, &SbBufPtr->Msg);
         } 
         else if (CFE_SB_MsgId_Equal(MsgId, MqttGw.SendHkMid))
         {   
            SendHousekeepingPkt();
         }
         else
         {   
            CFE_EVS_SendEvent(MQTT_GW_INVALID_MID_EID, CFE_EVS_EventType_ERROR,
                              "Received invalid command packet, MID = 0x%08X", 
                              CFE_SB_MsgIdToValue(MsgId));
         }

      } /* End if got message ID */
   } /* End if received buffer */
   else
   {
      if (SysStatus != CFE_SB_NO_MESSAGE)
      {
         RetStatus = CFE_ES_RunStatus_APP_ERROR;
      }
   } 

   return RetStatus;
   
} /* End ProcessCommands() */

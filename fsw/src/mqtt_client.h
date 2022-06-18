/*
** Purpose: Define MQTT messages
**
** Notes:
**   1. Initial OSK MQTT App based on a January 2021 refactor of Alan Cudmore's
**      MQTT App https://github.com/alanc98/mqtt_app. 
**
** License:
**   Preserved original https://github.com/alanc98/mqtt_app Apache License 2.0.
**
** References:
**   1. OpenSatKit Object-based Application Developer's Guide
**   2. cFS Application Developer's Guide
*/
#ifndef _mqtt_client_
#define _mqtt_client


/*
** Includes
*/

#include "app_cfg.h"


/***********************/
/** Macro Definitions **/
/***********************/


/* 
** Events Message IDs
*/

#define MQTT_CLIENT_CONSTRUCT_EID      (MQTT_CLIENT_BASE_EID + 0)
#define MQTT_CLIENT_CONSTRUCT_ERR_EID  (MQTT_CLIENT_BASE_EID + 1)
#define MQTT_CLIENT_CONNECT_EID        (MQTT_CLIENT_BASE_EID + 2)
#define MQTT_CLIENT_CONNECT_ERR_EID    (MQTT_CLIENT_BASE_EID + 3)
#define MQTT_CLIENT_YIELD_ERR_EID      (MQTT_CLIENT_BASE_EID + 4)


/**********************/
/** Type Definitions **/
/**********************/


/* 
** Quality of Service
*/

typedef enum
{

   MQTT_CLIENT_QOS0 = QOS0,
   MQTT_CLIENT_QOS1 = QOS1,
   MQTT_CLIENT_QOS2 = QOS2

} MQTT_CLIENT_Qos_t; 


/*
** Process message function callback signature 
*/

typedef MessageData MQTT_CLIENT_MsgData_t; /* Redefined tio isolate/contain mqtt library dependencies */ 

typedef void (*MQTT_CLIENT_MsgCallback_t) (MQTT_CLIENT_MsgData_t *MsgData);

/*
** Class Definition
*/

typedef struct
{

   bool    Connected;
   
   MQTTMessage PubMsg;
   
   /*
   ** MQTT Library
   */
   
   Network                 Network;
   MQTTClient              Client;
   MQTTPacket_connectData  ConnectData;    
   unsigned char           SendBuf[MQTT_CLIENT_SEND_BUF_LEN];
   unsigned char           ReadBuf[MQTT_CLIENT_READ_BUF_LEN];

} MQTT_CLIENT_Class_t;


/************************/
/** Exported Functions **/
/************************/


/******************************************************************************
** Function: MQTT_CLIENT_Constructor
**
** Notes:
**   1. This function must be called prior to any other functions being
**      called using the same cmdmgr instance.
*/
void MQTT_CLIENT_Constructor(MQTT_CLIENT_Class_t *MqttClientPtr,
                             const INITBL_Class_t *IniTbl);


/******************************************************************************
** Function: MQTT_CLIENT_ResetStatus
**
** Reset counters and status flags to a known reset state.
**
*/
void MQTT_CLIENT_ResetStatus(void);


/******************************************************************************
** Function: MQTT_CLIENT_Publish
**
** Notes:
**    1. QOS needs to be converted to MQTT library constants
*/
bool MQTT_CLIENT_Publish(const char *Topic, const char *Payload);


/******************************************************************************
** Function: MQTT_CLIENT_Subscribe
**
** Notes:
**    1. QOS options are defined in mqtt_msg.h
*/
bool MQTT_CLIENT_Subscribe(const char *Topic, int Qos, 
                           MQTT_CLIENT_MsgCallback_t MsgCallbackFunc);


/******************************************************************************
** Function: MQTT_CLIENT_Yield
**
** Notes:
**    1. A task delay will always occur regardless of MQTT interface behavior
**       to avoid CPU hogging
**
*/
bool MQTT_CLIENT_Yield(uint32 YieldTime);


/******************************************************************************
** Function: MQTT_CLIENT_Connect
**
** Notes:
**    None
**
*/
bool MQTT_CLIENT_Connect(const char *ClientName,
                         const char *BrokerAddress, uint32 BrokerPort);


/******************************************************************************
** Function: MQTT_CLIENT_Disconnect
**
** Notes:
**    None
**
*/
void MQTT_CLIENT_Disconnect(void);


#endif /* _mqtt_client_ */


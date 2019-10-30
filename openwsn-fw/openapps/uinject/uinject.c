#include "opendefs.h"
#include "uinject.h"
#include "openudp.h"
#include "openqueue.h"
#include "opentimers.h"
#include "openserial.h"
#include "packetfunctions.h"
#include "scheduler.h"
#include "board.h"

//=========================== variables =======================================

uinject_vars_t uinject_vars;

/*static const uint8_t uinject_dst_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
}; */
// unicast address added by erfan!!
static const uint8_t uinject_dst_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x14, 0x15, 0x92, 0xcc, 0x00, 0x00, 0x00, 0x01
};

//=========================== prototypes ======================================

void uinject_timer_cb(opentimer_id_t id);
void uinject_task_cb(void);

//=========================== public ==========================================

void uinject_init() {
   
   // clear local variables
   memset(&uinject_vars,0,sizeof(uinject_vars_t));
   
   // start periodic timer
   uinject_vars.timerId                    = opentimers_start(
      UINJECT_PERIOD_MS,
      TIMER_PERIODIC,TIME_MS,
      uinject_timer_cb
   );
   
}

void uinject_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

void uinject_receive(OpenQueueEntry_t* pkt) {
   
   openqueue_freePacketBuffer(pkt);
   
   openserial_printError(
      COMPONENT_UINJECT,
      ERR_RCVD_ECHO_REPLY,
      (errorparameter_t)0,
      (errorparameter_t)0
   );
}

//=========================== private =========================================

/**
\note timer fired, but we don't want to execute task in ISR mode instead, push
   task to scheduler with CoAP priority, and let scheduler take care of it.
*/
void uinject_timer_cb(opentimer_id_t id){
   
   scheduler_push_task(uinject_task_cb,TASKPRIO_COAP);
}

void uinject_task_cb() {
   OpenQueueEntry_t*    pkt;
   open_addr_t *        p;
   
   uint16_t pktID;                                 
   uint8_t  quotient_pktID;
   uint8_t  remainder_pktID;
   uint8_t  moteID;
   // don't run if not synch
   if (ieee154e_isSynch() == FALSE) return;
   
   // don't run on dagroot
   if (idmanager_getIsDAGroot()) {
      opentimers_stop(uinject_vars.timerId);
      return;
   }
   
   // if you get here, send a packet
   
   // get a free packet buffer
   pkt = openqueue_getFreePacketBuffer(COMPONENT_UINJECT);
   if (pkt==NULL) {
      openserial_printError(
         COMPONENT_UINJECT,
         ERR_NO_FREE_PACKET_BUFFER,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   pkt->owner                         = COMPONENT_UINJECT;
   pkt->creator                       = COMPONENT_UINJECT;
   pkt->l4_protocol                   = IANA_UDP;
   pkt->l4_destination_port           = WKP_UDP_INJECT;
   pkt->l4_sourcePortORicmpv6Type     = WKP_UDP_INJECT;
   pkt->l3_destinationAdd.type        = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],uinject_dst_addr,16);
   
   /*
   packetfunctions_reserveHeaderSize(pkt,sizeof(uint16_t));
   *((uint16_t*)&pkt->payload[0]) = uinject_vars.counter++;
*/
   // add time
   struct tm      *t;
   struct timeval  tp;
   gettimeofday(&tp, 0);
   time_t curtime = tp.tv_sec;
   t = localtime(&curtime);

   p=idmanager_getMyID(ADDR_64B);
  
   pktID=++DataPacketID[p->addr_64b[7]-2];
   moteID=p->addr_64b[7];

   quotient_pktID = pktID/255;
   remainder_pktID = pktID%255;
   packetfunctions_reserveHeaderSize(pkt,4);
   pkt->payload[0] = quotient_pktID;
   pkt->payload[1] = remainder_pktID;
   pkt->payload[2] = 20;
   pkt->payload[3] = 200;
   //pkt-.payload[4] = 20;
   packetfunctions_reserveHeaderSize(pkt,sizeof(uint8_t));
   pkt->payload[0] = moteID;

   packetfunctions_reserveHeaderSize(pkt,10);

   /*
   // to check the max size of payload. if we add 27B it collapses, but for 26B works correctly.
   // So our packet size is 101B.
   uint8_t z;
   for (z=0;z<26;z++) {
       pkt->payload[z] = 11;
   }
   */

   FILE *GetPDRandDELAY;
   GetPDRandDELAY =fopen("_Log_UINJECTApp_Sndr.txt","a");
   
   if (GetPDRandDELAY != NULL) {
           fprintf(GetPDRandDELAY, "%d %02d %02d %02d %0d %d\n", p->addr_64b[7], t->tm_hour, t->tm_min, t->tm_sec, tp.tv_usec/1000, pktID);
           // reuse of pktID variable !!
           for (pktID=0;pktID<130;pktID++) {
              fprintf(GetPDRandDELAY, "%d ", pkt->packet[pktID]);// pkt's data
           }
           fprintf(GetPDRandDELAY, "\n");
   }
   fclose(GetPDRandDELAY);

   if ((openudp_send(pkt))==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }
}











/*
   packetfunctions_reserveHeaderSize(pkt,sizeof(uint8_t));
   *((uint8_t*)pkt->payload) = quotient_pktID;

   packetfunctions_reserveHeaderSize(pkt,sizeof(uint8_t));
   *((uint8_t*)pkt->payload) = remainder_pktID;

   packetfunctions_reserveHeaderSize(pkt,sizeof(uint8_t));
   *((uint8_t*)pkt->payload) = 20;
   
   packetfunctions_reserveHeaderSize(pkt,sizeof(uint8_t));
   *((uint8_t*)pkt->payload) = 200;

   packetfunctions_reserveHeaderSize(pkt,sizeof(uint8_t));
   *((uint8_t*)pkt->payload) = moteID;
*/

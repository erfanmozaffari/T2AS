#include "opendefs.h"
#include "udpconvergecast.h"
#include "opentimers.h"
#include "openudp.h"
#include "openqueue.h"
#include "packetfunctions.h"
#include "openserial.h"
#include "openrandom.h"
#include "scheduler.h"
#include "IEEE802154E.h"
#include "idmanager.h"
#include "time.h"
#include "sys/timeb.h"
//=========================== defines =========================================

// unicast address added by erfan!!
static const uint8_t dst_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x14, 0x15, 0x92, 0xcc, 0x00, 0x00, 0x00, 0x01
};

//=========================== variables =======================================

udpconvergecast_vars_t udpconvergecast_vars;

//=========================== prototypes ======================================

void udpconvergecast_timer_cb(void);
void udpconvergecast_task_cb(void);
void udpconvergecast_sendDone(OpenQueueEntry_t* msg, owerror_t error);

//=========================== public ==========================================

void udpconvergecast_init(void) {

   //start a periodic timer
   udpconvergecast_vars.period = 4000;

   udpconvergecast_vars.timerId    	= opentimers_start(udpconvergecast_vars.period,
                                           	   TIMER_PERIODIC,TIME_MS,
                                           	   udpconvergecast_timer_cb);
   //stop 
   //opentimers_stop(udpconvergecast_vars.timerId);
}

//=========================== private =========================================

/**
\note timer fired, but we don't want to execute task in ISR mode instead, push
   task to scheduler with CoAP priority, and let scheduler take care of it.
*/
void udpconvergecast_timer_cb(){
   scheduler_push_task(udpconvergecast_task_cb,TASKPRIO_COAP);
}

void udpconvergecast_task_cb() {

   OpenQueueEntry_t*	pkt;
   open_addr_t *     	p;
   open_addr_t       	q;

   // don't run if not synch
   if (ieee154e_isSynch() == FALSE) return;
   
   // don't run on dagroot
   if (idmanager_getIsDAGroot()) {
      opentimers_stop(udpconvergecast_vars.timerId);
      return;
   }
   
   if(udpconvergecast_vars.period == 0) {
      // stop the periodic timer
      opentimers_stop(udpconvergecast_vars.timerId);
      return;
   }

   //prepare packet
   pkt = openqueue_getFreePacketBuffer(COMPONENT_UDPCONVERGECAST);
   if (pkt==NULL) {
      openserial_printError(COMPONENT_UDPCONVERGECAST,ERR_NO_FREE_PACKET_BUFFER,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
      return;
   }
   pkt->creator                     = COMPONENT_UDPCONVERGECAST;
   pkt->owner                       = COMPONENT_UDPCONVERGECAST;
   pkt->l4_protocol                 = IANA_UDP;
   pkt->l4_sourcePortORicmpv6Type   = WKP_UDP_CONVERGECAST;
   pkt->l4_destination_port         = WKP_UDP_CONVERGECAST;
   pkt->l3_destinationAdd.type      = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],&dst_addr,16);
  
   // add time
   struct tm      *t;
   struct timeval tp;
   gettimeofday(&tp, 0);
   time_t curtime = tp.tv_sec;
   t = localtime(&curtime);
/*   
   int ms = tp.tv_usec/1000;

   packetfunctions_reserveHeaderSize(pkt,10);
   pkt->payload[0]=(uint8_t)t->tm_hour;
   pkt->payload[1]=(uint8_t)t->tm_min;
   pkt->payload[2]=(uint8_t)t->tm_sec;
   if( ms <= 255){
       pkt->payload[3]=ms;
       pkt->payload[4]=0;
       pkt->payload[5]=0;
       pkt->payload[6]=0;
   }
   else if( 255 < ms && ms <= 510){
       pkt->payload[3]=255;
       pkt->payload[4]=ms - 255;
       pkt->payload[5]=0;
       pkt->payload[6]=0;
   }
   else if( ms > 510 && ms <= 765){
       pkt->payload[3]=255;
       pkt->payload[4]=255;
       pkt->payload[5]=ms - 510;
       pkt->payload[6]=0;
   }
   else {
       pkt->payload[3]=255;
       pkt->payload[4]=255;
       pkt->payload[5]=255;
       pkt->payload[6]=ms - 765;
   }

   pkt->payload[7]= 20;
   pkt->payload[8]= 200;
   pkt->payload[9]= 20;
*/
   p=idmanager_getMyID(ADDR_64B);
   DataPacketID[p->addr_64b[7]-2]+=1;
   uint16_t pktID;
   uint16_t tmpID;
   pktID=DataPacketID[p->addr_64b[7]-2];
   tmpID=pktID;

   uint8_t counter;
   counter=0;
   while(pktID>255) {
       counter++;
       pktID-=255;
   }
   packetfunctions_reserveHeaderSize(pkt,5);
   pkt->payload[0]= counter;
   pkt->payload[1]= pktID;
   pkt->payload[2]= 20;
   pkt->payload[3]= 200;
   pkt->payload[4]= 20;


   packetfunctions_reserveHeaderSize(pkt,8);
   pkt->payload[0]=p->addr_64b[0];
   pkt->payload[1]=p->addr_64b[1];
   pkt->payload[2]=p->addr_64b[2];
   pkt->payload[3]=p->addr_64b[3];
   pkt->payload[4]=p->addr_64b[4];
   pkt->payload[5]=p->addr_64b[5];
   pkt->payload[6]=p->addr_64b[6];
   pkt->payload[7]=p->addr_64b[7];

  /* 
   neighbors_getPreferredParentEui64(&q);
   if (q.type==ADDR_64B){
      packetfunctions_reserveHeaderSize(pkt,8);
      //copy my preferred parent so we can build the topology
      pkt->payload[0]=q.addr_64b[0];
      pkt->payload[1]=q.addr_64b[1];
      pkt->payload[2]=q.addr_64b[2];
      pkt->payload[3]=q.addr_64b[3];
      pkt->payload[4]=q.addr_64b[4];
      pkt->payload[5]=q.addr_64b[5];
      pkt->payload[6]=q.addr_64b[6];
      pkt->payload[7]=q.addr_64b[7];
   }
*/
   open_addr_t*		 myadd64;
   myadd64       = idmanager_getMyID(ADDR_64B);

   FILE *GetPDRandDELAY;
   GetPDRandDELAY =fopen("_Log_UdpConvergecastApp_Sndr.txt","a");
   
   if(GetPDRandDELAY != NULL){
           fprintf(GetPDRandDELAY, "%d %02d %02d %02d %0d %d\n", myadd64->addr_64b[7], t->tm_hour, t->tm_min, t->tm_sec, tp.tv_usec/1000, tmpID);
           for(tmpID=0;tmpID<130;tmpID++)
              fprintf(GetPDRandDELAY, "%d ", pkt->packet[tmpID]);// pkt's data
           fprintf(GetPDRandDELAY, "\n");
   }
   fclose(GetPDRandDELAY);
   
   //send packet
   if ((openudp_send(pkt))==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }

}

void udpconvergecast_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}
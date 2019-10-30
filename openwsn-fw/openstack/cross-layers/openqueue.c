#include "opendefs.h"
#include "openqueue.h"
#include "openserial.h"
#include "packetfunctions.h"
#include "IEEE802154E.h"

//=========================== variables =======================================

openqueue_vars_t openqueue_vars;

//=========================== prototypes ======================================

void openqueue_reset_entry(OpenQueueEntry_t* entry);

//=========================== public ==========================================

//======= admin

/**
\brief Initialize this module.
*/
void openqueue_init() {
   uint8_t i;
   for (i=0;i<QUEUELENGTH;i++){
      openqueue_reset_entry(&(openqueue_vars.queue[i]));
   }
}

/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_queue() {
   debugOpenQueueEntry_t output[QUEUELENGTH];
   uint8_t i;
   for (i=0;i<QUEUELENGTH;i++) {
      output[i].creator = openqueue_vars.queue[i].creator;
      output[i].owner   = openqueue_vars.queue[i].owner;
   }
   openserial_printStatus(STATUS_QUEUE,(uint8_t*)&output,QUEUELENGTH*sizeof(debugOpenQueueEntry_t));
   return TRUE;
}

//======= called by any component

/**
\brief Request a new (free) packet buffer.

Component throughout the protocol stack can call this function is they want to
get a new packet buffer to start creating a new packet.

\note Once a packet has been allocated, it is up to the creator of the packet
      to free it using the openqueue_freePacketBuffer() function.

\returns A pointer to the queue entry when it could be allocated, or NULL when
         it could not be allocated (buffer full or not synchronized).
*/
OpenQueueEntry_t* openqueue_getFreePacketBuffer(uint8_t creator) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   // refuse to allocate if we're not in sync
   if (ieee154e_isSynch()==FALSE && creator > COMPONENT_IEEE802154E){
     ENABLE_INTERRUPTS();
     return NULL;
   }
   
   // if you get here, I will try to allocate a buffer for you
   
   // walk through queue and find free entry
   for (i=0;i<QUEUELENGTH;i++) {
      if (openqueue_vars.queue[i].owner==COMPONENT_NULL) {
         openqueue_vars.queue[i].creator=creator;
         openqueue_vars.queue[i].owner=COMPONENT_OPENQUEUE;
         ENABLE_INTERRUPTS(); 
         return &openqueue_vars.queue[i];
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}


/**
\brief Free a previously-allocated packet buffer.

\param pkt A pointer to the previsouly-allocated packet buffer.

\returns E_SUCCESS when the freeing was succeful.
\returns E_FAIL when the module could not find the specified packet buffer.
*/
owerror_t openqueue_freePacketBuffer(OpenQueueEntry_t* pkt) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++) {
      if (&openqueue_vars.queue[i]==pkt) {
         if (openqueue_vars.queue[i].owner==COMPONENT_NULL) {
            // log the error
            openserial_printCritical(COMPONENT_OPENQUEUE,ERR_FREEING_UNUSED,
                                  (errorparameter_t)0,
                                  (errorparameter_t)0);
         }
         openqueue_reset_entry(&(openqueue_vars.queue[i]));
         ENABLE_INTERRUPTS();
         return E_SUCCESS;
      }
   }
   // log the error
   openserial_printCritical(COMPONENT_OPENQUEUE,ERR_FREEING_ERROR,
                         (errorparameter_t)0,
                         (errorparameter_t)0);
   ENABLE_INTERRUPTS();
   return E_FAIL;
}

/**
\brief Free all the packet buffers created by a specific module.

\param creator The identifier of the component, taken in COMPONENT_*.
*/
void openqueue_removeAllCreatedBy(uint8_t creator) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++){
      if (openqueue_vars.queue[i].creator==creator) {
         openqueue_reset_entry(&(openqueue_vars.queue[i]));
      }
   }
   ENABLE_INTERRUPTS();
}

/**
\brief Free all the packet buffers owned by a specific module.

\param owner The identifier of the component, taken in COMPONENT_*.
*/
void openqueue_removeAllOwnedBy(uint8_t owner) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++){
      if (openqueue_vars.queue[i].owner==owner) {
         openqueue_reset_entry(&(openqueue_vars.queue[i]));
      }
   }
   ENABLE_INTERRUPTS();
}

//======= called by RES

OpenQueueEntry_t* openqueue_sixtopGetSentPacket() {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++) {
      if (openqueue_vars.queue[i].owner==COMPONENT_IEEE802154E_TO_SIXTOP &&
          openqueue_vars.queue[i].creator!=COMPONENT_IEEE802154E) {
         ENABLE_INTERRUPTS();
         return &openqueue_vars.queue[i];
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}

OpenQueueEntry_t* openqueue_sixtopGetReceivedPacket() {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++) {
      if (openqueue_vars.queue[i].owner==COMPONENT_IEEE802154E_TO_SIXTOP &&
          openqueue_vars.queue[i].creator==COMPONENT_IEEE802154E) {
         ENABLE_INTERRUPTS();
         return &openqueue_vars.queue[i];
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}

//======= called by IEEE80215E

OpenQueueEntry_t* openqueue_macGetDataPacket(open_addr_t* toNeighbor) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   if (toNeighbor->type==ADDR_64B) {
      // added by erfan to log queue size
      uint8_t queueSize = QUEUELENGTH;
      for (i=0;i<QUEUELENGTH;i++) {
          if (openqueue_vars.queue[i].owner==COMPONENT_NULL) {
             queueSize-=1;
          }
      }

      struct tm * t;
      struct timeval tp;
      gettimeofday(&tp, 0);
      time_t curtime = tp.tv_sec;
      t = localtime(&curtime);

      FILE *queueHandler;
      queueHandler =fopen("_Log_QSize.txt","a");
      open_addr_t *        p;
      p=idmanager_getMyID(ADDR_64B);

      if(queueHandler != NULL){                           // current mote       // time of received pkts                           
          fprintf(queueHandler, "%d %02d %02d %02d %d %d\n", p->addr_64b[7], t->tm_hour, t->tm_min, t->tm_sec, tp.tv_usec/1000, queueSize);
      }
      fclose(queueHandler);

      // ended by erfan


      // a neighbor is specified, look for a packet unicast to that neigbhbor
      for (i=0;i<QUEUELENGTH;i++) {                                               // added by erfan to allow only udpconvergecast packets pass
          if (openqueue_vars.queue[i].owner==COMPONENT_SIXTOP_TO_IEEE802154E && openqueue_vars.queue[i].l4_protocol == IANA_UDP &&
              packetfunctions_sameAddress(toNeighbor,&openqueue_vars.queue[i].l2_nextORpreviousHop)) {
              ENABLE_INTERRUPTS();

              // added by erfan to get log of sent pkts
              if (openqueue_vars.queue[i].creator!=COMPONENT_FORWARDING) {
                  struct tm * t;
                  struct timeval tp;
                  gettimeofday(&tp, 0);
                  time_t curtime = tp.tv_sec;
                  t = localtime(&curtime);
                  uint8_t j;
                  FILE *DelayProblemSnt;
                  DelayProblemSnt =fopen("_Log_OpenQueue_Snt.txt","a");
                  open_addr_t *        p;
                  p=idmanager_getMyID(ADDR_64B);
                  if(DelayProblemSnt != NULL){                           // current mote       // time of received pkts                           
                      fprintf(DelayProblemSnt, "%d %02d %02d %02d %d %d\n", p->addr_64b[7], t->tm_hour, t->tm_min, t->tm_sec, tp.tv_usec/1000, queueSize);
                      for (j=0;j<130;j++) {
                           fprintf(DelayProblemSnt, "%d ", openqueue_vars.queue[i].packet[j]);// pkt's data
                      }
                      fprintf(DelayProblemSnt, "\n");
                  }
                  fclose(DelayProblemSnt);
              }
	      // get only forwarding data packets to get delay of them
	      else if (openqueue_vars.queue[i].creator==COMPONENT_FORWARDING){
                  struct tm * t;
                  struct timeval tp;
                  gettimeofday(&tp, 0);
                  time_t curtime = tp.tv_sec;
                  t = localtime(&curtime);
                  uint8_t j;
                  FILE *DelayProblemSnt;
                  DelayProblemSnt =fopen("_Log_forwardingQueue.txt","a");
                  open_addr_t *        p;
                  p=idmanager_getMyID(ADDR_64B);
                  if(DelayProblemSnt != NULL){                           // current mote       // time of received pkts                           
                      fprintf(DelayProblemSnt, "%d %02d %02d %02d %d %d %d\n", p->addr_64b[7], t->tm_hour, t->tm_min, t->tm_sec, tp.tv_usec/1000, openqueue_vars.queue[i].l3_sourceAdd.addr_128b[15], openqueue_vars.queue[i].l3_destinationAdd.addr_128b[15]);
                      for (j=0;j<130;j++) {
                           fprintf(DelayProblemSnt, "%d ", openqueue_vars.queue[i].packet[j]);// pkt's data
                      }
                      fprintf(DelayProblemSnt, "\n");
                  }
                  fclose(DelayProblemSnt);
              }

              // end of erfan changes
              
	      	
              return &openqueue_vars.queue[i];
          }
      }
   } else if (toNeighbor->type==ADDR_ANYCAST) {
      // anycast case: look for a packet which is either not created by RES
      // or an KA (created by RES, but not broadcast)
      for (i=0;i<QUEUELENGTH;i++) {                                            // bellow part added by erfan to block udpconvergecast packets
         if (openqueue_vars.queue[i].owner==COMPONENT_SIXTOP_TO_IEEE802154E && openqueue_vars.queue[i].creator!=COMPONENT_UINJECT &&
             ( openqueue_vars.queue[i].creator!=COMPONENT_SIXTOP ||
                (
                   openqueue_vars.queue[i].creator==COMPONENT_SIXTOP &&
                   packetfunctions_isBroadcastMulticast(&(openqueue_vars.queue[i].l2_nextORpreviousHop))==FALSE
                )
             )
            ) {
            // added by erfan to block udpconvergecast packets
            if (openqueue_vars.queue[i].creator==COMPONENT_FORWARDING && openqueue_vars.queue[i].l4_protocol == IANA_UDP) {
                 break;
            }
            // end of erfan
            ENABLE_INTERRUPTS();
            return &openqueue_vars.queue[i];
         }
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}

OpenQueueEntry_t* openqueue_macGetEBPacket() {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++) {
      if (openqueue_vars.queue[i].owner==COMPONENT_SIXTOP_TO_IEEE802154E &&
          openqueue_vars.queue[i].creator==COMPONENT_SIXTOP              &&
          packetfunctions_isBroadcastMulticast(&(openqueue_vars.queue[i].l2_nextORpreviousHop))) {
         ENABLE_INTERRUPTS();
         return &openqueue_vars.queue[i];
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}

//=========================== private =========================================

void openqueue_reset_entry(OpenQueueEntry_t* entry) {
   //admin
   entry->creator                      = COMPONENT_NULL;
   entry->owner                        = COMPONENT_NULL;
   entry->payload                      = &(entry->packet[127]);
   entry->length                       = 0;
   //l4
   entry->l4_protocol                  = IANA_UNDEFINED;
   //l3
   entry->l3_destinationAdd.type       = ADDR_NONE;
   entry->l3_sourceAdd.type            = ADDR_NONE;
   //l2
   entry->l2_nextORpreviousHop.type    = ADDR_NONE;
   entry->l2_frameType                 = IEEE154_TYPE_UNDEFINED;
   entry->l2_retriesLeft               = 0;
   entry->l2_IEListPresent             = 0;
}

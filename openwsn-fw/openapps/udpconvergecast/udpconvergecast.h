#ifndef __UDPCONVERGECAST_H
#define __UDPCONVERGECAST_H

/**
\addtogroup AppUdp
\{
\addtogroup udpconvergecast
\{
*/

#include "openudp.h"

//=========================== define ==========================================

#define NUMPKTTEST           1000

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
   opentimer_id_t  	timerId;
   uint16_t         period;   ///< inter-packet period (in ms)
} udpconvergecast_vars_t;

//=========================== prototypes ======================================

void udpconvergecast_init(void);

/**
\}
\}
*/

#endif

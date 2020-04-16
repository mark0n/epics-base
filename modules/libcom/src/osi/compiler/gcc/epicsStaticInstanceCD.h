
/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef epicsStaticInstanceCD_h
#define epicsStaticInstanceCD_h

#if ( __GNUC__ * 100 + __GNUC_MINOR__ ) >= 401
    // any gcc version that has -fno-threadsafe-statics
    // should be ok
#   include "epicsStaticInstanceSaneCmplr.h"
#else // ( __GNUC__ * 100 + __GNUC_MINOR__ ) >= 401
    // other gcc maybe cant be trusted without more testing
#   include "epicsStaticInstanceSketchyCmplr.h"
#endif // ( __GNUC__ * 100 + __GNUC_MINOR__ ) >= 401

#endif // epicsStaticInstanceCD_h


/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author: Jeffrey O. Hill
 */

#ifndef epicsDemangleh
#define epicsDemangleh

#include <string>
#include <typeinfo>

#include "shareLib.h"

epicsShareFunc std :: string 
        epicsDemangle ( const char * const pMangledName );
epicsShareFunc std :: string 
        epicsDemangleTypeName ( const std :: type_info & );

#endif /* epicsDemangleh */

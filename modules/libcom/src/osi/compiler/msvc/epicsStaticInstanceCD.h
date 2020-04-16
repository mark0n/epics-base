
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

// visual c++ has some issues 
// --------------------------
//
// 1) it lacks mutual exclusion protecting concurrent on demand
// initialization of block scope static variables
//
// 2) it silently adds the path to the .cpp file to the very beginning
// of the include search list so if a msvc specific version of this
// file does not exist and we use the default version then it will 
// also use compiler/default/ version, despite the existence 
// of a more specialized version, and even if not told to do so on 
// the command line
//
#   include "epicsStaticInstanceSketchyCmplr.h"

#endif epicsStaticInstanceCD_h


/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author: Jeffrey O. Hill
 */
#include <exception>
#include <cstdlib>

#define epicsExportSharedSymbols
#include "epicsDemangle.h"

using std :: string;

std :: string epicsDemangle ( const char * const pMangledName )
{
    return std :: string ( pMangledName );
}

std :: string epicsDemangleTypeName ( const std :: type_info & ti )
{
    return epicsDemangle ( ti.name () );
}

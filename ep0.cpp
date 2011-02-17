/***************************************************************************
 *   Copyright (C) 2011 by Terraneo Federico                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "ep0.h"
#include "def_ctrl_pipe.h"

#ifdef _MIOSIX
#include "kernel/kernel.h"
using namespace miosix;
#endif //_MIOSIX

namespace mxusb {

/**
 * \internal
 * This class is the default implementation for endpoint zero callbacks.
 * It STALLs every class or vendor specific request.
 */
class DefaultEndpointZeroCallbacks : public EndpointZeroCallbacks
{
public:
    virtual bool IRQsetup(const Setup* setup) { return false; }
};

//Default callbacks
static DefaultEndpointZeroCallbacks defaultCallbacks;

//
// class EndpointZeroCallbacks
//

bool EndpointZeroCallbacks::IRQendOfOutDataStage(const Setup *setup)
{
    return false;
}

EndpointZeroCallbacks::~EndpointZeroCallbacks() {}

void EndpointZeroCallbacks::IRQsetDataBuffer(unsigned char *data)
{
    DefCtrlPipe::IRQsetDataForCustomRequest(data);
}

void EndpointZeroCallbacks::setCallbacks(EndpointZeroCallbacks * callback)
{
    #ifdef _MIOSIX
    InterruptDisableLock dLock;
    #else //_MIOSIX
    __disable_irq();
    #endif //_MIOSIX

    if(callback==0) callbacks=&defaultCallbacks;
    else callbacks=callback;

    #ifndef _MIOSIX
    __enable_irq();
    #endif //_MIOSIX
}

EndpointZeroCallbacks *EndpointZeroCallbacks::callbacks=&defaultCallbacks;

} //namespace mxusb

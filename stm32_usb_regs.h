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

#ifndef MXUSB_LIBRARY
#error "This is header is private, it can be used only within mxusb."
#error "If your code depends on a private header, it IS broken."
#endif //MXUSB_LIBRARY

#include "endpoint_reg.h"

#ifndef STM32_USB_REGS_H
#define	STM32_USB_REGS_H

namespace mxusb {

/// \internal
/// Number of hardware endpoints of the stm32
const int NUM_ENDPOINTS=8;

/*
 * \internal
 * Can you believe it? stm32f10x.h despite being nearly 8000 lines long doesn't
 * have the memory layout for the USB peripheral...
 */
struct USBmemoryLayout
{
    //These hardware registers are encapsulated in the Endpoint class
    EndpointRegister endpoint[NUM_ENDPOINTS];
    char reserved0[32];
    volatile unsigned short CNTR;
    short reserved1;
    volatile unsigned short ISTR;
    short reserved2;
    volatile unsigned short FNR;
    short reserved3;
    volatile unsigned short DADDR;
    short reserved4;
    volatile unsigned short BTABLE;
};

/**
 * \internal
 * Pointer that maps the USBmemoryLayout to the peripheral address in memory
 */
USBmemoryLayout* const USB=reinterpret_cast<USBmemoryLayout*>(0x40005c00);

} //namespace mxusb

#endif //STM32_USB_REGS_H

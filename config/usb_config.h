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

#ifndef USB_CONFIG_H
#define	USB_CONFIG_H

#define USB_CONFIG_VERSION 100

/**
 * \file usb_config.h
 * This file contains configuration parameters for the mxusb library.
 */

namespace mxusb {

//
// configuration parameters for the mxgui library
//

/// Size of buffer for endpoint zero. This constant is put here because it
/// must be accessible also from user code, to fill in the value in the device
/// descriptor. The USB standard specifies that only 8,16,32,64 are valid values
const unsigned short EP0_SIZE=32;

/// Enable descriptor validation.<br>
/// While developing, this should be kept enabled so that if an error is
/// introduced in the descriptors, the USB stack will refuse to start and
/// print an error message. When releasing code, to minimize
/// code size it can be disabled.
//#define MXUSB_ENABLE_DESC_VALIDATION

/// Enable trace mode.<br>
/// This spawns a background thread which prints debug data.<br>
/// Since data is printed in a thread, the time needed to print does not cause
/// interrupt latency.
//#define MXUSB_ENABLE_TRACE

/// Enable printing also TracePoints tagged as verbose.<br>
/// Useful for debugging enumeration issues.
//#define MXUSB_PRINT_VERBOSE

/// Size of buffer used to move data from the interrupt routine to the
/// printing thread.
const unsigned int QUEUE_SIZE=1024;

} //namespace mxusb

#endif //USB_CONFIG_H

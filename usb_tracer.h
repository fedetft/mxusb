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

#include "usb_config.h"

#ifdef MXUSB_ENABLE_TRACE
#include "miosix.h"
#endif //MXUSB_ENABLE_TRACE

#ifndef USB_TRACER_H
#define	USB_TRACER_H

namespace mxusb {

/**
 * \internal
 * Class only meant to wrap the TracePoint enum to prevent namespace pollution
 */
class Ut
{
public:
    ///Verbose mode. Some of the TracePoints are tagged with this.
    ///It allows to select printing everything or only non verbose data
    static const unsigned char VERBOSE=0x80;

    /**
     * Each point an USBtrace::trace() is called is uniquely identified by
     * a TracePoint number.
     */
    enum TracePoint
    {
        //Standard traces
        DEVICE_STATE_CHANGE=      1,//1byte paramter (new state)
        DEVICE_RESET=             2,
        EP0_VALID_SETUP=          3,//8byte parameter (Setup struct)
        SUSPEND_REQUEST=          4,
        RESUME_REQUEST=           5,
        EP0_INTERRUPTED_SETUP=    6,
        EP0_IN_ABORT=             7,
        DESC_ERROR=               8,
        OUT_OF_SHMEM=             9,
        ADDRESS_SET=             10,//1byte parameter (new address)
        CONFIGURING_EP=          11,//1byte bEndpointAddress, 1byte bmAttributes
        EP0_OUT_OVERRUN=         12,

        //Verbose only traces
        EP0_SETUP_IRQ=  VERBOSE | 1,
        EP0_IN_IRQ=     VERBOSE | 2,
        EP0_OUT_IRQ=    VERBOSE | 3,
        EP0_UNSUPP_BREQ=VERBOSE | 4,
        EP0_UNSUPP_DESC=VERBOSE | 5,
        EP0_STATUS_OUT= VERBOSE | 6,
        EP0_STATUS_IN=  VERBOSE | 7,
        IN_BUF_FILL=    VERBOSE | 8,//two 1byte parameters (ep #, buffer size)
        OUT_BUF_READ=   VERBOSE | 9,//two 1byte parameters (ep #, buffer size)

        //Special traces
        DBG_DUMP_EPnR=126,//Two byte parameters (the EPnR 16 bit register)
        MARKER=127, //Generic marker, used for debugging
        TERMINATE=0 //Used within USBtracer to kill the background therad
    };
private:
    Ut();
};

/**
 * \internal
 * Class to trace USB data transfer. Mainly designed for endpoint zero debugging
 */
class Tracer
{
public:
    #ifdef MXUSB_ENABLE_TRACE

    /**
     * Initializes USB tracer, spawns background thread
     */
    static void init();

    /**
     * Quits the tracer, joins the thread
     */
    static void shutdown();

    /**
     * Insert calls to this function where you want to trace something
     * \param tp ID of the point to trace
     */
    static void IRQtrace(Ut::TracePoint tp);

    /**
     * Insert calls to this function where you want to trace something
     * \param tp ID of the point to trace
     * \param param additional trace parameter
     */
    static void IRQtrace(Ut::TracePoint tp, unsigned char param);

    /**
     * Insert calls to this function where you want to trace something
     * \param tp ID of the point to trace
     * \param p1 additional trace parameter
     * \param p2 additional trace parameter
     */
    static void IRQtrace(Ut::TracePoint tp, unsigned char p1, unsigned char p2);

    /**
     * Insert calls to this function where you want to trace something
     * \param tp ID of the point to trace
     * \param data additional trace parameter, array type
     * \param size size of array
     */
    static void IRQtraceArray(Ut::TracePoint tp, unsigned char *data, int size);

    /**
     * Dump EPnR register
     * \param reg register to dump
     */
    static void IRQtraceEPnR(unsigned short reg);

    #else //MXUSB_ENABLE_TRACE
    //Do nothing stubs
    static void init() {}
    static void shutdown() {}
    static void IRQtrace(Ut::TracePoint) {}
    static void IRQtrace(Ut::TracePoint, unsigned char) {}
    static void IRQtrace(Ut::TracePoint, unsigned char, unsigned char) {}
    static void IRQtraceArray(Ut::TracePoint, unsigned char *, int) {}
    static void IRQtraceEPnR(unsigned short reg) {}
    #endif //MXUSB_ENABLE_TRACE
private:
    Tracer();

    /**
     * Thread that prints trace data.
     * Trace data is printed using iprintf, so it is usually redirected to a
     * serial port. Trace format is this:
     * - normal tracepoints start with      "++"
     * - verbose tracepoints start with     "+ "
     * - errors/warnings start with         "**"
     * - verbose errors/warnings start with "* "
     * After that there is a three character code:
     * - EP0 For tracepoints regarding endpoint zero
     * - DEV For tracepoint regarding device state change
     * - IN  For tracepoint regarding IN transactions
     * - OUT For tracepoint regarding OUT transactions
     */
    static void printerThread(void *argv);

    /**
     * Log device state change
     */
    static void deviceStateChange();

    /**
     * Log valid setup
     */
    static void validSetup();

    /**
     * Log when a new address is assigned by the host
     */
    static void addressSet();

    /**
     * Log when IN buffer of an endpoint is filled
     */
    static void inBufFill();

    /**
     * Log when OUT buffer of an endpoint is read
     */
    static void outBufRead();

    /**
     * Log when an endpoint is being configured
     */
    static void configuringEp();

    /**
     * Dump EPnR register
     */
    static void dumpEPnR();

    #ifdef MXUSB_ENABLE_TRACE
    static miosix::Queue<unsigned char,QUEUE_SIZE> queue;
    static volatile bool error; //True in case of queue overflow
    static miosix::Thread *printer;
    #endif //MXUSB_ENABLE_TRACE
};

} //namespace mxusb

#endif //USB_TRACER_H

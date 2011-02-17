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

#include "usb_tracer.h"
#include "usb.h"
#include "def_ctrl_pipe.h"
#include "usb_util.h"
#include <cstdio>

#ifdef MXUSB_ENABLE_TRACE

using namespace std;
using namespace miosix;

namespace mxusb {

//
// class USBtracer
//

void Tracer::init()
{
    if(printer!=0) return; //Already initialized
    error=false;
    queue.reset();
    printer=Thread::create(printerThread,2048,1,0,Thread::JOINABLE);
}

void Tracer::shutdown()
{
    queue.put(Ut::TERMINATE);
    printer->join();
    printer=0;
}

void Tracer::IRQtrace(Ut::TracePoint tp)
{
    if(error) return;
    #ifndef MXUSB_PRINT_VERBOSE
    if(tp & Ut::VERBOSE) return; //Discard verbose
    #endif //MXUSB_PRINT_VERBOSE
    if(queue.IRQput(tp)==false) error=true;
}

void Tracer::IRQtrace(Ut::TracePoint tp, unsigned char param)
{
    if(error) return;
    #ifndef MXUSB_PRINT_VERBOSE
    if(tp & Ut::VERBOSE) return; //Discard verbose
    #endif //MXUSB_PRINT_VERBOSE
    if(queue.IRQput(tp)==false) error=true;
    if(queue.IRQput(param)==false) error=true;
}

void Tracer::IRQtrace(Ut::TracePoint tp, unsigned char p1, unsigned char p2)
{
    if(error) return;
    #ifndef MXUSB_PRINT_VERBOSE
    if(tp & Ut::VERBOSE) return; //Discard verbose
    #endif //MXUSB_PRINT_VERBOSE
    if(queue.IRQput(tp)==false) error=true;
    if(queue.IRQput(p1)==false) error=true;
    if(queue.IRQput(p2)==false) error=true;
}

void Tracer::IRQtraceArray(Ut::TracePoint tp, unsigned char *data, int size)
{
    if(error) return;
    #ifndef MXUSB_PRINT_VERBOSE
    if(tp & Ut::VERBOSE) return; //Discard verbose
    #endif //MXUSB_PRINT_VERBOSE
    if(queue.IRQput(tp)==false) error=true;
    for(int i=0;i<size;i++) if(queue.IRQput(*data++)==false) error=true;
}

void Tracer::IRQtraceEPnR(unsigned short reg)
{
    unsigned char *toChar=reinterpret_cast<unsigned char*>(&reg);
    if(queue.IRQput(Ut::DBG_DUMP_EPnR)==false) error=true;
    if(queue.IRQput(toChar[0])==false) error=true;
    if(queue.IRQput(toChar[1])==false) error=true;
}

void Tracer::printerThread(void *argv)
{
    for(;;)
    {
        unsigned char c;
        queue.get(c);
        if(c==Ut::TERMINATE)
        {
            iprintf("++    Tracer thread is terminating\n");
            return;
        }
        if(error)
        {
            iprintf("**    Error: trace buffer full\n");
            return;
        }

        //Trace printing
        switch(c)
        {
            //Standard traces
            case Ut::DEVICE_STATE_CHANGE:
                deviceStateChange();
                break;
            case Ut::DEVICE_RESET:
                iprintf("++DEV Device RESET\n");
                break;   
            case Ut::EP0_VALID_SETUP:
                validSetup();
                break;
            case Ut::SUSPEND_REQUEST:
                iprintf("++DEV Suspend request\n");
                break;
            case Ut::RESUME_REQUEST:
                iprintf("++DEV Resume request\n");
                break;
            case Ut::EP0_INTERRUPTED_SETUP:
                iprintf("**EP0 Setup interrupts previous transaction\n");
                break;
            case Ut::EP0_IN_ABORT:
                iprintf("**EP0 IN data stage aborted by host\n");
                break;
            case Ut::DESC_ERROR:
                iprintf("**DEV Failed parsing descriptors\n");
                break;
            case Ut::OUT_OF_SHMEM:
                iprintf("**DEV Out of shared memory\n");
                break;
            case Ut::ADDRESS_SET:
                addressSet();
                break;
            case Ut::CONFIGURING_EP:
                configuringEp();
                break;
            case Ut::EP0_OUT_OVERRUN:
                iprintf("**EP0 Overrun within OUT data stage\n");
                break;

            //Verbose only traces
            case Ut::EP0_SETUP_IRQ:
                iprintf("+ EP0 SETUP irq\n");
                break;
            case Ut::EP0_IN_IRQ:
                iprintf("+ EP0 IN irq\n");
                break;
            case Ut::EP0_OUT_IRQ:
                iprintf("+ EP0 OUT irq\n");
                break;
            case Ut::EP0_UNSUPP_BREQ:
                iprintf("* EP0 Unsupported bRequest\n");
                break;
            case Ut::EP0_UNSUPP_DESC:
                iprintf("* EP0 Unsupported descriptor\n");
                break;
            case Ut::EP0_STATUS_OUT:
                iprintf("+ EP0 OUT transaction completed\n");
                break;
            case Ut::EP0_STATUS_IN:
                iprintf("+ EP0 IN transaction completed\n");
                break;
            case Ut::IN_BUF_FILL:
                inBufFill();
                break;
            case Ut::OUT_BUF_READ:
                outBufRead();
                break;

            //Special traces
            case Ut::DBG_DUMP_EPnR:
                dumpEPnR();
                break;
            case Ut::MARKER:
                iprintf("-->   Marker\n");
                break;
            default:
                iprintf("+ Error: unknown TracePoint\n");
        }
    }
}

void Tracer::deviceStateChange()
{
    unsigned char c;
    queue.get(c);
    iprintf("++DEV New device state=");
    switch(c)
    {
        case USBdevice::DEFAULT:
            iprintf("DEFAULT\n");
            break;
        case USBdevice::ADDRESS:
            iprintf("ADDRESS\n");
            break;
        case USBdevice::CONFIGURED:
            iprintf("CONFIGURED\n");
            break;
    }
}

void Tracer::validSetup()
{
    Setup setup;
    unsigned char *packet=reinterpret_cast<unsigned char*>(&setup);
    for(int i=0;i<8;i++)
    {
        queue.get(*packet);
        packet++;
    }
    iprintf("++EP0 Setup={ bmRequestType=0x%x bRequest=%d wValue=%d wIndex=%d "
            "wLength=%d }\n",setup.bmRequestType,setup.bRequest,setup.wValue,
            setup.wIndex,setup.wLength);
}

void Tracer::addressSet()
{
    unsigned char c;
    queue.get(c);
    iprintf("++DEV Host assigned adress %d\n",c);
}

void Tracer::inBufFill()
{
    unsigned char epNum, numBytes;
    queue.get(epNum);
    queue.get(numBytes);
    iprintf("+ IN  endpoint %d: buffer filled with %d bytes\n",epNum,numBytes);
}

void Tracer::outBufRead()
{
    unsigned char epNum, numBytes;
    queue.get(epNum);
    queue.get(numBytes);
    iprintf("+ OUT endpoint %d: reading %d bytes\n",epNum,numBytes);
}

void Tracer::configuringEp()
{
    unsigned char bEndpointAddress, bmAttributes;
    queue.get(bEndpointAddress);
    queue.get(bmAttributes);
    if(bEndpointAddress & 0x80) iprintf("++DEV configuring IN endpoint ");
    else iprintf("++DEV configuring OUT endpoint ");
    iprintf("%d as ",bEndpointAddress & 0x7f);
    switch(bmAttributes & Descriptor::TYPE_MASK)
    {
        case Descriptor::CONTROL:
            iprintf("CONTROL\n");
            break;
        case Descriptor::BULK:
           iprintf("BULK\n");
            break;
        case Descriptor::INTERRUPT:
           iprintf("INTERRUPT\n");
            break;
        case Descriptor::ISOCHRONOUS:
           iprintf("ISOCHRONOUS\n");
            break;
    }
}

void Tracer::dumpEPnR()
{
    unsigned char a[2];
    queue.get(a[0]);
    queue.get(a[1]);
    iprintf("--> EPnR=0x%x\n",toShort(a));
}

miosix::Queue<unsigned char,QUEUE_SIZE> Tracer::queue;
volatile bool Tracer::error; //True in case of queue overflow
miosix::Thread *Tracer::printer=0;

} //namespace mxusb

#endif //MXUSB_ENABLE_TRACE

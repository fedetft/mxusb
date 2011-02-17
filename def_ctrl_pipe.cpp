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

#include <algorithm>
#include <cstdio>
#include "def_ctrl_pipe.h"
#include "shared_memory.h"
#include "usb_util.h"
#include "usb_tracer.h"
#include "usb_config.h"
#include "usb_impl.h"
#include "ep0.h"

using namespace std;

namespace mxusb {

//
// class DefCtrlPipe
//

#define xassert(x) { if(!(x)) \
    { iprintf("Descriptor error %s:%d\n",__FILE__,__LINE__); return false; } \
}

bool DefCtrlPipe::registerAndValidateDescriptors(const unsigned char *device,
            const unsigned char * const * configs,
            const unsigned char * const * strings,
            unsigned char numStrings)
{
    #ifdef MXUSB_ENABLE_DESC_VALIDATION
    //Check device descriptor
    xassert(device[0]==18); //Device descriptor size is constant
    xassert(device[1]==Descriptor::DEVICE);
    xassert(device[7]==SharedMemory::EP0_SIZE);
    const unsigned char numConfigs=device[17];

    //Check configuration descriptors
    for(int i=0;i<numConfigs;i++)
    {
        if(validateConfigEndpoint(configs[i],i)==false) return false;
    }

    //Check string descriptors
    for(int i=0;i<numStrings;i++)
    {
        xassert((strings[i][0] % 2)==0); //Strings are unicode utf16
        xassert(strings[i][1]==Descriptor::STRING);
    }
    #endif //#ifdef MXUSB_ENABLE_DESC_VALIDATION
    deviceDesc=device;
    configDesc=configs;
    stringsDesc=strings;
    numStringDesc=numStrings;
    return true;
}

void DefCtrlPipe::IRQsetup()
{
    Tracer::IRQtrace(Ut::EP0_SETUP_IRQ);
    //Setup packets are all 8 bytes long, ignore anything with a different size
    if(USB->endpoint[0].IRQgetReceivedBytes()!=8) return;

    //Copy packet into setup struct
    unsigned char *packet=reinterpret_cast<unsigned char*>(&setup);
    SharedMemory::copyBytesFrom(packet,SharedMemory::EP0RX_ADDR,sizeof(setup));
    Tracer::IRQtraceArray(Ut::EP0_VALID_SETUP,packet,sizeof(setup));

    if(controlState.state!=CTR_NO_REQ_PENDING)
    {
        //This setup packet is interrupting a previous ongoing control
        //transaction, so abort the previous one
        controlState.state=CTR_NO_REQ_PENDING;
        //Clear EP_KIND in case the interruption happened while EP_KIND was set
        USB->endpoint[0].IRQclearEpKind();
        fixForStallTiming=false;
        Tracer::IRQtrace(Ut::EP0_INTERRUPTED_SETUP);
    }

    //Forward non standard requests to user code via callbacks
    if((setup.bmRequestType & Setup::TYPE_MASK)!=Setup::TYPE_STANDARD)
    {
        controlState.ptr=0;

        if(EndpointZeroCallbacks::IRQgetCallbacks()->IRQsetup(&setup)==false)
            return; //Not recognized as a valid setup request for this device

        if(setup.wLength==0)
        {
                /*
                 * It seems that if wLength==0 bit #7 of bmRequestType should
                 * be ignored, and all thransactions should be treated as OUT
                 * for what concerns ACK type.
                 */
                //STATUS handshake is an IN with zero bytes
                controlState.state=CTR_OUT_STATUS;
                USB->endpoint[0].IRQsetTxDataSize(0);
                IRQsetEp0TxValid();
                return; //Do not go through the standard requests switch
        }

        //This is an error, user code has accepted the request, but has
        //not set the buffer for the data stage. So reject the transfer
        if(controlState.ptr==0) return;

        if(setup.bmRequestType & 0x80)
        {
            IRQstartInData(controlState.ptr,controlState.size);
        } else {
            controlState.state=CTR_CUSTOM_OUT_IN_PROGRESS;
            IRQsetEp0RxValid();
        }
        return; //Do not go through the standard requests switch
    }
    
    switch(setup.bRequest)
    {
//        case Setup::CLEAR_FEATURE: TODO
        case Setup::GET_CONFIGURATION:
            IRQgetConfiguration();
            break;
        case Setup::GET_DESCRIPTOR:
            IRQgetDescriptor();
            break;
//        case Setup::GET_INTERFACE: TODO
        case Setup::GET_STATUS:
            IRQgetStatus();
            break;
        case Setup::SET_ADDRESS:
            IRQsetAddress();
            break;
        case Setup::SET_CONFIGURATION:
            IRQsetConfiguration();
            break;
            Tracer::IRQtrace(Ut::EP0_UNSUPP_BREQ);
            break;
//        case Setup::SET_FEATURE: TODO
//        case Setup::SET_INTERFACE: TODO
        case Setup::SET_DESCRIPTOR: //(fallthrough) This won't be implemented.
        case Setup::SYNCH_FRAME:    //(fallthrough) This won't be implemented.
        default:
            Tracer::IRQtrace(Ut::EP0_UNSUPP_BREQ);
            break;
    }
}

void DefCtrlPipe::IRQin()
{
    Tracer::IRQtrace(Ut::EP0_IN_IRQ);
    switch(controlState.state)
    {
        case CTR_IN_IN_PROGRESS:
            IRQstartInData(controlState.ptr,controlState.size);
            break;
        case CTR_IN_STATUS_BEGIN:
            controlState.state=CTR_IN_STATUS_END;
            USB->endpoint[0].IRQsetEpKind();
            IRQsetEp0RxValid();
            break;
        case CTR_SET_ADDRESS:
            controlState.state=CTR_NO_REQ_PENDING;
            //We just received the status handshake as a response to a
            //SET_ADDRESS, now it's time to change our address
            USB->DADDR=setup.wValue | USB_DADDR_EF;
            if(setup.wValue!=0)
                DeviceStateImpl::IRQsetState(USBdevice::ADDRESS);
            else DeviceStateImpl::IRQsetState(USBdevice::DEFAULT);
            Tracer::IRQtrace(Ut::ADDRESS_SET,setup.wValue);
            break;
        case CTR_OUT_STATUS:
            //End of control OUT request
            controlState.state=CTR_NO_REQ_PENDING;
            Tracer::IRQtrace(Ut::EP0_STATUS_OUT);
            break;
        default:
            break;
    }
}

void DefCtrlPipe::IRQout()
{
    Tracer::IRQtrace(Ut::EP0_OUT_IRQ);
    switch(controlState.state)
    {
        case CTR_IN_STATUS_END:
            //Good, STATUS handshake for IN transaction completed
            controlState.state=CTR_NO_REQ_PENDING;
            USB->endpoint[0].IRQclearEpKind();
            Tracer::IRQtrace(Ut::EP0_STATUS_IN);
            break;
        case CTR_CUSTOM_OUT_IN_PROGRESS:
            IRQstartCustomOutData();
            break;
        case CTR_IN_IN_PROGRESS:
        case CTR_IN_STATUS_BEGIN:
            /*
             * Quirk:
             * When handling GET_STATUS transactions and the source code is
             * compiled to run in external RAM a timing issue causes a spurious
             * abort, see the Log:
             * + EP0 SETUP irq
             * ++EP0 Setup={ bmRequestType=0x80 bRequest=0 wValue=0 wIndex=0
             * wLength=2 }
             * ++IN  endpoint 0 buffer filled with 2 bytes
             * + EP0 OUT irq
             * **EP0 IN data stage aborted by host
             * + EP0 IN irq
             *
             * Reason: the USB interrupt handler is called only once with both
             * OUT and IN interrupt flags set in the EP0R register, because a
             * two-byte IN transaction is really fast, while the STM32 running
             * from external RAM is slow.
             * When both interrupt flags are set, the information regarding
             * the ORDER in which the events happened is lost.
             * Then, the interrupt  handler in usb.cpp first checks if OUT
             * bit is set and calls IRQout(), and then calls IRQin(), even if
             * the packets on the bus were sent in the opposite order.
             * On the PC side no error are reported, so this is not a bug, and
             * these aborts in the log can be ignored.
             * Note that running the same program from the (faster) FLASH
             * results in two separate interrupts, and no oredering ambiguity.
             */
            controlState.state=CTR_NO_REQ_PENDING;
            USB->endpoint[0].IRQclearEpKind();
            Tracer::IRQtrace(Ut::EP0_IN_ABORT);
            break;
        default:
            break;
    }
}

void DefCtrlPipe::IRQdefaultStatus()
{
    //Clear interrupt flag, EP_KIND bit, set endpoint address to zero
    EndpointRegister& epr=USB->endpoint[0];
    epr=0;
    epr.IRQsetType(EndpointRegister::CONTROL);
    epr.IRQsetTxBuffer(SharedMemory::EP0TX_ADDR,SharedMemory::EP0_SIZE);
    epr.IRQsetRxBuffer(SharedMemory::EP0RX_ADDR,SharedMemory::EP0_SIZE);
    USB->endpoint[0].IRQsetTxStatus(EndpointRegister::STALL);
    USB->endpoint[0].IRQsetRxStatus(EndpointRegister::STALL);
}

void DefCtrlPipe::IRQrestoreStatus()
{
    if(fixForStallTiming==false)
    {
        if(txUntouchedFlag)
            USB->endpoint[0].IRQsetTxStatus(EndpointRegister::STALL);
        if(rxUntouchedFlag)
            USB->endpoint[0].IRQsetRxStatus(EndpointRegister::STALL);
    } else {
        if(txUntouchedFlag)
            USB->endpoint[0].IRQsetTxStatus(EndpointRegister::NAK);
        if(rxUntouchedFlag)
            USB->endpoint[0].IRQsetRxStatus(EndpointRegister::NAK);
    }
}

void DefCtrlPipe::IRQsetEp0TxValid()
{
    USB->endpoint[0].IRQsetTxStatus(EndpointRegister::VALID);
    txUntouchedFlag=false;
}

void DefCtrlPipe::IRQsetEp0RxValid()
{
    USB->endpoint[0].IRQsetRxStatus(EndpointRegister::VALID);
    rxUntouchedFlag=false;
}

void DefCtrlPipe::IRQgetDescriptor()
{
    //If wrong direction or recipient, ignore
    if((setup.bmRequestType & Setup::DIR_MASK)!=Setup::DIR_IN ||
       (setup.bmRequestType & Setup::RECIPIENT_MASK)!=Setup::RECIPIENT_DEVICE)
        return;

    unsigned char descType=setup.wValue >> 8;
    unsigned char descIndex=setup.wValue & 0xff;
    switch(descType)
    {
        case Descriptor::DEVICE:
            if(setup.wIndex!=0 || descIndex!=0) return;
            IRQstartInData(deviceDesc,min<unsigned short>(
                    setup.wLength,deviceDesc[0]));
            break;
        case Descriptor::CONFIGURATION:
            //deviceDesc[17]=bNumConfigrations
            if(setup.wIndex!=0 || descIndex>=deviceDesc[17]) return;
            //NOTE: configDesc[2] and not configDesc[0] because of wTotalLength
            IRQstartInData(configDesc[descIndex],
                    min(setup.wLength,toShort(&configDesc[descIndex][2])));
            break;
        case Descriptor::STRING:
            if(descIndex>=numStringDesc) return;
            if(setup.wIndex!=0 && setup.wIndex!=toShort(&stringsDesc[0][2]))
                return;
            IRQstartInData(stringsDesc[descIndex],min<unsigned short>(
                    setup.wLength,stringsDesc[descIndex][0]));
            break;
        default:
            Tracer::IRQtrace(Ut::EP0_UNSUPP_DESC);
            break;
        //interface and endpoint descriptors seem unused...
    }
}

void DefCtrlPipe::IRQsetAddress()
{
    //Shorthand for DIR_OUT and RECIPIENT_DEVICE
    if(setup.bmRequestType!=0) return;
    if(setup.wIndex!=0 || setup.wLength!=0) return;
    //We can't actually set the address now, so we use the state machine
    //to remenber that we need to set the address at the end of the STATUS
    //transaction
    controlState.state=CTR_SET_ADDRESS;
    //STATUS handshake is an IN with zero bytes
    USB->endpoint[0].IRQsetTxDataSize(0);
    IRQsetEp0TxValid();
}

void DefCtrlPipe::IRQgetConfiguration()
{
    //If wrong direction or recipient, ignore
    if((setup.bmRequestType & Setup::DIR_MASK)!=Setup::DIR_IN ||
       (setup.bmRequestType & Setup::RECIPIENT_MASK)!=Setup::RECIPIENT_DEVICE)
        return;
    if(setup.wValue!=0 || setup.wIndex!=0 || setup.wLength!=1) return;
    unsigned char configuration=USBdevice::IRQgetConfiguration();
    IRQstartInData(&configuration,1);
}

void DefCtrlPipe::IRQsetConfiguration()
{
    //Shorthand for DIR_OUT and RECIPIENT_DEVICE
    if(setup.bmRequestType!=0) return;
    if(setup.wIndex!=0 || setup.wLength!=0 || (setup.wValue & 0xff00)) return;
    unsigned char config=setup.wValue & 0xff;
    //deviceDesc[17]=bNumConfigrations
    if(config>deviceDesc[17]) return;
    
    //In any case, deconfigure all endpoints except endpoint zero.
    //Then, if config!=0 reconfigure endpoints
    EndpointImpl::IRQdeconfigureAll();
    
    if(config!=0)
    {
        DeviceStateImpl::IRQsetConfiguration(config);
        EndpointImpl::IRQconfigureAll(configDesc[config-1]);
        DeviceStateImpl::IRQsetState(USBdevice::CONFIGURED);
    } else {
        DeviceStateImpl::IRQsetConfiguration(0);
        DeviceStateImpl::IRQsetState(USBdevice::ADDRESS);
    }
    //STATUS handshake is an IN with zero bytes
    controlState.state=CTR_OUT_STATUS;
    USB->endpoint[0].IRQsetTxDataSize(0);
    IRQsetEp0TxValid();
}

void DefCtrlPipe::IRQgetStatus()
{
    //If wrong direction or recipient, ignore
    if((setup.bmRequestType & Setup::DIR_MASK)!=Setup::DIR_IN) return;
    if(setup.wValue!=0 || setup.wLength!=2) return;
    unsigned short result=0;
    switch(setup.bmRequestType & Setup::RECIPIENT_MASK)
    {
        case Setup::RECIPIENT_DEVICE:
            //Request invalid if device not configured.
            if(USBdevice::IRQgetState()!=USBdevice::CONFIGURED) return;
            //Get SELF_POWERED information from current configuration descriptor
            if(configDesc[USBdevice::IRQgetConfiguration()-1][7] & 0x40)
                result=1;
            //Remote wakeup not yet implemented
            IRQstartInData(reinterpret_cast<unsigned char*>(&result),2);
            break;
        case Setup::RECIPIENT_INTERFACE:
            //The standard says to return zero, so just do it
            IRQstartInData(reinterpret_cast<unsigned char*>(&result),2);
            break;
//        case Setup::RECIPIENT_ENDPOINT: TODO
        default:
            return;
    }
}

void DefCtrlPipe::IRQstartInData(const unsigned char* data, unsigned short size)
{
    EndpointRegister& ep=USB->endpoint[0];
    //Note the >=, since the USB spec state that a data transmission ends with
    //a packet less than maxium size OR a zero byte packet. If size==ep0size
    //there is the need to send that zero byte packet
    if(size>=EP0_SIZE)
    {
        SharedMemory::copyBytesTo(SharedMemory::EP0TX_ADDR,data,EP0_SIZE);
        ep.IRQsetTxDataSize(EP0_SIZE);
        controlState.state=CTR_IN_IN_PROGRESS;
        //The cast is necessary because controlState.ptr is used both for
        //IN and OUT transactions, so it can't be const. When used for IN
        //transaction no one will write to that pointer, so the cast is safe
        controlState.ptr=const_cast<unsigned char *>(data)+EP0_SIZE;
        controlState.size=size-EP0_SIZE;
        Tracer::IRQtrace(Ut::IN_BUF_FILL,0,EP0_SIZE);
    } else {
        SharedMemory::copyBytesTo(SharedMemory::EP0TX_ADDR,data,size);
        ep.IRQsetTxDataSize(size);
        controlState.state=CTR_IN_STATUS_BEGIN;
        Tracer::IRQtrace(Ut::IN_BUF_FILL,0,size);
    }
    IRQsetEp0TxValid();
    //It looks like the host can abort an IN data stage by issuing the STATUS
    //packet (a zero-byte OUT packet) sooner than expected. To support this,
    //we enable the endpoint for RX too, only for zero-size packets (EP_KIND)
    USB->endpoint[0].IRQsetEpKind();
    IRQsetEp0RxValid();
}

void DefCtrlPipe::IRQstartCustomOutData()
{
    const unsigned short received=USB->endpoint[0].IRQgetReceivedBytes();
    Tracer::IRQtrace(Ut::OUT_BUF_READ,0,received);

    if(received>controlState.size)
    {
        Tracer::IRQtrace(Ut::EP0_OUT_OVERRUN);
        return;
    }

    SharedMemory::copyBytesFrom(controlState.ptr,SharedMemory::EP0RX_ADDR,
            received);
    controlState.ptr+=received;
    controlState.size-=received;
    
    if(controlState.size>0)
    {
        //If next transfer is last transfer, be sure not to STALL the IN packet
        if(controlState.size<=EP0_SIZE) fixForStallTiming=true;

        IRQsetEp0RxValid();
        return;
    }

    //We reach here when the last transfer arrived, disable the fix
    fixForStallTiming=false;
    
    if(EndpointZeroCallbacks::IRQgetCallbacks()->IRQendOfOutDataStage(&setup))
    {
        //STATUS handshake is an IN with zero bytes
        controlState.state=CTR_OUT_STATUS;
        USB->endpoint[0].IRQsetTxDataSize(0);
        IRQsetEp0TxValid();
    } //else STALL, since the received data was not acknowledged by user code
}

bool DefCtrlPipe::validateConfigEndpoint(const unsigned char* config, int num)
{
    xassert(config[0]==9);     //Descriptor size
    xassert(config[5]==num+1); //Expecting config descriptors in order

    //Parse descriptors nested in the configuration descriptor
    const unsigned short wTotalLength=toShort(&config[2]);
    unsigned short curDescBase=0;

    struct EpCheck
    {
        EpCheck() : in(false), out(false), interrupt(false) {}
        
        bool in;        //True if IN side used
        bool out;       //True if OUT side used
        bool interrupt; //True if INTERRUPT type
    };
    EpCheck used[NUM_ENDPOINTS-1];

    for(;;)
    {
        curDescBase+=config[curDescBase+0];  //Advance to next descriptor
        if(curDescBase==wTotalLength) break; //End of configuration descriptor?
        xassert(curDescBase<=wTotalLength);  //Wrong wTotalLength
        xassert(config[curDescBase+0]!=0);   //Desc size of 0 not allowed

        unsigned char epAddr; //Endpoint address
        bool epDirection;     //True if IN
        Descriptor::Type type;//Endpoint type
        switch(config[curDescBase+1])
        {
            case Descriptor::INTERFACE:
                xassert(config[curDescBase+0]==9); //Descriptor size
                xassert(config[curDescBase+3]==0); //Alt settings unsupported
                break;
            case Descriptor::ENDPOINT:
                xassert(config[curDescBase+0]==7); //Descriptor size

                epAddr=config[curDescBase+2] & 0x7f;
                epDirection=(config[curDescBase+2] & 0x80)!=0;
                type=static_cast<Descriptor::Type>(
                        config[curDescBase+3] & Descriptor::TYPE_MASK);

                //This limits the number of endpoints to NUM_ENDPOINTS
                xassert(epAddr<NUM_ENDPOINTS && epAddr!=0);

                //No two descriptor with same ep address and same direction
                //Moreover, if two endpoint have the same number and opposite
                //direction, they must be of type INTERRUPT
                if(epDirection) //Is it a descriptor of a IN endpoint?
                {
                    xassert(used[epAddr-1].in==false);
                    if(used[epAddr-1].out)
                    {
                        //If both enabled both of type interrupt
                        xassert(used[epAddr-1].interrupt==true);
                        xassert(type==Descriptor::INTERRUPT);
                    }
                    used[epAddr-1].in=true;
                } else {
                    xassert(used[epAddr-1].out==false);
                    if(used[epAddr-1].in)
                    {
                        //If both enabled both of type interrupt
                        xassert(used[epAddr-1].interrupt==true);
                        xassert(type==Descriptor::INTERRUPT);
                    }
                    used[epAddr-1].out=true;
                }
                if(type==Descriptor::INTERRUPT) used[epAddr-1].interrupt=true;

                //Isochronous endpoints and control endpoint other than
                //endpoint zero unsuppoted
                xassert(type!=Descriptor::CONTROL);
                xassert(type!=Descriptor::ISOCHRONOUS);

                //Limit size of endpoint to what's allowed for full speed device
                xassert(toShort(&config[curDescBase+4])!=0); //size >0
                //size <=255 because of constraint imposed by EndpointImpl
                xassert(toShort(&config[curDescBase+4])<=255);
                break;
            case Descriptor::CONFIGURATION:
                iprintf("Error: config descriptor nested in config desc\n");
                return false;
            case Descriptor::DEVICE:
                iprintf("Error: device descriptor nested in config desc\n");
                return false;
            case Descriptor::STRING:
                iprintf("Error: string descriptor nested in config desc\n");
                return false;
            default:
                iprintf("Warning: unknown descriptor 0x%x\n",
                        config[curDescBase+1]);
        }
    }
    return true;
}

const unsigned char *DefCtrlPipe::deviceDesc=0;
const unsigned char * const * DefCtrlPipe::configDesc=0;
const unsigned char * const * DefCtrlPipe::stringsDesc=0;
unsigned char DefCtrlPipe::numStringDesc=0;
DefCtrlPipe::ControlStateMachine DefCtrlPipe::controlState=
{
    DefCtrlPipe::CTR_NO_REQ_PENDING,0,0
};
Setup DefCtrlPipe::setup;
bool DefCtrlPipe::txUntouchedFlag;
bool DefCtrlPipe::rxUntouchedFlag;
bool DefCtrlPipe::fixForStallTiming=false;

} //namespace mxusb

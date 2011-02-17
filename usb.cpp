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

#include "usb.h"
#include "stm32_usb_regs.h"
#include "shared_memory.h"
#include "def_ctrl_pipe.h"
#include "usb_gpio.h"
#include "usb_tracer.h"
#include "usb_impl.h"
#include <algorithm>

#ifdef _MIOSIX
#include "interfaces/delays.h"
#include "kernel/kernel.h"
#include "interfaces-impl/portability_impl.h"
using namespace miosix;
#else //_MIOSIX
#include "libraries/system.h"
#endif //_MIOSIX

using namespace std;

//
// interrupt handler
//

/**
 * \internal
 * Low priority interrupt, called for everything except double buffered
 * bulk/isochronous correct transfers.
 */
extern void USB_LP_CAN1_RX0_IRQHandler() __attribute__((naked));
void USB_LP_CAN1_RX0_IRQHandler()
{
    #ifdef _MIOSIX
    //Since a context switch can happen within this interrupt handler, it is
    //necessary to save and restore context
    saveContext();
    asm volatile("bl _ZN5mxusb15USBirqLpHandlerEv");
    restoreContext();
    #else //_MIOSIX
    asm volatile("bl _ZN5mxusb15USBirqLpHandlerEv");
    asm volatile("bx lr");
    #endif //_MIOSIX
}

/**
 * \internal
 * High priority interrupt, called for a correct transfer on double bufferes
 * bulk/isochronous endpoint.
 */
extern void USB_HP_CAN1_TX_IRQHandler() __attribute__((naked));
void USB_HP_CAN1_TX_IRQHandler()
{
    #ifdef _MIOSIX
    //Since a context switch can happen within this interrupt handler, it is
    //necessary to save and restore context
    saveContext();
    asm volatile("bl _ZN5mxusb15USBirqHpHandlerEv");
    restoreContext();
    #else //_MIOSIX
    asm volatile("bl _ZN5mxusb15USBirqHpHandlerEv");
    asm volatile("bx lr");
    #endif //_MIOSIX
}

namespace mxusb {

/**
 * \internal
 * Handles USB device RESET
 */
static void IRQhandleReset()
{
    Tracer::IRQtrace(Ut::DEVICE_RESET);

    USB->DADDR=0;  //Disable transaction handling
    USB->ISTR=0;   //When the device is reset, clear all pending interrupts
    USB->BTABLE=SharedMemory::BTABLE_ADDR; //Set BTABLE

    for(int i=1;i<NUM_ENDPOINTS;i++) EndpointImpl::get(i)->IRQdeconfigure(i);
    SharedMemory::reset();
    DefCtrlPipe::IRQdefaultStatus();

    //After a reset device address is zero, enable transaction handling
    USB->DADDR=0 | USB_DADDR_EF;

    //Enable more interrupt sources now that reset happened
    USB->CNTR=USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM | USB_CNTR_RESETM;

    //Device is now in the default address state
    DeviceStateImpl::IRQsetState(USBdevice::DEFAULT);
}

/**
 * \internal
 * Actual low priority interrupt handler.
 */
void USBirqLpHandler() __attribute__ ((noinline));
void USBirqLpHandler()
{
    unsigned short flags=USB->ISTR;
    Callbacks *callbacks=Callbacks::IRQgetCallbacks();
    if(flags & USB_ISTR_RESET)
    {
        IRQhandleReset();
        callbacks->IRQreset();
        return; //Reset causes all interrupt flags to be ignored
    }
    if(flags & USB_ISTR_SUSP)
    {
        USB->ISTR= ~USB_ISTR_SUSP; //Clear interrupt flag
        USB->CNTR|=USB_CNTR_FSUSP;
        USB->CNTR|=USB_CNTR_LP_MODE;
        Tracer::IRQtrace(Ut::SUSPEND_REQUEST);
        DeviceStateImpl::IRQsetSuspended(true);
        //If device is configured, deconfigure all endpoints. This in turn will
        //wake the threads waiting to write/read on endpoints
        if(USBdevice::IRQgetState()==USBdevice::CONFIGURED)
            EndpointImpl::IRQdeconfigureAll();
        callbacks->IRQsuspend();
    }
    if(flags & USB_ISTR_WKUP)
    {
        USB->ISTR= ~USB_ISTR_WKUP; //Clear interrupt flag
        USB->CNTR&= ~USB_CNTR_FSUSP;
        Tracer::IRQtrace(Ut::RESUME_REQUEST);
        DeviceStateImpl::IRQsetSuspended(false);
        callbacks->IRQresume();
        //Reconfigure all previously deconfigured endpoints
        unsigned char conf=USBdevice::IRQgetConfiguration();
        if(conf!=0)
            EndpointImpl::IRQconfigureAll(DefCtrlPipe::IRQgetConfigDesc(conf));
    }
    while(flags & USB_ISTR_CTR)
    {
        int epNum=flags & USB_ISTR_EP_ID;
        unsigned short reg=USB->endpoint[epNum].get();
        if(epNum==0)
        {
            DefCtrlPipe::IRQstatusNak();
            //Transaction on endpoint zero
            if(reg & USB_EP0R_CTR_RX)
            {
                bool isSetupPacket=reg & USB_EP0R_SETUP;
                USB->endpoint[epNum].IRQclearRxInterruptFlag();
                if(isSetupPacket) DefCtrlPipe::IRQsetup();
                else DefCtrlPipe::IRQout();
            }

            if(reg & USB_EP0R_CTR_TX)
            {
                USB->endpoint[epNum].IRQclearTxInterruptFlag();
                DefCtrlPipe::IRQin();
            }
            DefCtrlPipe::IRQrestoreStatus();

        } else {
            //Transaction on other endpoints
            EndpointImpl *epi=EndpointImpl::IRQget(epNum);
            if(reg & USB_EP0R_CTR_RX)
            {
                USB->endpoint[epNum].IRQclearRxInterruptFlag();
                //NOTE: Increment buffer before the callabck
                epi->IRQincBufferCount();
                callbacks->IRQendpoint(epNum,Endpoint::OUT);
                epi->IRQwakeWaitingThreadOnOutEndpoint();
            }

            if(reg & USB_EP0R_CTR_TX)
            {
                USB->endpoint[epNum].IRQclearTxInterruptFlag();

                /*
                 * Fix for double buffered bulk IN endpoints when only one of
                 * the two buffer is filled. This does not get rid of sending
                 * an empty buffer to the host if it reads while only one buffer
                 * is filled, but at least by setting dataSize to 0 it avoids
                 * sending the same buffer twice.
                 * Actually, since the high priority interrupt is enabled,
                 * this interrupt handler should never be called with a
                 * double buffered bulk endpoint request and therefore this code
                 * should not be required, but guess what? It DOES happen...
                 */
                if(epi->IRQgetData().type==Descriptor::BULK)
                {
                    if(USB->endpoint[epNum].IRQgetDtogTx())
                    {
                        USB->endpoint[epNum].IRQsetTxDataSize0(0);
                    } else {
                        USB->endpoint[epNum].IRQsetTxDataSize1(0);
                    }
                }

                //NOTE: Decrement buffer before the callabck
                epi->IRQdecBufferCount();
                callbacks->IRQendpoint(epNum,Endpoint::IN);
                epi->IRQwakeWaitingThreadOnInEndpoint();
            }
        }
        //Read again the ISTR register so that if more endpoints have completed
        //a transaction, they are all serviced
        flags=USB->ISTR;
    }
}

/**
 * \internal
 * Actual high priority interrupt handler.
 */
void USBirqHpHandler() __attribute__ ((noinline));
void USBirqHpHandler()
{
    unsigned short flags=USB->ISTR;
    Callbacks *callbacks=Callbacks::IRQgetCallbacks();
    while(flags & USB_ISTR_CTR)
    {
        int epNum=flags & USB_ISTR_EP_ID;
        unsigned short reg=USB->endpoint[epNum].get();
        EndpointImpl *epi=EndpointImpl::IRQget(epNum);
        if(reg & USB_EP0R_CTR_RX)
        {
            USB->endpoint[epNum].IRQclearRxInterruptFlag();
            //NOTE: Increment buffer before the callabck
            epi->IRQincBufferCount();
            callbacks->IRQendpoint(epNum,Endpoint::OUT);
            epi->IRQwakeWaitingThreadOnOutEndpoint();
        }

        if(reg & USB_EP0R_CTR_TX)
        {
            USB->endpoint[epNum].IRQclearTxInterruptFlag();

            /*
             * Fix for double buffered bulk IN endpoints when only one of
             * the two buffer is filled. This does not get rid of sending
             * an empty buffer to the host if it reads while unly one buffer
             * is filled, but at least by setting dataSize to 0 it avoids
             * sending the same buffer twice.
             * Note: this code should be guarded by a
             * if(epi->IRQgetData().type==Descriptor::BULK) to check if the
             * endpoint is really a BULK endpoint, but is not because the high
             * priority interrupt is only fired by double buffered bulk
             * endpoints (hopefully)
             */
            if(USB->endpoint[epNum].IRQgetDtogTx())
            {
                USB->endpoint[epNum].IRQsetTxDataSize0(0);
            } else {
                USB->endpoint[epNum].IRQsetTxDataSize1(0);
            }

            //NOTE: Decrement buffer before the callabck
            epi->IRQdecBufferCount();
            callbacks->IRQendpoint(epNum,Endpoint::IN);
            epi->IRQwakeWaitingThreadOnInEndpoint();
        }
        //Read again the ISTR register so that if more endpoints have completed
        //a transaction, they are all serviced
        flags=USB->ISTR;
    }
}

//
// class Endpoint
//

Endpoint Endpoint::get(unsigned char epNum)
{
    return Endpoint(EndpointImpl::get(epNum));
}

const int Endpoint::maxNumEndpoints()
{
    return NUM_ENDPOINTS;
}

bool Endpoint::isInSideEnabled() const
{
    return pImpl->getData().enabledIn==1;
}

bool Endpoint::isOutSideEnabled() const
{
    return pImpl->getData().enabledOut==1;
}

unsigned short Endpoint::inSize() const
{
    return pImpl->getSizeOfInBuf();
}

unsigned short Endpoint::outSize() const
{
    return pImpl->getSizeOfOutBuf();
}

bool Endpoint::write(const unsigned char *data, int size, int& written)
{
    written=0;
    #ifdef _MIOSIX
    InterruptDisableLock dLock;
    unsigned char initialConfig=DeviceStateImpl::getConfiguration();
    for(;;)
    {
        int partialWritten;
        bool result=IRQwrite(data,size,partialWritten);
        written+=partialWritten;
        data+=partialWritten;
        size-=partialWritten;
        if(size<=0) return true;
        if(result==false) return false; //Return error *after* updating written
        if(partialWritten==0)
        {
            Thread *self=Thread::IRQgetCurrentThread();
            pImpl->IRQsetWaitingThreadOnInEndpoint(self);
            self->IRQwait();
            {
                InterruptEnableLock eLock(dLock);
                Thread::yield(); //The wait becomes effective
            }
            //If configuration changet in the meantime, return error
            if(DeviceStateImpl::getConfiguration()!=initialConfig) return false;
        }
    }
    #else //_MIOSIX
    unsigned char initialConfig=DeviceStateImpl::getConfiguration();
    for(;;)
    {
        if(DeviceStateImpl::getConfiguration()!=initialConfig) return false;
        int partialWritten;
        __disable_irq();
        bool result=IRQwrite(data,size,partialWritten);
        __enable_irq();
        written+=partialWritten;
        data+=partialWritten;
        size-=partialWritten;
        if(size<=0) return true;
        if(result==false) return false; //Return error *after* updating written
        //if(partialWritten==0) __WFI();
        //FIXME: can't add a __WFI() here to minimize power consumption because
        //of a race condition: if the interrupt occurs after __enable_irq()
        //but before the __WFI() the program will lock. Miosix does not have
        //this race condition
    }
    #endif //_MIOSIX
}

bool Endpoint::read(unsigned char *data, int& readBytes)
{
    #ifdef _MIOSIX
    InterruptDisableLock dLock;
    unsigned char initialConfig=DeviceStateImpl::getConfiguration();
    for(;;)
    {
        if(IRQread(data,readBytes)==false) return false; //Error
        if(readBytes>0) return true; //Got data
        Thread *self=Thread::IRQgetCurrentThread();
        pImpl->IRQsetWaitingThreadOnOutEndpoint(self);
        self->IRQwait();
        {
            InterruptEnableLock eLock(dLock);
            Thread::yield(); //The wait becomes effective
        }
        //If configuration changet in the meantime, return error
        if(DeviceStateImpl::getConfiguration()!=initialConfig) return false;
    }
    #else //_MIOSIX
    unsigned char initialConfig=DeviceStateImpl::getConfiguration();
    for(;;)
    {
        if(DeviceStateImpl::getConfiguration()!=initialConfig) return false;
        __disable_irq();
        bool result=IRQread(data,readBytes);
        __enable_irq();
        if(result==false) return false; //Error
        if(readBytes>0) return true;    //Got data
        //__WFI();
        //FIXME: can't add a __WFI() here to minimize power consumption because
        //of a race condition: if the interrupt occurs after __enable_irq()
        //but before the __WFI() the program will lock. Miosix does not have
        //this race condition
    }
    #endif //_MIOSIX
}

bool Endpoint::IRQwrite(const unsigned char *data, int size, int& written)
{
    written=0;
    if(pImpl->IRQgetData().enabledIn==0) return false;
    EndpointRegister& epr=USB->endpoint[pImpl->IRQgetData().epNumber];
    EndpointRegister::Status stat=epr.IRQgetTxStatus();
    if(stat==EndpointRegister::STALL) return false;

    if(pImpl->IRQgetData().type==Descriptor::INTERRUPT)
    {
        //INTERRUPT
        if(stat!=EndpointRegister::NAK) return true;//No error, just buffer full
        written=min<unsigned int>(size,pImpl->IRQgetSizeOfInBuf());
        SharedMemory::copyBytesTo(pImpl->IRQgetInBuf(),data,written);
        epr.IRQsetTxDataSize(written);
        epr.IRQsetTxStatus(EndpointRegister::VALID);
    } else {
        //BULK
        if(pImpl->IRQgetBufferCount()>=2) return true;//No err, just buffer full
        pImpl->IRQincBufferCount();
        if(epr.IRQgetDtogRx()) //Actually, SW_BUF
        {
            written=min<unsigned int>(size,pImpl->IRQgetSizeOfBuf1());
            SharedMemory::copyBytesTo(pImpl->IRQgetBuf1(),data,written);
            epr.IRQsetTxDataSize1(written);
        } else {
            written=min<unsigned int>(size,pImpl->IRQgetSizeOfBuf0());
            SharedMemory::copyBytesTo(pImpl->IRQgetBuf0(),data,written);
            epr.IRQsetTxDataSize0(written);
        }
        epr.IRQtoggleDtogRx();
        /*
         * This is a quirk of the stm32 peripheral: when the double buffering
         * feature is enabled, and the endpoint is set to valid, the peripheral
         * assumes that both buffers are filled with valid data, but this is
         * not the case. When IRQwrite is first called only one buffer is
         * filled. If the host issued three IN transactions immediately after
         * the IRQwrite and before any other IRQwrite, it will get first the
         * buffer, and then two transactions with zero bytes. Unfortunately,
         * I have no idea how to fix this.
         */
        epr.IRQsetTxStatus(EndpointRegister::VALID);
    }
    Tracer::IRQtrace(Ut::IN_BUF_FILL,pImpl->IRQgetData().epNumber,written);
    return true;
}

bool Endpoint::IRQread(unsigned char *data, int& readBytes)
{
    readBytes=0;
    if(pImpl->IRQgetData().enabledOut==0) return false;
    EndpointRegister& epr=USB->endpoint[pImpl->IRQgetData().epNumber];
    EndpointRegister::Status stat=epr.IRQgetRxStatus();
    if(stat==EndpointRegister::STALL) return false;

    if(pImpl->IRQgetData().type==Descriptor::INTERRUPT)
    {
        //INTERRUPT
        if(stat!=EndpointRegister::NAK) return true; //No errors, just no data
        readBytes=epr.IRQgetReceivedBytes();
        SharedMemory::copyBytesFrom(data,pImpl->IRQgetOutBuf(),readBytes);
        epr.IRQsetRxStatus(EndpointRegister::VALID);
    } else {
        //BULK
        if(pImpl->IRQgetBufferCount()==0) return true; //No errors, just no data
        pImpl->IRQdecBufferCount();
        if(epr.IRQgetDtogTx()) //Actually, SW_BUF
        {
            readBytes=epr.IRQgetReceivedBytes1();
            SharedMemory::copyBytesFrom(data,pImpl->IRQgetBuf1(),readBytes);
        } else {
            readBytes=epr.IRQgetReceivedBytes0();
            SharedMemory::copyBytesFrom(data,pImpl->IRQgetBuf0(),readBytes);
        }
        epr.IRQtoggleDtogTx();
    }
    Tracer::IRQtrace(Ut::OUT_BUF_READ,pImpl->IRQgetData().epNumber,readBytes);
    return true;
}

//
// class Callbacks
//

void Callbacks::IRQendpoint(unsigned char epNum, Endpoint::Direction dir) {}

void Callbacks::IRQstateChanged() {}

void Callbacks::IRQconfigurationChanged() {}

void Callbacks::IRQsuspend() {}

void Callbacks::IRQresume() {}

void Callbacks::IRQreset() {}

Callbacks::~Callbacks() {}

void Callbacks::setCallbacks(Callbacks* callback)
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

Callbacks Callbacks::defaultCallbacks;
Callbacks *Callbacks::callbacks=&defaultCallbacks;

//
// class USBdevice
//

bool USBdevice::enable(const unsigned char *device,
            const unsigned char * const * configs,
            const unsigned char * const * strings,
            unsigned char numStrings)
{
    Tracer::init();
    if(DefCtrlPipe::registerAndValidateDescriptors(
            device,configs,strings,numStrings)==false) return false;

    #ifdef _MIOSIX
    {
    InterruptDisableLock dLock; //The easy way
    #else //_MIOSIX
    __disable_irq();
    #endif //_MIOSIX
    
    //Configure gpio for USB pullup
    USBgpio::init();
    
    //Enable clock to USB peripheral
    if(SystemFrequency==72000000)
        RCC->CFGR &= ~RCC_CFGR_USBPRE; //Prescaler=1.5 (72MHz/1.5=48MHz)
    else if(SystemFrequency==48000000)
        RCC->CFGR |= RCC_CFGR_USBPRE;  //Prescaler=1   (48MHz)
    else {
        //USB can't work with other clock frequencies
        #ifndef _MIOSIX
        __enable_irq();
        #endif //_MIOSIX
        return false;
    }
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;

    //Connect pull-up to vcc
    USBgpio::enablePullup();

    USB->CNTR=USB_CNTR_FRES; //Clear PDWN, leave FRES asserted
    delayUs(1);  //Wait till USB analog circuitry stabilizes
    USB->CNTR=0; //Clear FRES too, USB peripheral active
    USB->ISTR=0; //Clear interrupt flags
    
    //First thing the host does is reset, so wait for that interrupt only
    USB->CNTR=USB_CNTR_RESETM;
    DeviceStateImpl::IRQsetState(USBdevice::DEFAULT);

    //Configure interrupts
    NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
    NVIC_SetPriority(USB_HP_CAN1_TX_IRQn,3);//Higher priority (Max=0, min=15)
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn,4);//High priority (Max=0, min=15)

    #ifdef _MIOSIX
    }
    #else //_MIOSIX
    __enable_irq();
    #endif //_MIOSIX
    return true;
}

void USBdevice::disable()
{
    #ifdef _MIOSIX
    {
    InterruptDisableLock dLock;
    #else //_MIOSIX
    __disable_irq();
    #endif //_MIOSIX

    USBgpio::disablePullup();
    for(int i=1;i<NUM_ENDPOINTS;i++) EndpointImpl::get(i)->IRQdeconfigure(i);
    SharedMemory::reset();
    USB->DADDR=0;  //Clear EF bit
    USB->CNTR=USB_CNTR_PDWN | USB_CNTR_FRES;
    USB->ISTR=0; //Clear interrupt flags
    RCC->APB1ENR &= ~RCC_APB1ENR_USBEN;
    DeviceStateImpl::IRQsetState(USBdevice::DEFAULT);
    DeviceStateImpl::IRQsetConfiguration(0);
    #ifdef _MIOSIX
    }
    #else //_MIOSIX
    __enable_irq();
    #endif //_MIOSIX
    Tracer::shutdown();
}

USBdevice::State USBdevice::getState()
{
    return DeviceStateImpl::getState();
}

unsigned char USBdevice::getConfiguration()
{
    return DeviceStateImpl::getConfiguration();
}

void USBdevice::waitUntilConfigured()
{
    DeviceStateImpl::waitUntilConfigured();
}

bool USBdevice::isSuspended()
{
    return DeviceStateImpl::isSuspended();
}

} //namespace mxusb

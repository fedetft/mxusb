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

#include "usb.h"
#include "endpoint_reg.h"
#include "stm32_usb_regs.h"

#ifdef _MIOSIX
#include "kernel/kernel.h"
#include "kernel/sync.h"
#include "kernel/scheduler/scheduler.h"
#endif //_MIOSIX

#ifndef USB_IMPL_H
#define	USB_IMPL_H

namespace mxusb {

/**
 * \internal
 * Implemenation class for Endpoint facade class.
 * It contains all what should be accessible from within the mxgui library,
 * but not accessible by user code.
 */
class EndpointImpl
{
public:
    /**
     * \internal
     * Endpoint data, subject to these restrictions:
     * - Type can be Descriptor::BULK or Descriptor::INTERRUPT only
     * - If type is Descriptor::BULK only enabledIn or enabledOut can be @ 1,
     *   while if Descriptor::INTERRUPT both IN and OUT side can be enabled.
     * - epNumber!=0
     */
    struct EpData
    {
        EpData(): enabledIn(0), enabledOut(0), type(0), epNumber(1) {}

        unsigned int enabledIn:1;  ///< 1 if IN  side of endpoint enabled
        unsigned int enabledOut:1; ///< 1 if OUT side of endpoint enabled
        unsigned int type:2;       ///< Contains a Descriptor::Type enum
        unsigned int epNumber:4;   ///< Endpoint number
    };
    
    /**
     * Allows to access an endpoint.
     * \param epNum Endpoint number, must be in range 1<=epNum<maxNumEndpoints()
     * or behaviour is undefined.
     * \return the endpoint class that allows to read/write from that endpoint
     */
    static EndpointImpl *get(unsigned char epNum)
    {
        if(epNum==0 || epNum>=NUM_ENDPOINTS) return &invalidEp;
        return &endpoints[epNum-1];
    }

    /**
     * Allows to access an endpoint within IRQ or with interrupts disabled.
     * \param epNum Endpoint number, must be in range 1<=epNum<maxNumEndpoints()
     * or behaviour is undefined.
     * \return the endpoint class that allows to read/write from that endpoint
     */
    static EndpointImpl *IRQget(unsigned char epNum) { return get(epNum); }

    /**
     * Wake thread waiting on IN side of an endpoint, if there is one.
     */
    void IRQwakeWaitingThreadOnInEndpoint()
    {
        #ifdef _MIOSIX
        using namespace miosix;
        if(waitIn==0) return;
        waitIn->IRQwakeup();
        if(waitIn->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
                Scheduler::IRQfindNextThread();
        waitIn=0;
        #endif //_MIOSIX
    }

    /**
     * Wake thread waiting on OUT side of an endpoint, if there is one.
     */
    void IRQwakeWaitingThreadOnOutEndpoint()
    {
        #ifdef _MIOSIX
        using namespace miosix;
        if(waitOut==0) return;
        waitOut->IRQwakeup();
        if(waitOut->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
                Scheduler::IRQfindNextThread();
        waitOut=0;
        #endif //_MIOSIX
    }

    #ifdef _MIOSIX
    /**
     * Set thread waiting on IN side of an endpoint.
     */
    void IRQsetWaitingThreadOnInEndpoint(miosix::Thread *t) { waitIn=t; }

    /**
     * Set thread waiting on OUT side of an endpoint.
     */
    void IRQsetWaitingThreadOnOutEndpoint(miosix::Thread *t) { waitOut=t; }
    #endif //_MIOSIX

    /**
     * \return buf1 for double buffered BULK endpoints
     */
    shmem_ptr IRQgetBuf1() const { return buf1; }

    /**
     * \return size of buf1 for double buffered BULK endpoints
     */
    unsigned char IRQgetSizeOfBuf1() const { return size1; }

    /**
     * \return OUT buffer for INTERRUPT endpoints
     */
    shmem_ptr IRQgetOutBuf() const { return buf1; }

    /**
     * \return size of OUT buffer for INTERRUPT endpoints
     */
    unsigned char IRQgetSizeOfOutBuf() const { return size1; }

    /**
     * \return size of OUT buffer for INTERRUPT endpoints
     */
    unsigned char getSizeOfOutBuf() const { return size1; }

    /**
     * \return buf0 for double buffered BULK endpoints
     */
    shmem_ptr IRQgetBuf0() const { return buf0; }

    /**
     * \return size of buf0 for double buffered BULK endpoints
     */
    unsigned char IRQgetSizeOfBuf0() const { return size0; }

    /**
     * \return IN buffer for INTERRUPT endpoints
     */
    shmem_ptr IRQgetInBuf() const { return buf0; }

    /**
     * \return size of IN buffer for INTERRUPT endpoints
     */
    unsigned char IRQgetSizeOfInBuf() const {  return size0; }

    /**
     * \return size of IN buffer for INTERRUPT endpoints
     */
    unsigned char getSizeOfInBuf() const { return size0; }

    /**
     * \return endpoint data
     */
    EpData IRQgetData() const { return data; }

    /**
     * \return endpoint data. Can be called with interrupts enabled.
     */
    EpData getData() const { return data; }

    /**
     * \return number of full buffers, used to understand how many buffers
     * are full/empty with double buffered BULK endpoints.
     */
    unsigned char IRQgetBufferCount() const { return bufCount; }

    /**
     * Increment number of used buffers
     */
    void IRQincBufferCount()
    {
        bufCount++;
    }

    /**
     * Decrement number of used buffers
     */
    void IRQdecBufferCount()
    {
        if(bufCount!=0) bufCount--;
    }

    /**
     * Deconfigure all endpoints
     */
    static void IRQdeconfigureAll();

    /**
     * Configure all endpoints
     * \param desc configuration descriptor
     */
    static void IRQconfigureAll(const unsigned char *desc);

    /**
     * Deconfigure this endpoint.
     * \param epNum the number of this endpoint, used to ser data.epNumber
     */
    void IRQdeconfigure(int epNum);

    /**
     * Configure this endpoint given an endpoint descriptor.
     * \param desc endpoint descriptor
     */
    void IRQconfigure(const unsigned char *desc);

private:
    EndpointImpl(const EndpointImpl&);
    EndpointImpl& operator= (const EndpointImpl&);

    #ifdef _MIOSIX
    EndpointImpl(): data(), size0(0), size1(0), buf0(0), buf1(0),
            waitIn(0), waitOut(0) {}
    #else //_MIOSIX
    EndpointImpl(): data(), size0(0), size1(0), buf0(0), buf1(0) {}
    #endif //_MIOSIX

    /**
     * Called by IRQconfigure() to set up an Interrupt endpoint
     * \param desc Must be the descriptor of an Interrupt endpoint
     */
    void IRQconfigureInterruptEndpoint(const unsigned char *desc);

    /**
     * Called by IRQconfigure() to set up an Bulk endpoint
     * \param desc Must be the descriptor of a Bulk endpoint
     */
    void IRQconfigureBulkEndpoint(const unsigned char *desc);

    // Note: size0 and size1 are unsigned char because the stm32 has a full
    // speed USB peripheral, so max buffer size for and endpoint is 64bytes

    EpData data;            ///< Endpoint data (status, type, number)
    unsigned char bufCount; ///< Buffer count, used for double buffered BULK
    unsigned char size0;    ///< Size of buf0 (if type==BULK size0==size1)
    unsigned char size1;    ///< Size of buf1 (if type==BULK size0==size1)
    shmem_ptr buf0;         ///< IN  buffer for INTERRUPT, buf0 for BULK
    shmem_ptr buf1;         ///< OUT buffer for INTERRUPT, buf1 for BULK

    #ifdef _MIOSIX
    miosix::Thread *waitIn;  ///< Thread waiting on IN side
    miosix::Thread *waitOut; ///< Thread waiting on OUT side
    #endif //_MIOSIX

    static EndpointImpl endpoints[NUM_ENDPOINTS-1];
    static EndpointImpl invalidEp; //Invalid endpoint, always disabled
};

/**
 * \internal
 * Implemenation class for DeviceState facade class.
 * It contains all what should be accessible from within the mxgui library,
 * but not accessible by user code.
 */
class DeviceStateImpl
{
public:
    /**
     * \return current device state.
     * Can be called both when interrupts are disabled or not.
     */
    static USBdevice::State getState() { return state; }

    /**
     * \internal
     * Set device state. If newstate!=oldstate calls the state change callback
     * \param s new state
     */
    static void IRQsetState(USBdevice::State s);

    /**
     * \return The configuration that the USB host has selected.
     * Zero means unconfigured.
     * Can be called both when interrupts are disabled or not.
     */
    static unsigned char getConfiguration() { return configuration; }

    /**
     * \internal
     * Set device configuration. Also calls configuration change callabck.
     * Note that the callback is called even if oldconfig==config.
     * Can be called only when getState()==CONFIGURED otherwise it does nothing
     * \param c configuration
     */
    static void IRQsetConfiguration(unsigned char c)
    {
        //Important: configuration is assigned before the callback is called
        //because if the callback calls IRQgetConfiguration() it must see the
        //new configuration number
        configuration=c;
        Callbacks::IRQgetCallbacks()->IRQconfigurationChanged();
    }

    /**
     * Wait until device is configured. If device is already configured, it
     * does nothing.
     */
    static void waitUntilConfigured()
    {
        #ifdef _MIOSIX
        miosix::InterruptDisableLock dLock;
        for(;;)
        {
            if(state==USBdevice::CONFIGURED) return;
            configWaiting=miosix::Thread::IRQgetCurrentThread();
            configWaiting->IRQwait();
            {
                miosix::InterruptEnableLock eLock(dLock);
                miosix::Thread::yield(); //The wait becomes effective
            }
        }
        #else //_MIOSIX
        while(state!=USBdevice::CONFIGURED) ;
        #endif //_MIOSIX
    }

    /**
     * Set suspend state
     * \param susp true if suspended
     */
    static void IRQsetSuspended(bool susp) { suspended=susp; }

    /**
     * \return true if suspended
     */
    static bool isSuspended() { return suspended; }

private:
    DeviceStateImpl();

    static volatile USBdevice::State state; ///< Current device state
    static volatile unsigned char configuration; ///< Current device config
    static volatile bool suspended; ///< True if suspended
    #ifdef _MIOSIX
    static miosix::Thread *configWaiting;
    #endif //_MIOSIX
};

}

#endif //USB_IMPL_H

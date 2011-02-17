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
#include "ep0.h"
#include "stm32_usb_regs.h"

#ifndef ENDPOINT_ZERO_H
#define	ENDPOINT_ZERO_H

namespace mxusb {

/**
 * \internal
 * This class is intended to implement the state machine to correctly handle
 * control transactions on the default control pipe (endpoint zero),
 * to allow enumeration of the USB device
 */
class DefCtrlPipe
{
public:
    /**
     * Called to register a set of descriptors during USB device initialization
     * \param device device descriptor
     * \param configs array of configuration descriptor. Its size is inferred by
     * the bNumConfigrations value in the device descriptor
     * \param strings array of string descriptors
     * \param numStrings size of the array of string descriptors
     * \return true if the edpoints are well-formed
     */
    static bool registerAndValidateDescriptors(const unsigned char *device,
            const unsigned char * const * configs,
            const unsigned char * const * strings,
            unsigned char numStrings);

    /**
     * Called by the USB interrupt handler when a setup packet is received
     * on endpoint zero
     */
    static void IRQsetup();

    /**
     * Called by the USB interrupt handler when an in packet is received
     * on endpoint zero
     */
    static void IRQin();

    /**
     * Called by the USB interrupt handler when an out packet is received
     * on endpoint zero
     */
    static void IRQout();

    /**
     * Allows custom requests to set a buffer for data transfer
     * \param data data buffer whose size must be at least wLength.
     * If data transfer is IN, the buffer must contain the data to send to
     * the host, else must be an empty buffer which will b filled by the host.
     */
    static void IRQsetDataForCustomRequest(unsigned char *data)
    {
        controlState.ptr=data;
        controlState.size=setup.wLength;
    }

    /**
     * Set default status for endpoint zero.
     * This configures endpoint zero as CONTROL endpoint, setting tx/rx buffers.
     * In particular, both endpoint directions are set to STALL.
     * At first, setting rx endpoint to stall might seem wrong, because we need
     * to receive SETUP packets. But the stm32 doc says that a SETUP is
     * acknowledged even if the endpoint is in stall as long as another SETUP
     * request is not already pending. This allows us to correctly stall if the
     * host sends an OUT without a SETUP, but still accept the SETUP.
     */
    static void IRQdefaultStatus();

    /**
     * By default, endpoint zero is set to stall on both directions, but when
     * a request is received, they might become valid after processing the
     * request. Therefore this member function allows to set both directions
     * to nak so that if the host attempts a tx/rx too soon, it will be told
     * to retry till the endpoint status changes to either valid or stall.
     * It will also set the untouched flag on both tx and rx directions
     * (see IRQrestoreStatus)
     */
    static void IRQstatusNak()
    {
        USB->endpoint[0].IRQsetTxStatus(EndpointRegister::NAK);
        USB->endpoint[0].IRQsetRxStatus(EndpointRegister::NAK);
        txUntouchedFlag=true;
        rxUntouchedFlag=true;
    }

    /**
     * This member function solves the issue of setting the tx and rx endpoint
     * direction to stall <b>if</b> the endpoint 0 handling code hasn't set the
     * the endpoint to valid. It works like this: when IRQstatusNak() is called
     * an "untouched" flag is set for both tx and rx directions, then endpoint
     * 0 handling code is called. If that code has to set a direction to valid,
     * it calls IRQsetEp0[Tx|Rx]Valid() which clears the untouched flag for that
     * direction. Therefore, this member function will simply set to STALL the
     * directions where the untouched flag is still set.
     */
    static void IRQrestoreStatus();

    /**
     * Get a configuration descriptor
     * \param config a valid configuration descriptor number.
     * Behaviour is undefined if config is not within 0 and bNumConfigrations
     * \return pointer to the configuration descriptor
     */
    static const unsigned char *IRQgetConfigDesc(unsigned char config)
    {
        return configDesc[config];
    }

private:

    /**
     * Possible states for the state machine that controls control transfers
     */
    enum ControlState
    {
        CTR_NO_REQ_PENDING, ///<Currently no control transaction ongoing
        //CTR_OUT_IN_PROGRESS,
        CTR_OUT_STATUS,     ///<IN packet with size 0 ready for host to get
        CTR_IN_IN_PROGRESS, ///<Transmitting to host, there's still data left
        CTR_IN_STATUS_BEGIN,///<Last data loaded to SharedMemory for host to get
        CTR_IN_STATUS_END,  ///<Waiting for OUT packet with size 0 from host
        CTR_SET_ADDRESS,    ///<Same as CTR_OUT_STATUS, but adress will change
        CTR_CUSTOM_OUT_IN_PROGRESS///< OUT in progress, for custom requests
    };

    /**
     * Control state machine, to handle control transfers
     */
    struct ControlStateMachine
    {
        ///Current state
        ControlState state;
        ///Pointer to data yet to send.
        ///Valid iff state==(CTR_IN_IN_PROGRESS | CTR_CUSTOM_OUT_IN_PROGRESS)
        unsigned char *ptr;
        ///Number of bytes yet to send.
        ///Valid iff state==(CTR_IN_IN_PROGRESS | CTR_CUSTOM_OUT_IN_PROGRESS)
        unsigned short size;
    };

    /**
     * Code for endpoint 0 handling must call this function instead of
     * USB->endpoint[0].setTxStatus(Endpoint::VALID), since this member function
     * clears the untouched flag on tx direction
     */
    static void IRQsetEp0TxValid();

    /**
     * Code for endpoint 0 handling must call this function instead of
     * USB->endpoint[0].setRxStatus(Endpoint::VALID), since this member function
     * clears the untouched flag on rx direction
     */
    static void IRQsetEp0RxValid();

    /**
     * Handles the GET_DESCRIPTOR request
     */
    static void IRQgetDescriptor();

    /**
     * Handles the SET_ADDRESS request
     */
    static void IRQsetAddress();

    /**
     * Handles the GET_CONFIGURATION request
     */
    static void IRQgetConfiguration();

    /**
     * Handles the SET_CONFIGURATION request
     */
    static void IRQsetConfiguration();

    /**
     * Handles the GET_STATUS request
     */
    static void IRQgetStatus();

    /**
     * Validate a configuration endpoint
     * \param config configuration endpoint
     * \param num expected configuration number
     * \return false if errors
     */
    static bool validateConfigEndpoint(const unsigned char *config, int num);

    /**
     * Called when a setup packet is received, initiate the data stage of a
     * control transfer
     * \param data pointer to data to be sent
     * \param size length of data to be sent
     */
    static void IRQstartInData(const unsigned char *data, unsigned short size);

    /**
     * Called when a class/vendor OUT packet is received.
     * Do not use for standard OUT requests, as at the end it calls the
     * custom OUT callback.
     */
    static void IRQstartCustomOutData();

    static const unsigned char *deviceDesc; ///<Pointer to device descriptor
    static const unsigned char * const * configDesc; ///<Array of config desc
    static const unsigned char * const * stringsDesc;  ///<Array of string desc
    static unsigned char numStringDesc; ///<Size of stringDesc
    static ControlStateMachine controlState; ///<State machine for endpoint 0
    static Setup setup; ///<Currently received setup packet
    static bool txUntouchedFlag; ///<Used to set ep0 to nak while processing
    static bool rxUntouchedFlag; ///<Used to set ep0 to nak while processing
    /// There is a timing condition that might cause problems with data transfer
    /// on endpoint zero, consider what happens if an IN packet is received
    /// *immediately* after an OUT packet, what should happen is this:
    /// the interrupt routine calls IRQstatusNak() after the OUT packet,
    /// so endpoint zero responds NAK to the IN packet indicating it's
    /// processing the previous request, then IRQsetEp0TxValid() is called and
    /// the IN request is accepted.
    /// What happens is this: since the IN packet arrives *immediately*
    /// after the OUT packet, it arrives before the interrupt routine has
    /// called IRQstatusNak(), and because of that it gets STALLed. Error.
    /// Setting both endpoint directions to NAK should be done in hardware,
    /// but on the stm32 it is not done. This problem arises when the CPU is
    /// running very slow, for example when it is executing code from external
    /// RAM. It might also happen when the CPU is busy handling interrupts
    /// with a higher priority that the USB interrupt.
    /// For places where it is known that this might happen, setting this
    /// variable temporarily to true causes IRQrestoreStatus() to NAK instead
    /// of STALL. It is not a clean fix, but it's the best I've found.
    static bool fixForStallTiming;
};

} //namespace mxusb

#endif //ENDPOINT_ZERO_H

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

#ifndef EP0_H
#define	EP0_H

namespace mxusb {

/**
 * Format of a setup packet
 */
struct Setup
{
    unsigned char bmRequestType; ///< Request type, direction and recipient
    unsigned char bRequest;      ///< Request code
    unsigned short wValue;       ///< A two byte parameter
    unsigned short wIndex;       ///< A two byte parameter
    unsigned short wLength;      ///< Length of data stage

    /**
     * Flags for the bmRequestType field
     */
    enum bmRequest
    {
        DIR_MASK=0x80,        ///< Used to mask only the direction field
        DIR_IN=0x80,          ///< Device to host

        TYPE_MASK=3<<5,       ///< Used to mask only the type field
        TYPE_STANDARD=0<<5,   ///< Standard request type
        TYPE_CLASS=1<<5,      ///< Class request type
        TYPE_VENDOR=2<<5,     ///< Vendor request type

        RECIPIENT_MASK=0x1f,  ///< Used to mask only the recipient field
        RECIPIENT_DEVICE=0,   ///< Recipient is device
        RECIPIENT_INTERFACE=1,///< Recipient is interface
        RECIPIENT_ENDPOINT=2, ///< Recipient is endpoint
        RECIPIENT_OTHER=3     ///< Recipient is other
    };

    /**
     * Possible values for the bRequestType field
     */
    enum RequestCodes
    {
        GET_STATUS=0,        ///< GET_STATUS
        CLEAR_FEATURE=1,     ///< CLEAR_FEATURE
        SET_FEATURE=3,       ///< SET_FEATURE
        SET_ADDRESS=5,       ///< SET_ADDRESS
        GET_DESCRIPTOR=6,    ///< GET_DESCRIPTOR
        SET_DESCRIPTOR=7,    ///< SET_DESCRIPTOR
        GET_CONFIGURATION=8, ///< GET_CONFIGURATION
        SET_CONFIGURATION=9, ///< SET_CONFIGURATION
        GET_INTERFACE=10,    ///< GET_INTERFACE
        SET_INTERFACE=11,    ///< SET_INTERFACE
        SYNCH_FRAME=12       ///< SYNCH_FRAME
    };

    /**
     * Passed as wValue when bRequest==SET/CLEAR_FEATURE
     */
    enum FeatureSelector
    {
        DEVICE_REMOTE_WAKEUP=1, ///< Targeted to whole device
        ENDPOINT_HALT=0,        ///< Targeted to single endpoints
        TEST_MODE=2             ///< Targeted to whole device
    };
};

/**
 * By making a class that derives from this it is possible to handle class and
 * vendor specific request on the default control pipe (endpoint zero).
 * These callbacks are called from the USB interrupt handler, and therefore are
 * subject to these restrictions:
 * - Keep execution time as low as possible, to avoid increasing interrupt
 *   latency for other interrupts
 * - Do not call any kernel service that has not been designed to run from
 *   within an IRQ. For example, do not printf, create threads or sleep
 * - Do not allow C++ exceptions to propagate through these callbacks
 * - Never call these callback member functions yourself, not even when
 *   interrupts are disabled.
 */
class EndpointZeroCallbacks
{
public:

    /**
     * This callback is called when a class or vendor request is received on
     * endpoint zero. If setup.wLength>0, a data stage is required and in this
     * case the callback must either return false or call IRQsetDataBuffer()
     * to set up the buffer used for the data stage.
     * \param setup the associated setup request
     * \return if this is an expected request for the device being built,
     * return true. This means the request will be accepted, otherwise return
     * false and the request will be STALLed.
     */
    virtual bool IRQsetup(const Setup *setup)=0;

    /**
     * This callback is called after an expected setup request is received
     * (IRQsetup() callback called, and user code returned true), and the data
     * stage is completed. If there is no data stage (setup.wLenght==0)
     * or the request was of IN (device to host) type, then this callback
     * won't be called.
     * If no IN transaction with data stage is expected as valid, there is no
     * need to override this member function.
     * \param setup the same setup request passed to the previous IRQsetup()
     * call
     * \return true if the data received in the buffer was correct. In this
     * case the status stage of the transaction will be ACKed, otherwise
     * a STALL will be returned to the host.
     */
    virtual bool IRQendOfOutDataStage(const Setup *setup);

    /**
     * Destructor
     */
    virtual ~EndpointZeroCallbacks();

    /**
     * Set data buffer for setup requests that require it. It is meant to be
     * called from within IRQsetup() if setup.wLength>0. Other uses may produce
     * undefined behaviour. Do not allocate the buffer on the stack, since it
     * will be accessed after IRQsetup() returns.
     * \param data data to send to the host, if the setup transaction is of
     * type IN, else pointer to an empty buffer that will be filled by the host.
     * Buffer length must be at least setup.wLength.
     */
    static void IRQsetDataBuffer(unsigned char *data);

    /**
     * Set callbacks for USB nonstandard requests on endpoint zero.
     * \param callback an instance of a class that derives from
     * EndpoinZeroCallbacks, or NULL to disable the callbacks.
     * If a previous callback was set, the object will not be deleted,
     * so if it was allocated on the heap, user code is responsible for
     * object deallocation.
     */
    static void setCallbacks(EndpointZeroCallbacks *callback);

    /**
     * Must be called with interrupts disabled. When interrupts are re-enabled
     * a thread can call setCallbacks(), so caching the result is not allowed.
     * \return the current callbacks class registered for USB event handling
     */
    static EndpointZeroCallbacks *IRQgetCallbacks() { return callbacks; }

private:
    ///Pointer to currently active callbacks
    static EndpointZeroCallbacks *callbacks;
};

} //namespace mxusb

#endif //EP0_H

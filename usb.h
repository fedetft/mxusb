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

#ifndef USB_H
#define	USB_H

namespace mxusb {

//Forward declaration
class EndpointImpl;

/**
 * Every endpoint, except endpoint zero, have an associated Endpoint class
 * that allows user code to read/write data to that endpoint.
 * Beware that endpoints can change their status (enabled/disabled/direction)
 * at any time because of a SET_CONFIGURATION issued by the host.
 */
class Endpoint
{
public:
    /**
     * Possible endpoint directions
     */
    enum Direction
    {
        IN=0x80, ///< Device to host
        OUT=0    ///< Host to device
    };

    /**
     * Allows to access an endpoint.
     * \param epNum Endpoint number, must be in range 1<=epNum<maxNumEndpoints()
     * or behaviour is undefined.
     * \return the endpoint class that allows to read/write from that endpoint
     */
    static Endpoint get(unsigned char epNum);

    /**
    * Allows to access an endpoint from an IRQ or when interrupts are disabled.
     * \param epNum Endpoint number, must be in range 1<=epNum<maxNumEndpoints()
     * or behaviour is undefined.
     * \return the endpoint class that allows to read/write from that endpoint
     */
    static Endpoint IRQget(unsigned char epNum) { return get(epNum); }

    /**
     * \return the maximum number of endpoints that the the USB peripheral
     * supports.
     */
    static const int maxNumEndpoints();

    /**
     * \return true if IN side of the endpoint is enabled.
     * Before the host selects a configuration all endpoints are disabled.
     */
    bool isInSideEnabled() const;

    /**
     * \return true if IN side of the endpoint is enabled.
     * Before the host selects a configuration all endpoints are disabled.
     */
    bool IRQisInSideEnabled() const { return isInSideEnabled(); }

    /**
     * \return true if OUT side of the endpoint is enabled.
     * Before the host selects a configuration all endpoints are disabled.
     */
    bool isOutSideEnabled() const;

    /**
     * \return true if OUT side of the endpoint is enabled.
     * Before the host selects a configuration all endpoints are disabled.
     */
    bool IRQisOutSideEnabled() const { return isOutSideEnabled(); }

    /**
     * \return size of the buffer used for the IN side of the endpoint,
     * as specified in the endpoint descriptor.
     * Returned value is meaningful only when the endpoint side is enabled.
     */
    unsigned short inSize() const;

    /**
     * \return size of the buffer used for the IN side of the endpoint,
     * as specified in the endpoint descriptor.
     * Returned value is meaningful only when the endpoint side is enabled.
     */
    unsigned short IRQinSize() const { return inSize(); }

    /**
     * \return size of the buffer used for the IN side of the endpoint,
     * as specified in the endpoint descriptor.
     * Returned value is meaningful only when the endpoint side is enabled.
     */
    unsigned short outSize() const;

    /**
     * \return size of the buffer used for the IN side of the endpoint,
     * as specified in the endpoint descriptor.
     * Returned value is meaningful only when the endpoint side is enabled.
     */
    unsigned short IRQoutSize() const { return outSize(); }

    /**
     * Write data to an endpoint. Enpoint IN side must be enabled.
     * This is a blocking call that won't return until all data has been
     * copied into the write buffer or an error is encountered.
     * Because of the existence of a buffer, when the function returns some data
     * might still be in the buffer waiting for the host to read it.<br>
     * Only one thread at a time can call write on an endpoint. If two or more
     * threads call this function on the same endpoint, the behaviour is
     * undefined. Two thread one calling read and one calling write are allowed.
     * \param data data to write
     * \param size size of data to write
     * \param written number of bytes actually written. If no errors happened
     * it should be equal to size. User code should inspect written in
     * case of errors to know the number of bytes written before the error.
     * \return false in case of errors, or if the host suspended/reconfigured
     * the device
     */
    bool write(const unsigned char *data, int size, int& written);

    /**
     * Read data from an endpoint. Enpoint OUT side must be enabled.
     * This is a blocking call that won't return until data has been read
     * or an error is encountered.<br>
     * Only one thread at a time can call write on an endpoint. If two or more
     * threads call this function on the same endpoint, the behaviour is
     * undefined. Two thread one calling read and one calling write are allowed.
     * \param data buffer where read data is stored.
     * Buffer size must be at least Endpoint::outSize()
     * \param readBytes number of bytes actually read. User code should
     * inspect readBytes even in case of errors, since some bytes might be read
     * before the error.
     * \return false in case of errors, or if the host suspended/reconfigured
     * the device
     */
    bool read(unsigned char *data, int& readBytes);

    /**
     * Write data to an endpoint. Enpoint IN side must be enabled.
     * This is a nonblocking call that returns immediately. It must be called
     * with interrupts disabled or within an IRQ (such as a Callback).
     * Because of the existence of a buffer, when the function returns some data
     * might still be in the buffer waiting for the host to read it.
     * \param data data to write
     * \param size size of data to write.
     * \param written number of bytes actually written. Contrary to write()
     * even if no errors occurred, this value can be lower than size. User code
     * should inspect written in case of errors to know the number of bytes
     * written before the error.
     * \return false in case of errors, or if the host suspended/reconfigured
     * the device
     */
    bool IRQwrite(const unsigned char *data, int size, int& written);

    /**
     * Read data from an endpoint. Enpoint OUT side must be enabled.
     * This is a nonblocking call that returns immediately. It must be called
     * with interrupts disabled or within an IRQ (such as a Callback).
     * \param data buffer where read data is stored.
     * Buffer size must be at least Endpoint::outSize()
     * \param readBytes number of bytes actually read. User code should
     * inspect readBytes even in case of errors, since some bytes might be read
     * before the error.
     * \return false in case of errors, or if the host suspended/reconfigured
     * the device
     */
    bool IRQread(unsigned char *data, int& readBytes);

private: 
    /**
     * Private constructor
     * \param pImpl endpoint implementation
     */
    explicit Endpoint(EndpointImpl *pImpl) : pImpl(pImpl) {}

    EndpointImpl *pImpl;///< Endpoint imlementation
};

/**
 * By making a class that derives from this it is possible to handle USB
 * events. Override methods for the events you need to handle.
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
class Callbacks
{
public:

    /**
     * Called when an endpoint has completed a transfer (tx or rx, depending on
     * configuration). You <b>can</b> cause a context switch from within this
     * callback, by calling Scheduler::IRQfindNextThread();
     * \param epNum endpoint number
     * \param dir direction, endpoint direction
     */
    virtual void IRQendpoint(unsigned char epNum, Endpoint::Direction dir);

    /**
     * Called every time the device state changes, for example from
     * DeviceState::DEFAULT to DeviceState::ADDRESS. Don't cause context
     * switches from here, as this callback is not always called from within
     * an interrupt routine.
     */
    virtual void IRQstateChanged();

    /**
     * This callback is called right <b>before</b> device configuration changes.
     * It is useful for devices with multiple configurations that can be changed
     * on the fly by the host. To get the new configuration number it is
     * possible to call DeviceState::IRQgetConfiguration().
     * Don't cause context switches from here, as this callback is not always
     * called from within an interrupt routine.
     */
    virtual void IRQconfigurationChanged();

    /**
     * Called when the host suspends the device. You <b>can</b> cause a context
     * switch from within this callback, by calling
     * Scheduler::IRQfindNextThread();
     */
    virtual void IRQsuspend();

    /**
     * Called when the host resumes the device. You <b>can</b> cause a context
     * switch from within this callback, by calling
     * Scheduler::IRQfindNextThread();
     */
    virtual void IRQresume();

    /**
     * Called when the hosts resets the device. You <b>can</b> cause a context
     * switch from within this callback, by calling
     * Scheduler::IRQfindNextThread();
     */
    virtual void IRQreset();

    /**
     * Destructor
     */
    virtual ~Callbacks();

    /**
     * Set callbacks for USB events.
     * \param callback an instance of a class that derives from Callbacks, or
     * NULL to disable the callbacks. If a previous callback was set, the object
     * will not be deleted, so if it was allocated on the heap, user code is
     * responsible for object deallocation.
     */
    static void setCallbacks(Callbacks *callback);

    /**
     * Must be called with interrupts disabled. When interrupts are re-enabled
     * a thread can call setCallbacks(), so caching the result is not allowed.
     * \return the current callbacks class registered for USB event handling
     */
    static Callbacks *IRQgetCallbacks() { return callbacks; }

private:
    static Callbacks defaultCallbacks; ///<Default callbacks. They do nothing
    static Callbacks *callbacks; ///<Pointer to currently active callbacks
};

/**
 * Allows to configure the USB peripheral.
 */
class USBdevice
{
public:
    /**
     * Possible USB states
     */
    enum State
    {
        DEFAULT,   ///<Reached after an USB reset request
        ADDRESS,   ///<USB host assigned an address to device
        CONFIGURED ///<Device is configured
    };

    /**
     * Configure the USB peripheral, given the descriptors.
     * \param device device descriptor
     * \param configs array of configuration descriptor. Its size is inferred by
     * the bNumConfigrations value in the device descriptor
     * \param strings array of string descriptors
     * \param numStrings size of strings array
     * \return true if configuration succeeded
     */
    static bool enable(const unsigned char *device,
            const unsigned char * const * configs,
            const unsigned char * const * strings=0,
            unsigned char numStrings=0);

    /**
     * Disables the USB peripheral
     */
    static void disable();

    /**
     * \return current device state.
     */
    static State getState();

    /**
     * \return same as getState(), but can be called from IRQ or with interrupts
     * disabled.
     */
    static State IRQgetState() { return getState(); }

    /**
     * \return The configuration that the USB host has selected.
     * Zero means unconfigured.
     */
    static unsigned char getConfiguration();

    /**
     * \return same as getConfiguration(), but can be called from IRQ or with
     * interrupts disabled.
     */
    static unsigned char IRQgetConfiguration() { return getConfiguration(); }

    /**
     * This function returns as soon as the USB device is in the configured
     * state. If the device is already configured, it returns immediately.
     * If two or more threads call this function, the behaviour is undefined.
     */
    static void waitUntilConfigured();

    /**
     * \return true if device is suspended
     */
    static bool isSuspended();

private:
    USBdevice();
};

/**
 * Wrapper class for Descriptor constants
 */
class Descriptor
{
public:
    /**
     * Standard descriptor types
     */
    enum DescType
    {
        DEVICE=                   1, ///< Device descriptor
        CONFIGURATION=            2, ///< Configuration descriptor
        STRING=                   3, ///< String descriptor
        INTERFACE=                4, ///< Interface descriptor
        ENDPOINT=                 5, ///< Endpoint descriptor
        DEVICE_QUALIFIER=         6, ///< Device qualifier
        OTHER_SPEED_CONFIGURATION=7, ///< Other speed configuration
        INTERFACE_POWER=          8  ///< Interface power
    };

    /**
     * Size for the standard descriptors that are fixed size.
     * There is no constant for string descriptors because they are variable
     * sized.
     */
    enum DescSize
    {
        DEVICE_DESC_SIZE=18,       ///< Size of device descriptor
        CONFIGURATION_DESC_SIZE=9, ///< Size of configuration descriptor
        INTERFACE_DESC_SIZE=9,     ///< Size of interface descriptor
        ENDPOINT_DESC_SIZE=7       ///< Size of endpoint descriptor
    };

    /**
     * Describes the endpoint type as for the bmAttributes field of ENDPOINT
     * descriptors.
     */
    enum Type
    {
        TYPE_MASK=3,   ///<\internal Used for parsing endpoint type
        
        CONTROL=0,     ///< Control endpoint
        ISOCHRONOUS=1, ///< Isochronous endpoint
        BULK=2,        ///< Bulk endpoint
        INTERRUPT=3    ///<Interrupt endpoint
    };

private:
    Descriptor();
};

} //namespace mxusb

#endif //USB_H

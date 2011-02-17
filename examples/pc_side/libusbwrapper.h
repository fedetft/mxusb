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

#include <libusb.h>
#include <stdexcept>
#include <map>

#ifndef LIBUSBWRAPPER_H
#define LIBUSBWRAPPER_H

namespace libusb {

/**
 * RAII style class to prevent forgetting about calling
 * libusb_exit if an exception is thrown.
 */
class Context
{
public:
	/**
	 * Constructor, create an instance of a libusb context.
	 * \throws runtime_error in case of errors.
	 */
	Context();
	
	/**
	 * \internal
	 * \return the context
	 */
	libusb_context *get() const { return ctx; }
	
	/**
	 * Destructor, calls libusb_exit()
	 */
	~Context()
	{
		libusb_exit(ctx);
	}
	
private:
	//Non copyiable
	Context(const Context& );
	Context& operator= (const Context& );
	
	libusb_context *ctx; ///< The context
};

/**
 * Timeout when reading/writing to the USB device
 */
class TimeoutException : public std::runtime_error
{
public:
	/**
	 * \param bytesTransferred number of bytes transferred before the timeout
	 */
	TimeoutException(int bytesTransferred=0)
		: runtime_error("Timeout"), bytesTransferred(bytesTransferred) {}
	
	/**
	 * \return bytesTransferred number of bytes transferred before the timeout
	 */
	int getBytesTransferred() const { return bytesTransferred; }
	
private:
	int bytesTransferred;
};

/**
 * Wrapper for the two constants Endpoint::IN and Endpoint::OUT
 */
class Endpoint
{
public:
	static const int IN= LIBUSB_ENDPOINT_IN; ///< Device to host transfer
	static const int OUT=LIBUSB_ENDPOINT_OUT;///< Host to device transfer
private:
	Endpoint();//Just a wrapper class, disallow making instances
};

/**
 * USB device class
 */
class Device
{
public:
	/**
	 * Constructor. Device is not opened.
	 * \param context libusb context
	 */
	Device(Context& context)
			: context(context), handle(0), configuration(0),
			interfaces(), timeout(0) {}
	
	/**
	 * Constructor, device is opened.
	 * \param context libusb context
	 * \param vid device vid
	 * \param pid device pid
	 * \throws runtime_error if device could not be opened
	 */
	Device(Context& context, unsigned short vid, unsigned short pid)
			: context(context), handle(0), configuration(0),
			interfaces(), timeout(0)
	{
		this->open(vid,pid);
	}
	
	/**
	 * Open device
	 * \param vid device vid
	 * \param pid device pid
	 * \throws runtime_error if device could not be opened
	 */
	void open(unsigned short vid, unsigned short pid);
	
	/**
	 * \return true if the device is configured
	 * If the device is not already configured, you have to do so before
	 * claiming an interface
	 */
	bool isConfigured() const { return configuration!=0; }
	
	/**
	 * \return the currently active configuration.
	 * Coherently with USB spec, it returns 0 if the device is not configured
	 */
	int getConfiguration() const { return configuration; }
	
	/**
	 * Set configuration of the device (bConfigurationValue)
	 * Device must be open.
	 * \param config configuration. A value of zero unconfigures the device
	 * \throws logic_error if device was not open
	 * \throws runtime_error if configuration could not be set
	 */
	void setConfiguration(unsigned int config);
	
	/**
	 * Claim an interface.
	 * Device must be configured before calling this member function.
	 * You can claim more than one interface, but can't claim the same
	 * interface twice.
	 * If a kernel driver is active on that interface, an attempt to detach it
	 * is made. If it fails an exception is thrown. When the device class'
	 * destructor is called detached kernel drivers are reattached.
	 * \param iface interface to claim (bInterfaceNumber)
	 * \throws logic_error if device was not configured
	 * \throws runtime_error if interface could not be claimed, or if a
	 * kernel driver was active and detatch failed.
	 */
	void claimInterface(unsigned int iface);
	
	/**
	 * Explicitly release a claimed interface.
	 * Note that when the destructor of this class is called, or when
	 * close() is called, all interfaces are released.
	 * If a kernel driver was attached before the interface is claimed,
	 * it is reattached.
	 * \param iface interface to release
	 * \throws logic_error if interface was not claimed
	 * \throws runtime_error if interface could not be released, or if
	 * kernel driver reattach failed.
	 */
	void releaseInterface(unsigned int iface);
	
	/**
	 * Set alternate settings on an already claimed interface.
	 * \param iface already claimed interface
	 * \throws logic_error if interface was not claimed
	 * \throws runtime_error if operation failed
	 */
	void setAltSettings(unsigned int iface, unsigned int altSettings);
	
	/**
	 * Reset USB device. Device must be open.
	 * If fails, the device is closed.
	 * \throws logic_error if device was not open
	 * \throws runtime_error if reset failed (device is now closed)
	 */
	void reset();
	
	/**
	 * Clear the halt/stall condition for an endpoint.
	 * Device must be open.
	 * \param endpoint endpoint
	 * \throws logic_error if device was not open
	 * \throws runtime_error if operation failed
	 */
	void clearHalt(unsigned char endpoint);
	
	/**
	 * Set the timeout for read/write operations.
	 * The default is 0 (infinite timeout)
	 * \param ms timeout in milliseconds, or 0 for no timeout
	 */
	void setTimeout(unsigned int ms)
	{
		timeout=ms;
	}
	
	/**
	 * \return the timeout
	 */
	unsigned int getTimeout() const { return timeout; }
	
	/**
	 * Perform a control transfer.
	 * If bit #7 of bmRequestType is 1, this denotes an IN transfer
	 * (device to host), else an OUT transfer (host to device).
	 * Oring with Endpoint::IN or Endpoint::OUT can be used
	 * to specify direction.
	 * \param bmRequestType request type, see p276 of usb_20.pdf
	 * \param bRequest request field for setup packet, see p276 of usb_20.pdf
	 * \param wValue value field for setup packet, see p276 of usb_20.pdf
	 * \param wIndex index filed for setup packet, see p276 of usb_20.pdf
	 * \param data buffer for either input or output
	 * \param wLength buffer size, see p276 of usb_20.pdf
	 * \return number of bytes actually transferred
	 * \throws TimeoutException on timeout
	 * \throws logic_error if no configuration chosen
	 * \throws runtime_error if any other error
	 */
	int controlTransfer(unsigned char bmRequestType, unsigned char bRequest,
			unsigned short wValue, unsigned short wIndex,
			unsigned char *data, unsigned short wLength);
	
	/**
	 * Perform a bulk transfer.
	 * If bit #7 of endpoint is 1, this denotes an IN transfer
	 * (device to host), else an OUT transfer (host to device).
	 * Oring with Endpoint::IN or Endpoint::OUT can be used
	 * to specify direction.
	 * When reading, the buffer should have for size the maximum amount of
	 * bytes you expect to receive. If more data arrives, a runtime_error is
	 * thrown.
	 * \param endpoint endpoint number
	 * \param data buffer where data is read/written
	 * \param length buffer size
	 * \return the number of bytes actually read/written
	 * \throws TimeoutException on timeout
	 * \throws logic_error if no interface claimed
	 * \throws runtime_error if any other error
	 */
	int bulkTransfer(unsigned char endpoint, unsigned char *data, int length);
	
	/**
	 * Perform an interrupt transfer.
	 * If bit #7 of endpoint is 1, this denotes an IN transfer
	 * (device to host), else an OUT transfer (host to device).
	 * Oring with Endpoint::IN or Endpoint::OUT can be used
	 * to specify direction.
	 * When reading, the buffer should have for size the maximum amount of
	 * bytes you expect to receive. If more data arrives, a runtime_error is
	 * thrown.
	 * \param endpoint endpoint number
	 * \param data buffer where data is read/written
	 * \param length buffer size
	 * \return the number of bytes actually read/written
	 * \throws TimeoutException on timeout
	 * \throws logic_error if no interface claimed
	 * \throws runtime_error if any other error
	 */
	int interruptTransfer(unsigned char endpoint, unsigned char *data,
			int length);
	
	/**
	 * Close the device.<br>
	 * Also releases all the claimed interfaces, reattaching kernel drivers
	 * that were detached.
	 * \throws runtime_error if releasing interfaces failed
	 */
	void close();
	
	/**
	 * \internal
	 * \return the handle
	 */
	libusb_device_handle *get() const { return handle; }
	
	/**
	 * Destructor. 
	 */
	~Device()
	{
		try { close(); } catch(...) {}
	}
	
private:
	//Non copyiable
	Device(const Device& );
	Device& operator= (const Device& );
	
	/**
	 * Throws a runtime_error with given description and error code
	 * \param desc error description
	 * \param code error code
	 * \throws runtime_error, always
	 */
	static void throwRuntimeError(const std::string& desc, int code);

	/**
	 * Release all interfaces
	 * \return true if errors
	 */
	bool releaseAllInterfaces();
	
	Context& context; ///< Context
	libusb_device_handle *handle; ///< Handle, 0 if not open
	int configuration; ///< Configuration, 0 if not configured
	///Claimed interfaces, mapped to whether a kernel driver was active on them
	std::map<int,bool> interfaces;  
	unsigned int timeout; ///< Timeout for read/write operations
};

} //namespace libusb

#endif //LIBUSBWRAPPER_H

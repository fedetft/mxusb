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

#include "libusbwrapper.h"
#include <sstream>

namespace libusb {

//
// class Context
//

Context::Context()
{
	int error=libusb_init(&ctx);
	if(error!=0)
	{
		std::stringstream ss;
		ss<<"libusb_init: "<<error;
		throw(std::runtime_error(ss.str()));
	}
}

//
// class Device
//

void Device::open(unsigned short vid, unsigned short pid)
{
	if(handle!=0) close();
	//FIXME: use libusb_get_device_list instead?
	handle=libusb_open_device_with_vid_pid(context.get(),vid,pid);
	if(handle==0)
	{
		std::stringstream ss;
		ss<<"open(): Could not find "<<std::hex<<vid<<":"<<pid;
		throw(std::runtime_error(ss.str()));
	}
	int error=libusb_get_configuration(handle,&configuration);
	if(error!=0) throwRuntimeError("libusb_get_configuration",error);
}

void Device::setConfiguration(unsigned int config)
{
	if(handle==0) throw(std::logic_error("Not open"));
	
	if(config==configuration) return;
	if(releaseAllInterfaces()==true)
		throw(std::runtime_error("Failed releasing interfaces"));
	int error;
	if(config!=0)
	{
		error=libusb_set_configuration(handle,config);
	} else {
		//See documentation of libusb_set_configuration
		error=libusb_set_configuration(handle,-1);
	}
	if(error!=0) throwRuntimeError("libusb_set_configuration",error);
	configuration=config;
}

void Device::claimInterface(unsigned int iface)
{
	if(configuration==0) throw(std::logic_error("Not configured"));
	if(interfaces.find(iface)!=interfaces.end())
		throw(std::logic_error("Interface already claimed"));
	
	bool driverActive=false;
	int error=libusb_kernel_driver_active(handle,iface);
	if(error<0) throwRuntimeError("libusb_kernel_driver_active",error);
	if(error==1)
	{
		//A kernel driver is active, try to deactivate it
		error=libusb_detach_kernel_driver(handle,iface);
		if(error!=0)
			throwRuntimeError("libusb_detach_kernel_driver",error);
		driverActive=true;
	}
	
	error=libusb_claim_interface(handle,iface);
	if(error!=0)
	{
		if(driverActive) libusb_attach_kernel_driver(handle,iface);
		throwRuntimeError("libusb_claim_interface",error);
	}
	interfaces[iface]=driverActive;
}

void Device::releaseInterface(unsigned int iface)
{
	std::map<int,bool>::iterator it=interfaces.find(iface);
	if(it==interfaces.end()) throw(std::logic_error("Interface not claimed"));
	
	int e2=0;
	int e1=libusb_release_interface(handle,it->first);
	if(it->second)
	{
		e2=libusb_attach_kernel_driver(handle,it->first);
	}
	interfaces.erase(it);
	if(e1!=0 && e2!=0)
		throw(std::runtime_error("Error releasing iface/reattaching driver"));
	if(e1!=0) throwRuntimeError("libusb_release_interface",e1);
	if(e2!=0) throwRuntimeError("libusb_attach_kernel_driver",e2);
}

void Device::setAltSettings(unsigned int iface, unsigned int altSettings)
{
	if(interfaces.find(iface)==interfaces.end())
		throw(std::logic_error("Interface not claimed"));
	
	int error=libusb_set_interface_alt_setting(handle,iface,altSettings);
	if(error!=0) throwRuntimeError("libusb_set_interface_alt_setting",error);
}

void Device::reset()
{
	if(handle==0) throw(std::logic_error("Not open"));
	
	int error=libusb_reset_device(handle);
	if(error!=0)
	{
		try { close(); } catch(...) {}
		throwRuntimeError("libusb_reset_device",error);
	}
}

void Device::clearHalt(unsigned char endpoint)
{
	if(handle==0) throw(std::logic_error("Not open"));
	
	int error=libusb_clear_halt(handle,endpoint);
	if(error!=0) throwRuntimeError("libusb_clear_halt",error);
}

int Device::controlTransfer(unsigned char bmRequestType,
		unsigned char bRequest, unsigned short wValue, unsigned short wIndex,
		unsigned char *data, unsigned short length)
{
	//Control transfer happens on endpoint zero, no need to claim interfaces
	if(configuration==0) throw(std::logic_error("Not configured"));
	
	int error=libusb_control_transfer(handle,bmRequestType,bRequest,
			wValue,wIndex,data,length,timeout);
	if(error==LIBUSB_ERROR_TIMEOUT) throw(TimeoutException());
	if(error<0) throwRuntimeError("libusb_control_transfer",error);
	return error;
}

int Device::bulkTransfer(unsigned char endpoint, unsigned char *data,
		int length)
{
	if(interfaces.empty()) throw(std::logic_error("No interface claimed"));
	
	int result;
	int error=libusb_bulk_transfer(handle,endpoint,data,length,
			&result,timeout);
	if(error==LIBUSB_ERROR_TIMEOUT) throw(TimeoutException(result));
	if(error!=0) throwRuntimeError("libusb_bulk_transfer",error);
	return result;
}

int Device::interruptTransfer(unsigned char endpoint, unsigned char *data,
		int length)
{
	if(interfaces.empty()) throw(std::logic_error("No interface claimed"));
	
	int result;
	int error=libusb_interrupt_transfer(handle,endpoint,data,length,
			&result,timeout);
	if(error==LIBUSB_ERROR_TIMEOUT) throw(TimeoutException(result));
	if(error!=0) throwRuntimeError("libusb_interrupt_transfer",error);
	return result;
}

void Device::close()
{
	bool error=releaseAllInterfaces();
	if(handle!=0) libusb_close(handle);
	configuration=0;
	handle=0;
	if(error) throw(std::runtime_error("Device::close()"));
}

void Device::throwRuntimeError(const std::string& desc, int code)
{
	std::stringstream ss;
	ss<<desc<<": "<<code;
	throw(std::runtime_error(ss.str()));
}

bool Device::releaseAllInterfaces()
{
	bool error=false;
	std::map<int,bool>::iterator it;
	for(it=interfaces.begin();it!=interfaces.end();++it)
	{
		if(libusb_release_interface(handle,it->first)!=0) error=true;
		if(it->second)
		{
			if(libusb_attach_kernel_driver(handle,it->first)!=0) error=true;
		}
	}
	interfaces.clear();
	return error;
}

} //namespace libusb
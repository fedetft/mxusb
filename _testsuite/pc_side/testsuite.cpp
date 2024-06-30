/***************************************************************************
 *   Copyright (C) 2011-2024 by Terraneo Federico                          *
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

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <sstream>
#include <chrono>
#include <thread>
#include <functional>
#include "libusbwrapper.h"

using namespace std;
using namespace std::chrono;
using namespace libusb;

/**
 * Test if the device is capable of accepting vendor requests
 * \param device USB device
 * \param context USB context
 */
void testVendorRequests(Device& device, Context& context)
{
	cout<<"Testing custom requests on endpoint zero... ";
	cout.flush();
	//Step 1: control transfer without data, IN
	device.controlTransfer(0xc0,0xa5,0xf000,0xcaca,0,0);
	//Step 2: control transfer without data, OUT
	device.controlTransfer(0x40,0xaa,0xabcd,0xcdef,0,0);
	//Step 3: unsupported requests should be rejected
	bool failed=true;
	try {
		device.controlTransfer(0x40,0x55,0xabcd,0xcdef,0,0);
	} catch(exception& e)
	{
		failed=false;
	}
	if(failed) throw(runtime_error("Not rejected wrong request"));
	
	unsigned char data[128];
	//Step 4: test IN transfer with data
	device.controlTransfer(0xc0,0,0,0,data,128);
	if(data[0]!=1) throw(runtime_error("Data receive error"));
	for(int i=1;i<128;i++)
		if(data[i]!=0) throw(runtime_error("Data receive error"));
	
	//Step 5: test OUT transfer with data
	memset(data,1,128);
	device.controlTransfer(0x40,0,0,0,data,128);
	
	cout<<"OK"<<endl;
}

/**
 * Test if the device is capable of disconnecting and reconnecting
 * \param device USB device
 * \param context USB context
 */
void testUsbDisconnect(Device& device, Context& context)
{
	cout<<"Testing device disconnect... ";
	cout.flush();
	unsigned char disconnectRequest[1]={0xaa};
	//When the device sees this, it will disconnect for two seconds
	device.interruptTransfer(1 | Endpoint::OUT,disconnectRequest,1);
	device.close();

	this_thread::sleep_for(1s);
	bool failed=true;
	try {
		device.open(0xdead,0xbeef);
	} catch(exception& e)
	{
		failed=false; //OK, disconnected
	}
	if(failed) throw(runtime_error("Device did not disconnect"));

	this_thread::sleep_for(2s);
	device.open(0xdead,0xbeef);
	if(device.getConfiguration()!=1) device.setConfiguration(1);
	device.setTimeout(5000); //5s
	device.claimInterface(0);
	cout<<"OK"<<endl;
}

/**
 * Test if reading and writing from interrupt endpoints work
 * \param device USB device
 * \param context USB context
 */
void testInterruptEndpoints(Device& device, Context& context)
{
	cout<<"Testing interrupt transfers... ";
	cout.flush();
	unsigned char out[8];
	unsigned char in[8];
	for(int i=0;i<500;i++)
	{
		for(int j=0;j<8;j++) out[j]=rand();
		device.interruptTransfer(2 | Endpoint::OUT,out,8);
		int readBytes=device.interruptTransfer(2 | Endpoint::IN,in,8);
		if(readBytes!=8) throw(runtime_error("Received wrong # of bytes"));
		for(int j=0;j<8;j++)
		{
			out[j]^=0x12; //The device applies this to incoming data
			if(in[j]!=out[j]) throw(runtime_error("Data transfer error"));
		}
	}
	cout<<"OK"<<endl;
}

/**
 * Test bulk endpoints to check if data transfer are correct.
 * Note: the use of libusb's synchrononus API means that only one packet
 * per USB frame will be sent. Therefore the transfer speed is low.
 * The USB bulk speed is tested in a separate test.
 * \param device USB device
 * \param context USB context
 */
void testBulkEndpoints(Device& device, Context& context)
{
	cout<<"Testing bulk transfers... ";
	cout.flush();
	unsigned char out[32];
	unsigned char in[32];
	for(int i=0;i<500;i++)
	{
		for(int j=0;j<32;j++) out[j]=rand();
		device.bulkTransfer(3 | Endpoint::OUT,out,32);
		this_thread::sleep_for(1ms);
		int readBytes=device.bulkTransfer(4 | Endpoint::IN,in,32);
		if(readBytes!=32)
		{
			stringstream ss; ss<<"Received wrong # of bytes: "<<readBytes;
			throw(runtime_error(ss.str()));
		}
		for(int j=0;j<32;j++)
		{
			out[j]^=0x56; //The device applies this to incoming data
			if(in[j]!=out[j]) throw(runtime_error("Data transfer error"));
		}
	}
	cout<<"OK"<<endl;
}

int numPackets=0; ///Used by decCount, testBulkInSpeed() and testBulkOutSpeed()

/**
 * Callback called by libusb when an asynchronous transfer completes.
 * Used by testBulkInSpeed() and testBulkOutSpeed()
 * \param transfer pointer to relative libusb_transfer struct
 */
void decCount(struct libusb_transfer *transfer)
{
	if(numPackets>0) libusb_submit_transfer(transfer);
	numPackets--;
}

/**
 * Test bulk out endpoints speed.
 * Note: data sent to the device is discareded, the correctness of
 * data transfers is tested in testBulkEndpoints()
 * \param device USB device
 * \param context USB context
 */
void testBulkOutSpeed(Device& device, Context& context)
{
	cout<<"Testing bulk out speed... ";
	cout.flush();
	numPackets=19000-19; //Number of transfers minus the initial ones
	vector<unsigned char *> buffers;
	vector<libusb_transfer *> transfers;

	//Have 19 outstanding transfers, since this is the max
	//number of 64 bulk packets per frame
	for(int i=0;i<19;i++)
	{
		unsigned char *buffer=new unsigned char[64];
		memset(buffer,0,64);
		libusb_transfer *transfer=libusb_alloc_transfer(0);
		buffers.push_back(buffer);
		transfers.push_back(transfer);
		libusb_fill_bulk_transfer(transfer,device.get(),
				1 | Endpoint::OUT,buffer,64,decCount,0,
				device.getTimeout());
		libusb_submit_transfer(transfer);
	}
	//When all transfer complete, we reach -19
	while(numPackets>-19) libusb_handle_events(context.get());

	for(int i=0;i<transfers.size();i++) delete[] buffers[i];
	for(int i=0;i<transfers.size();i++) libusb_free_transfer(transfers[i]);
	cout<<"OK"<<endl;
}

/**
 * Test bulk in endpoints speed.
 * Note: data received from the device is discareded, the correctness of
 * data transfers is tested in testBulkEndpoints()
 * \param device USB device
 * \param context USB context
 */
void testBulkInSpeed(Device& device, Context& context)
{
	cout<<"Testing bulk in speed... ";
	cout.flush();
	numPackets=19000-19; //Number of transfers minus the initial ones
	vector<unsigned char *> buffers;
	vector<libusb_transfer *> transfers;

	//Have 19 outstanding transfers, since this is the max
	//number of 64 bulk packets per frame
	for(int i=0;i<19;i++)
	{
		unsigned char *buffer=new unsigned char[64];
		memset(buffer,0,64);
		libusb_transfer *transfer=libusb_alloc_transfer(0);
		buffers.push_back(buffer);
		transfers.push_back(transfer);
		libusb_fill_bulk_transfer(transfer,device.get(),
				2 | Endpoint::IN,buffer,64,decCount,0,
				device.getTimeout());
		libusb_submit_transfer(transfer);
	}
	//When all transfer complete, we reach -19
	while(numPackets>-19) libusb_handle_events(context.get());

	for(int i=0;i<transfers.size();i++) delete[] buffers[i];
	for(int i=0;i<transfers.size();i++) libusb_free_transfer(transfers[i]);
	cout<<"OK"<<endl;
}

/**
 * Wrap a call to a test function measuring execution time
 * \param func test function
 * \param device USB device
 * \param context USB context
 * \param numPackets optional, to have packet transfer statistics for BULK ep
 */
void measureTime(function<void (Device&,Context&)> func, Device& device,
		Context& context, int numPackets=0)
{
	auto t1=steady_clock::now();
	func(device,context);
	auto t2=steady_clock::now();
	float time=duration<float>(t2-t1).count();
	cout<<" Time required="<<time<<"s"<<endl;
	if(numPackets==0) return;
	cout<<" Average packets per frame="<<(numPackets/1000)/time<<endl;
	cout<<" Speed="<<static_cast<int>((numPackets*64/1024)/time)<<"KB/s"<<endl;
}

int main()
{
	try {
		Context context;
		Device device(context,0xdead,0xbeef);
		if(device.getConfiguration()!=1) device.setConfiguration(1);
		device.setTimeout(5000); //5s
		device.claimInterface(0);
		testVendorRequests(device,context);
		testUsbDisconnect(device,context);
		measureTime(testInterruptEndpoints,device,context);
		device.setConfiguration(2);
		device.claimInterface(0);
		this_thread::sleep_for(10ms);
		measureTime(testBulkEndpoints,device,context);
		measureTime(testBulkOutSpeed,device,context,19000);
		measureTime(testBulkInSpeed,device,context,19000);
		device.setConfiguration(1);
		cout<<"Test passed"<<endl;
	} catch(exception& e)
	{
		cout<<"Exception:"<<e.what()<<endl;
	}
}

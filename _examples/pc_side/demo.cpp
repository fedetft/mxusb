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

#include <iostream>
#include <sstream>
#include "libusbwrapper.h"

using namespace std;
using namespace libusb;

/**
 * Sends a string to endpoint 1 using bulk transfers of up to 64 bytes.
 * Adds \n to the end of the line.
 * \param device USB device
 * \param line string to send
 */
void sendString(Device& device, const string& line)
{
	stringstream ss(line+"\n");
	for(;;)
	{
		unsigned char data[64+1];
		 //Split writes in chunks of up to 64 bytes
		ss.read(reinterpret_cast<char*>(data),64);
		int available=ss.gcount();
		if(available==0) break;
		device.bulkTransfer(1 | Endpoint::OUT,data,available);
		data[available]=0;
		cout<<data<<endl;
	}
}

int main()
{
	try {
		Context context;
		Device device(context,0xdead,0xbeef);
		device.claimInterface(0);
		cout<<"Enter some text, it will be sent to the USB device"<<endl;
		cout<<"Enter an empty line to quit"<<endl;
		for(;;)
		{
			string line;
			getline(cin,line);
			if(line.length()==0) break;
			sendString(device,line);
			cout<<line<<endl;
		}
	} catch(exception& e)
	{
		cout<<e.what()<<endl;
	}
}

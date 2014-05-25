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

#include "mxusb/usb.h"
#include <config/usb_config.h>
#include <cstdio>

using namespace mxusb;

//
// Descriptors
//
const unsigned char device[]=
{
    Descriptor::DEVICE_DESC_SIZE,
    Descriptor::DEVICE,
    0x0, 0x02,  //bcdUSB=2.00
    0xff,       //bDeviceClass=vendor specific
    0xff,       //bDeviceSubClass=vendor specific
    0xff,       //bDeviceProtocol=vendor specific
    EP0_SIZE,   //bMaxPacketSize0=max packet size for ep0
    0xad, 0xde, //idVendor=0xdead
    0xef, 0xbe, //idProduct=0xbeef
    0x00, 0x00, //bcdDevice=device version v0.00
    0x0,        //iManufacturer (no string)
    0x1,        //iProduct      (string index 1)
    0x0,        //iSerialNumber (no string)
    0x1         //bNumConfigrations
};

const unsigned char stringLangId[]=
{
    4,         //bLength
    Descriptor::STRING,
    0x09,0x04  //0x0409=US English
};

const unsigned char stringProduct[]=
{
    22,        //bLength
    Descriptor::STRING,
    'm',0,'x',0,'u',0,'s',0,'b',0,' ',0,'d',0,'e',0,'m',0,'o',0
};

const unsigned char * const strings[]=
{
    stringLangId,stringProduct
};

const int nstr=sizeof(strings)/sizeof(unsigned char*);

const unsigned char config1[]=
{
    Descriptor::CONFIGURATION_DESC_SIZE,
    Descriptor::CONFIGURATION,
    25,0,       //wTotalLength
    0x1,        //bNumInterfaces
    0x1,        //bConfigurationValue
    0x0,        //iConfiguration (no string)
    0xc0,       //bmAtributes=self powered
    100/2,      //bMaxPower=100mA

        Descriptor::INTERFACE_DESC_SIZE,
        Descriptor::INTERFACE,
        0x0,        //bInterfaceNumber
        0x0,        //bAlternateSetting
        0x1,        //bNumEndpoints
        0xff,       //bInterfaceClass=vendor specific
        0xff,       //bInterfaceSubClass=vendor specific
        0xff,       //bInterfaceProtocol=vendor specific
        0x0,        //iInterface (no string)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x01,        //bEndpointAddress=OUT1
            Descriptor::BULK,
            64,0,        //wMaxPacketSize
            0x0,         //bInterval (not used for bulk endpoints)
};

const unsigned char * const configurations[]=
{
    config1
};

int main()
{
    USBdevice::enable(device,configurations,strings,nstr);
    USBdevice::waitUntilConfigured();
    Endpoint ep1=Endpoint::get(1);
    unsigned char data[64+1];
    int readBytes;
    for(;;)
    {
        ep1.read(data,readBytes);
        data[readBytes]='\0'; //Terminate string
        iprintf("%s",data);
    }
}

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

#include "miosix.h"
#include "mxusb/usb.h"
#include <config/usb_config.h>
#include "mxusb/ep0.h"
#include <cstdio>
#include <cstring>

using namespace std;
using namespace miosix;
using namespace mxusb;

//
// Descriptors, taken from stm32 usb serial example.
//
const unsigned char device[]=
{
    Descriptor::DEVICE_DESC_SIZE,
    Descriptor::DEVICE,
    0x0, 0x02,  //bcdUSB=2.00
    0x02,       //bDeviceClass=cdc
    0x00,       //bDeviceSubClass
    0x00,       //bDeviceProtocol
    EP0_SIZE,   //bMaxPacketSize0=max packet size for ep0
    0x83, 0x04, //idVendor=0x0483
    0x40, 0x57, //idProduct=0x5740
    0x00, 0x00, //bcdDevice=device version v0.00
    0x1,        //iManufacturer (string index 1)
    0x2,        //iProduct      (string index 2)
    0x0,        //iSerialNumber (no string)
    0x1         //bNumConfigrations
};

const unsigned char stringLangId[]=
{
    4,         //bLength
    Descriptor::STRING,
    0x09,0x04  //0x0409=US English
};

const unsigned char stringManufacturer[]=
{
    38,        //bLength
    Descriptor::STRING,
    'S', 0, 'T', 0, 'M', 0, 'i', 0, 'c', 0, 'r', 0, 'o', 0, 'e', 0,
    'l', 0, 'e', 0, 'c', 0, 't', 0, 'r', 0, 'o', 0, 'n', 0, 'i', 0,
    'c', 0, 's', 0
};

const unsigned char stringProduct[]=
{
    50,        //bLength
    Descriptor::STRING,
    'S', 0, 'T', 0, 'M', 0, '3', 0, '2', 0, ' ', 0, 'V', 0, 'i', 0,
    'r', 0, 't', 0, 'u', 0, 'a', 0, 'l', 0, ' ', 0, 'C', 0, 'O', 0,
    'M', 0, ' ', 0, 'P', 0, 'o', 0, 'r', 0, 't', 0, ' ', 0, ' ', 0
};

const unsigned char * const strings[]=
{
    stringLangId,stringManufacturer,stringProduct
};

const int nstr=sizeof(strings)/sizeof(unsigned char*);

const unsigned char config1[]=
{
    Descriptor::CONFIGURATION_DESC_SIZE,
    Descriptor::CONFIGURATION,
    67,0,       //wTotalLength
    0x2,        //bNumInterfaces
    0x1,        //bConfigurationValue
    0x0,        //iConfiguration (no string)
    0xc0,       //bmAtributes=self powered
    100/2,       //bMaxPower=100mA

        Descriptor::INTERFACE_DESC_SIZE,
        Descriptor::INTERFACE,
        0x0,        //bInterfaceNumber
        0x0,        //bAlternateSetting
        0x1,        //bNumEndpoints
        0x2,        //bInterfaceClass=communication
        0x2,        //bInterfaceSubClass=ACM
        0x1,        //bInterfaceProtocol=AT commands
        0x0,        //iInterface

        5,          //bLength (length of this descriptor only)
        0x24,       //bDescriptorType=CS_INTERFACE
        0x0,        //bDescriptorSubtype=header func desc
        0x10,0x1,   //bcdCDC=spec release number

        5,          //bLength (length of this descriptor only)
        0x24,       //bDescriptorType=CS_INTERFACE
        0x1,        //bDescriptorSubtype=call management func desc
        0x0,        //bmCapabilities=D0+D1 @ 0 = no call management provided
        0x1,        //bDataInterface=1

        4,          //bLength (length of this descriptor only)
        0x24,       //bDescriptorType=CS_INTERFACE
        0x2,        //bDescriptorSubtype=abstract control management desc
        0x2,        //bmCapabilities=get/set_line_coding, set_control_line_state

        5,          //bLength (length of this descriptor only)
        0x24,       //bDescriptorType=CS_INTERFACE
        0x6,        //bDescriptorSubtype=union func desc
        0x0,        //bMasterInterface=communication class, interface #0, (this)
        0x1,        //bSlaveInterface0=data class, interface #1 (the other one)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x82,       //bEndpointAddress=IN2
            Descriptor::INTERRUPT,
            8,0,        //wMaxPacketSize
            0xff,       //bInterval

        Descriptor::INTERFACE_DESC_SIZE,
        Descriptor::INTERFACE,
        0x1,        //bInterfaceNumber
        0x0,        //bAlternateSetting
        0x2,        //bNumEndpoints
        0x0a,       //bInterfaceClass=CDC Data interface class
        0x0,        //bInterfaceSubClass=reserved, should be 0
        0x0,        //bInterfaceProtocol=no protocol defined
        0x0,        //iInterface

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x03,       //bEndpointAddress=OUT3
            Descriptor::BULK,
            64,0,       //wMaxPacketSize
            0x0,        //bInterval (unused for bulk)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x81,       //bEndpointAddress=IN1
            Descriptor::BULK,
            64,0,       //wMaxPacketSize
            0x0,        //bInterval (unused for bulk)
};

const unsigned char * const configurations[]=
{
    config1
};

/**
 * For an USB device to comply with the CDC ACM specifications, it must
 * implement some requests on endpoint zero.
 */
class SerialEndpointZeroCallbacks : public EndpointZeroCallbacks
{
public:

    enum Requests
    {
        GET_LINE_CODING=0x21,
        SET_LINE_CODING=0x20,
        SET_CONTROL_LINE_STATE=0x22,
        SET_COMM_FEATURE=0x02
    };

    SerialEndpointZeroCallbacks() : EndpointZeroCallbacks()
    {
        //Set to a reasonable default, 115.2 8n1
        //These values aren't actually used, bu the PC needs to be able to
        //set/get them
        lineCoding.dwDTERate=115200;
        lineCoding.bCharFormat=0;
        lineCoding.bParityType=0;
        lineCoding.bDataBits=8;
    }

    virtual bool IRQsetup(const Setup *setup)
    {
        if((setup->bmRequestType & (Setup::TYPE_MASK | Setup::RECIPIENT_MASK))
                !=(Setup::TYPE_CLASS | Setup::RECIPIENT_INTERFACE))
        {
            return false;
        }
        
        switch(setup->bRequest)
        {
            case SET_COMM_FEATURE:
            case SET_CONTROL_LINE_STATE:
                if(setup->wLength==0) return true;
                else return false;
                break;
            case SET_LINE_CODING:
            case GET_LINE_CODING:
                if(setup->wLength==sizeof(LineCoding))
                {
                    EndpointZeroCallbacks::IRQsetDataBuffer(
                            reinterpret_cast<unsigned char *>(&lineCoding));
                    return true;
                } else return false;
                break;
            default:
                return false;
        }
    }

    virtual bool IRQendOfOutDataStage(const Setup *setup)
    {
        return true;
    }

private:
    struct LineCoding
    {
        unsigned int dwDTERate;
        unsigned char bCharFormat;
        unsigned char bParityType;
        unsigned char bDataBits;
    };

    LineCoding lineCoding;
};

int main()
{
    SerialEndpointZeroCallbacks serialCallbacks;
    EndpointZeroCallbacks::setCallbacks(&serialCallbacks);
    USBdevice::enable(device,configurations);//,strings,nstr);
    USBdevice::waitUntilConfigured();
    Endpoint ep3=Endpoint::get(3);
    unsigned char data[64+1];
    int readBytes;
    for(;;)
    {
        if(ep3.read(data,readBytes)==false) break;
        data[readBytes]='\0'; //Terminate string
        for(int i=0;i<readBytes;i++) if(data[i]=='\r') data[i]='\n';
        iprintf("%s",data);
    }
    iprintf("Error\n");
    for(;;) ;
}

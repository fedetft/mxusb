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
#include "miosix.h"
#include "mxusb/ep0.h"
#include <cstdio>
#include <cstring>

using namespace mxusb;
using namespace miosix;

//
// Endpoints for the testsuite
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
    0x1,        //iManufacturer (string index 1)
    0x2,        //iProduct      (string index 2)
    0x0,        //iSerialNumber (no string)
    0x2         //bNumConfigrations
};

const unsigned char stringLangId[]=
{
    4,         //bLength
    Descriptor::STRING,
    0x09,0x04  //0x0409=US English
};

const unsigned char stringManufacturer[]=
{
    18,        //bLength
    Descriptor::STRING,
    'B',0,'i',0,'o',0,'p',0,'a',0,'r',0,'c',0,'o',0
};

const unsigned char stringProduct[]=
{
    14,        //bLength
    Descriptor::STRING,
    'A',0,'n',0,'t',0,'a',0,'n',0,'i',0
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
    39,0,       //wTotalLength
    0x1,        //bNumInterfaces
    0x1,        //bConfigurationValue
    0x0,        //iConfiguration (no string)
    0xc0,       //bmAtributes=self powered
    100/2,      //bMaxPower=100mA

        Descriptor::INTERFACE_DESC_SIZE,
        Descriptor::INTERFACE,
        0x0,        //bInterfaceNumber
        0x0,        //bAlternateSetting
        0x3,        //bNumEndpoints
        0xff,       //bInterfaceClass=vendor specific
        0xff,       //bInterfaceSubClass=vendor specific
        0xff,       //bInterfaceProtocol=vendor specific
        0x0,        //iInterface (no string)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x01,        //bEndpointAddress=OUT1
            Descriptor::INTERRUPT,
            8,0,         //wMaxPacketSize
            100,         //bInterval (poll every 100ms)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x02,        //bEndpointAddress=OUT2
            Descriptor::INTERRUPT,
            8,0,         //wMaxPacketSize
            0x1,         //bInterval (poll every 1ms)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x82,        //bEndpointAddress=IN2
            Descriptor::INTERRUPT,
            8,0,         //wMaxPacketSize
            0x1,         //bInterval (poll every 1ms)
};

const unsigned char config2[]=
{
    Descriptor::CONFIGURATION_DESC_SIZE,
    Descriptor::CONFIGURATION,
    46,0,       //wTotalLength
    0x1,        //bNumInterfaces
    0x2,        //bConfigurationValue
    0x0,        //iConfiguration (no string)
    0xc0,       //bmAtributes=self powered
    100/2,      //bMaxPower=100mA

        Descriptor::INTERFACE_DESC_SIZE,
        Descriptor::INTERFACE,
        0x0,        //bInterfaceNumber
        0x0,        //bAlternateSetting
        0x4,        //bNumEndpoints
        0xff,       //bInterfaceClass=vendor specific
        0xff,       //bInterfaceSubClass=vendor specific
        0xff,       //bInterfaceProtocol=vendor specific
        0x0,        //iInterface (no string)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x01,        //bEndpointAddress=OUT1
            Descriptor::BULK,
            64,0,        //wMaxPacketSize
            0x0,         //bInterval (ignored for bulk)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x82,        //bEndpointAddress=IN2
            Descriptor::BULK,
            64,0,        //wMaxPacketSize
            0x0,         //bInterval (ignored for bulk)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x03,        //bEndpointAddress=OUT3
            Descriptor::BULK,
            32,0,        //wMaxPacketSize
            0x0,         //bInterval (ignored for bulk)

            Descriptor::ENDPOINT_DESC_SIZE,
            Descriptor::ENDPOINT,
            0x84,        //bEndpointAddress=IN4
            Descriptor::BULK,
            32,0,        //wMaxPacketSize
            0x0,         //bInterval (ignored for bulk)
};

const unsigned char * const configurations[]=
{
    config1,config2
};

/**
 * Thread spawned when configuration 1 is selected.
 * It reads IN packets from endpoint 2 and writes them back to the same
 * endpoint. It terminates when the configuration is changed.
 * \param argv not used
 */
void config1Thread1(void *argv)
{
    Endpoint ep=Endpoint::get(2);
    unsigned char packet[8];
    int bytesRead,written;
    for(;;)
    {
        bool success=ep.read(packet,bytesRead);
        if(success==false || bytesRead!=8)
        {
            if(USBdevice::isSuspended() || USBdevice::getConfiguration()!=1)
                return; //No errors, just suspended/reconfigured
            iprintf("Thread 1: error while reading\n");
            continue;
        }
        for(int i=0;i<bytesRead;i++) packet[i]^=0x12; //Do some work
        success=ep.write(packet,bytesRead,written);
        if(success==false || written!=8)
        {
            if(USBdevice::isSuspended() || USBdevice::getConfiguration()!=1)
                return; //No errors, just suspended/reconfigured
            iprintf("Thread 1: error while writing\n");
            continue;
        }
    }
}

/**
 * Handles configuration 1.
 * There are three endpoints:
 * - EP1 OUT: handled by this thread, 0xaa is received the USB device is
 *   disconnected for two seconds
 * - EP2 OUT: handled by the spawned thread
 * - EP2 IN: handled by the spawned thread
 */
void configuration1()
{
    iprintf("Configuration 1 chosen\n");
    Thread *t=Thread::create(config1Thread1,2048,1,0,Thread::JOINABLE);
    Endpoint ep=Endpoint::get(1);
    unsigned char packet[8];
    int bytesRead;
    for(;;)
    {
        bool success=ep.read(packet,bytesRead);
        if(success==false || bytesRead!=1)
        {
            if(USBdevice::isSuspended() || USBdevice::getConfiguration()!=1)
                break; //No errors, just suspended/reconfigured
            iprintf("Error while reading (read=%d,config=%d,susp=%d,succ=%d)\n",
                bytesRead,USBdevice::getConfiguration(),
                USBdevice::isSuspended(),success);
            continue;
        }
        if(packet[0]==0xaa)
        {
            iprintf("Shutting down USB peripheral\n");
            USBdevice::disable();
            t->join();
            iprintf("Thread join OK\n");
            Thread::sleep(2000);
            iprintf("Enabling back USB device\n");
            USBdevice::enable(device,configurations,strings,nstr);
            return;
        } else {
            iprintf("Wrong packet\n");
        }
    }
    t->join();
}

/**
 * Callbacks that are set up when configuration 2 is selected.
 */
class MyCallbacks : public Callbacks
{
public:
    MyCallbacks(): Callbacks(), errors(false) {}

    /**
     * Handles endpoints 1 and 2 of configuration 2, used to test write and read
     * speed. Dataa transferred in both directions is discarded.
     */
    void IRQendpoint(unsigned char epNum, Endpoint::Direction dir)
    {
        Endpoint ep=Endpoint::IRQget(epNum);
        unsigned char data[64];
        int readBytes,written;
        switch(epNum)
        {
            case 1:
                if(ep.IRQread(data,readBytes)==false || readBytes!=64)
                    errors=true;
                break;
            case 2:
                if(ep.IRQwrite(data,64,written)==false || written!=64)
                    errors=true;
                break;
            default:
                break;
        }
    }

    /**
     * \return true if there were errors while reading or writing on endpoint
     * 1 or 2
     */
    bool getErrors() const { return errors; }

private:
    volatile bool errors;
};

/**
 * Handles configuration 2.
 * There are four endpoints:
 * - EP1 OUT: handled by this thread, data received is relayed to endpoint 2
 * - EP2 IN: read above
 * - EP3 OUT: handled by the callbacks
 * - EP4 IN: handled by the callbacks
 */
void configuration2()
{
    iprintf("Configuration 2 chosen\n");
    MyCallbacks callbacks;
    Callbacks::setCallbacks(&callbacks);
    unsigned char packet[64];
    int bytesRead,written;
    //Pre fill the output buffer of endpoint 2 with two buffers, to take
    //advantage of double buffering
    {
        InterruptDisableLock dLock;
        Endpoint::IRQget(2).IRQwrite(packet,64,written);
        Endpoint::IRQget(2).IRQwrite(packet,64,written);
    }

    for(;;)
    {
        bool success=Endpoint::get(3).read(packet,bytesRead);
        if(success==false || bytesRead!=32)
        {
            if(USBdevice::isSuspended() || USBdevice::getConfiguration()!=2)
                break; //No errors, just suspended/reconfigured
            iprintf("Error while reading\n");
            continue;
        }
        for(int i=0;i<bytesRead;i++) packet[i]^=0x56; //Do some work
        success=Endpoint::get(4).write(packet,bytesRead,written);
        if(success==false || written!=32)
        {
            if(USBdevice::isSuspended() || USBdevice::getConfiguration()!=2)
                break; //No errors, just suspended/reconfigured
            iprintf("Error while writing\n");
            continue;
        }
    }
    Callbacks::setCallbacks(0); //Disable callbacks
    if(callbacks.getErrors())
        iprintf("Note: there were errors while reading/wrting on ep 1 or 2\n");
}

class MyEndpointZeroCallbacks : public EndpointZeroCallbacks
{
public:
    MyEndpointZeroCallbacks() : EndpointZeroCallbacks(), error(false) {}

    virtual bool IRQsetup(const Setup* setup)
    {
        if(setup->bmRequestType==0x40 && setup->bRequest==0xaa &&
           setup->wValue==0xabcd && setup->wIndex==0xcdef && setup->wLength==0)
        {
            return true;  //OK
        }

        if(setup->bmRequestType==0xc0 && setup->bRequest==0xa5 &&
           setup->wValue==0xf000 && setup->wIndex==0xcaca && setup->wLength==0)
        {
            return true;  //OK
        }

        if(setup->bmRequestType==0x40 && setup->bRequest==0x55 &&
           setup->wValue==0xabcd && setup->wIndex==0xcdef && setup->wLength==0)
        {
            return false; //This is a to test unsupported requests, so reject it
        }

        if(setup->bmRequestType==0xc0 && setup->bRequest==0x0 &&
           setup->wValue==0x0 && setup->wIndex==0x0 && setup->wLength==128)
        {
            memset(buffer,0,128);
            buffer[0]=1;
            EndpointZeroCallbacks::IRQsetDataBuffer(buffer);
            return true; //OK
        }

        if(setup->bmRequestType==0x40 && setup->bRequest==0x0 &&
           setup->wValue==0x0 && setup->wIndex==0x0 && setup->wLength==128)
        {
            EndpointZeroCallbacks::IRQsetDataBuffer(buffer);
            return true; //OK
        }

        error=true;
        return false;
    }

    virtual bool IRQendOfOutDataStage(const Setup *setup)
    {
        for(int i=0;i<128;i++) if(buffer[i]!=1) return false;
        return true;
    }

    bool getError() const { return error; }

private:
    volatile bool error;
    unsigned char buffer[128];
};

int main()
{
    USBdevice::enable(device,configurations,strings,nstr);
    MyEndpointZeroCallbacks callbacks;
    EndpointZeroCallbacks::setCallbacks(&callbacks);
    for(;;)
    {
        iprintf("Waiting for USB host to configure device\n");
        USBdevice::waitUntilConfigured();
        unsigned char configuration=USBdevice::getConfiguration();
        if(configuration==1) configuration1();
        else if(configuration==2) configuration2();
        else {
            iprintf("Error: wrong configuration %d\n",configuration);
            break;
        }
        if(callbacks.getError())
        {
            iprintf("Note: there were errors processing"
                    " class requests on ep0\n");
        }
    }
}

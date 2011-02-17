
#include "usb_impl.h"
#include "usb_tracer.h"
#include "usb_util.h"
#include "shared_memory.h"

namespace mxusb {

//
// class EndpointImpl
//

void EndpointImpl::IRQdeconfigureAll()
{
    for(int i=1;i<NUM_ENDPOINTS;i++) EndpointImpl::get(i)->IRQdeconfigure(i);
    SharedMemory::reset();
}


void EndpointImpl::IRQconfigureAll(const unsigned char *desc)
{
    const unsigned short wTotalLength=toShort(&desc[2]);
    unsigned short descSize=0;
    for(;;)
    {
        //Advance to next descriptor
        unsigned int sizeIncrement=desc[0];
        desc+=sizeIncrement;
        descSize+=sizeIncrement;
        if(descSize==wTotalLength) break;
        if(descSize>wTotalLength || sizeIncrement==0)
        {
            Tracer::IRQtrace(Ut::DESC_ERROR);
            return; //configuration descriptor is wrong
        }
        if(desc[1]!=Descriptor::ENDPOINT) continue;
        const unsigned char epNum=desc[2] & 0xf;
        EndpointImpl::get(epNum)->IRQconfigure(desc);
    }
}

void EndpointImpl::IRQdeconfigure(int epNum)
{
    USB->endpoint[epNum]=epNum;
    this->data.enabledIn=0;
    this->data.enabledOut=0;
    this->data.epNumber=epNum;
    this->IRQwakeWaitingThreadOnInEndpoint();
    this->IRQwakeWaitingThreadOnOutEndpoint();
}

void EndpointImpl::IRQconfigure(const unsigned char *desc)
{
    const unsigned char bEndpointAddress=desc[2];
    unsigned char bmAttributes=desc[3];
    Tracer::IRQtrace(Ut::CONFIGURING_EP,bEndpointAddress,bmAttributes);

    const unsigned char addr=bEndpointAddress & 0xf;
    if(addr==0 || addr>NUM_ENDPOINTS-1 || addr!=this->data.epNumber)
    {
        Tracer::IRQtrace(Ut::DESC_ERROR);
        return; //Invalid ep #, or called with an endpoint with a wrong #
    }

    if((this->data.enabledIn==1  && ((bEndpointAddress & 0x80)==1)) ||
       (this->data.enabledOut==1 && ((bEndpointAddress & 0x80)==0)))
    {
        Tracer::IRQtrace(Ut::DESC_ERROR);
        return; //Trying to configure an ep direction twice
    }

    if((this->data.enabledIn==1 || this->data.enabledOut==1))
    {
        //We're trying to enable both sides of an endpoint.
        //This is only possible if both sides are of type INTERRUPT
        if((bmAttributes & Descriptor::TYPE_MASK)!=Descriptor::INTERRUPT ||
            this->data.type!=Descriptor::INTERRUPT)
        {
            Tracer::IRQtrace(Ut::DESC_ERROR);
            return; //Trying to enable both sides of a non INTERRUPT ep
        }
    }

    switch(bmAttributes & Descriptor::TYPE_MASK)
    {
        case Descriptor::INTERRUPT:
            IRQconfigureInterruptEndpoint(desc);
            break;
        case Descriptor::BULK:
            IRQconfigureBulkEndpoint(desc);
            break;
        case Descriptor::CONTROL:
        case Descriptor::ISOCHRONOUS:
            Tracer::IRQtrace(Ut::DESC_ERROR);
            return; //CONTROL and ISOCHRONOUS endpoints not supported
    }
}

void EndpointImpl::IRQconfigureInterruptEndpoint(const unsigned char *desc)
{
    //Get endpoint data
    const unsigned char bEndpointAddress=desc[2];
    const unsigned char addr=bEndpointAddress & 0xf;
    const unsigned short wMaxPacketSize=toShort(&desc[4]);

    const shmem_ptr ptr=SharedMemory::allocate(wMaxPacketSize);
    if(ptr==0 || wMaxPacketSize==0)
    {
        Tracer::IRQtrace(Ut::OUT_OF_SHMEM);
        return; //Out of memory, or wMaxPacketSize==0
    }

    this->data.type=Descriptor::INTERRUPT;

    USB->endpoint[addr].IRQclearEpKind();
    USB->endpoint[addr].IRQsetType(EndpointRegister::INTERRUPT);

    if(bEndpointAddress & 0x80)
    {
        //IN endpoint
        USB->endpoint[addr].IRQsetDtogTx(false);
        USB->endpoint[addr].IRQsetTxBuffer(ptr,0);
        USB->endpoint[addr].IRQsetTxStatus(EndpointRegister::NAK);
        this->buf0=ptr;
        this->size0=wMaxPacketSize;
        this->data.enabledIn=1;
    } else {
        //OUT endpoint
        USB->endpoint[addr].IRQsetDtogRx(false);
        USB->endpoint[addr].IRQsetRxBuffer(ptr,wMaxPacketSize);
        USB->endpoint[addr].IRQsetRxStatus(EndpointRegister::VALID);
        this->buf1=ptr;
        this->size1=wMaxPacketSize;
        this->data.enabledOut=1;
    }
}

void EndpointImpl::IRQconfigureBulkEndpoint(const unsigned char *desc)
{
    //Get endpoint data
    const unsigned char bEndpointAddress=desc[2];
    const unsigned char addr=bEndpointAddress & 0xf;
    const unsigned short wMaxPacketSize=toShort(&desc[4]);

    const shmem_ptr ptr0=SharedMemory::allocate(wMaxPacketSize);
    const shmem_ptr ptr1=SharedMemory::allocate(wMaxPacketSize);
    if(ptr0==0 || ptr1==0 || wMaxPacketSize==0)
    {
        Tracer::IRQtrace(Ut::OUT_OF_SHMEM);
        return; //Out of memory, or wMaxPacketSize==0
    }

    this->data.type=Descriptor::BULK;
    this->buf0=ptr0;
    this->size0=wMaxPacketSize;
    this->buf1=ptr1;
    this->size1=wMaxPacketSize;

    USB->endpoint[addr].IRQsetType(EndpointRegister::BULK);
    USB->endpoint[addr].IRQsetEpKind();//Enpoint is double buffered

    if(bEndpointAddress & 0x80)
    {
        //IN endpoint
        USB->endpoint[addr].IRQsetDtogTx(false);
        USB->endpoint[addr].IRQsetDtogRx(false); //Actually, SW_BUF
        USB->endpoint[addr].IRQsetTxBuffer0(ptr0,0);
        USB->endpoint[addr].IRQsetTxBuffer1(ptr1,0);
        USB->endpoint[addr].IRQsetTxStatus(EndpointRegister::NAK);
        this->data.enabledIn=1;
    } else {
        //OUT endpoint
        USB->endpoint[addr].IRQsetDtogRx(false);
        USB->endpoint[addr].IRQsetDtogTx(false); //Actually, SW_BUF
        USB->endpoint[addr].IRQsetRxBuffer0(ptr0,wMaxPacketSize);
        USB->endpoint[addr].IRQsetRxBuffer1(ptr1,wMaxPacketSize);
        USB->endpoint[addr].IRQsetRxStatus(EndpointRegister::VALID);
        this->data.enabledOut=1;
    }
    this->bufCount=0;
}

EndpointImpl EndpointImpl::endpoints[NUM_ENDPOINTS-1];
EndpointImpl EndpointImpl::invalidEp; //Invalid endpoint, always disabled

//
// class DeviceStateImpl
//

void DeviceStateImpl::IRQsetState(USBdevice::State s)
{
    if(state==s) return; //No real state change
    state=s;
    if(configWaiting!=0 && state==USBdevice::CONFIGURED)
    {
        configWaiting->IRQwakeup();
        configWaiting=0;
    }
    Tracer::IRQtrace(Ut::DEVICE_STATE_CHANGE,state);
    Callbacks::IRQgetCallbacks()->IRQstateChanged();
}

volatile USBdevice::State DeviceStateImpl::state=USBdevice::DEFAULT;
volatile unsigned char DeviceStateImpl::configuration=0;
volatile bool DeviceStateImpl::suspended=false;
#ifdef _MIOSIX
miosix::Thread *DeviceStateImpl::configWaiting=0;
#endif //_MIOSIX

} //namespace mxusb

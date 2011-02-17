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

#ifndef MXUSB_LIBRARY
#error "This is header is private, it can be used only within mxusb."
#error "If your code depends on a private header, it IS broken."
#endif //MXUSB_LIBRARY

#include "shared_memory.h"

#ifdef _MIOSIX
#include "interfaces/arch_registers.h"
#else //_MIOSIX
#include "stm32f10x.h"
#endif //_MIOSIX

#ifndef ENDPOINT_H
#define	ENDPOINT_H

namespace mxusb {

/**
 * \internal
 * Endpoint registers are quite a bit tricky to touch, since they both have
 * "normal" bits, rc_w0 bits that can only be cleared by writing zero and
 * toggle-only bits. This makes it hard to change a bit without inadvertently
 * flipping some other.
 */
class EndpointRegister
{
public:
    /**
     * Note: bitmask for Descriptor::Type (bitmask used in standard USB
     * descriptors) differ from Endpoint::Type (bitmask used in stm32's EPnR
     * register bits for endpoint types)
     */
    enum Type
    {
        BULK=0,
        CONTROL=USB_EP0R_EP_TYPE_0,
        ISOCHRONOUS=USB_EP0R_EP_TYPE_1,
        INTERRUPT=USB_EP0R_EP_TYPE_1 | USB_EP0R_EP_TYPE_0
    };

    enum Status
    {
        DISABLED=0,
        STALL=1<<0,
        NAK=1<<1,
        VALID=(1<<0) | (1<<1)
    };

    /**
     * Set endpoint type
     * \param type BULK/CONTROL/ISOCHRONOUS/INTERRUPT
     */
    void IRQsetType(Type type);

    /**
     * Set the way an endpoint answers IN transactions (device to host)
     * \param status DISABLED/STALL/NAK/VALID
     */
    void IRQsetTxStatus(Status status);

    /**
     * Get the way an endpoint answers IN transactions (device to host)
     * \return status DISABLED/STALL/NAK/VALID
     */
    Status IRQgetTxStatus() const
    {
        return static_cast<EndpointRegister::Status>((EPR>>4) & 0x3);
    }

    /**
     * Set the way an endpoint answers OUT transactions (host to device)
     * \param status DISABLED/STALL/NAK/VALID
     */
    void IRQsetRxStatus(Status status);

    /**
     * Get the way an endpoint answers OUT transactions (host to device)
     * \return status DISABLED/STALL/NAK/VALID
     */
    Status IRQgetRxStatus() const
    {
        return static_cast<EndpointRegister::Status>((EPR>>12) & 0x3);
    }

    /**
     * Set tx buffer for an endpoint. It is used for IN transactions
     * \param addr address of buffer, as returned by SharedMemory::allocate()
     * \param size buffer size. Size must be divisible by 2
     */
    void IRQsetTxBuffer(shmem_ptr addr, unsigned short size);

    /**
     * Set alternate tx buffer 0 for an endpoint.
     * It is used for double buffered BULK IN endpoints.
     * \param addr address of buffer, as returned by SharedMemory::allocate()
     * \param size buffer size. Size must be divisible by 2
     */
    void IRQsetTxBuffer0(shmem_ptr addr, unsigned short size)
    {
        IRQsetTxBuffer(addr,size);
    }

    /**
     * Set alternate tx buffer 1 for an endpoint.
     * It is used for double buffered BULK IN endpoints.
     * \param addr address of buffer, as returned by SharedMemory::allocate()
     * \param size buffer size. Size must be divisible by 2
     */
    void IRQsetTxBuffer1(shmem_ptr addr, unsigned short size);

    /**
     * Set size of buffer to be transmitted
     * \param size buffer size
     */
    void IRQsetTxDataSize(unsigned short size)
    {
        int ep=EPR & USB_EP0R_EA;
        SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+2)=size;
    }

    /**
     * Set size of alternate tx buffer 0 to be transmitted.
     * It is used for double buffered BULK IN endpoints.
     * \param size buffer size
     */
    void IRQsetTxDataSize0(unsigned short size)
    {
        IRQsetTxDataSize(size);
    }

    /**
     * Set size of alternate tx buffer 1 to be transmitted.
     * It is used for double buffered BULK IN endpoints.
     * \param size buffer size
     */
    void IRQsetTxDataSize1(unsigned short size)
    {
        int ep=EPR & USB_EP0R_EA;
        SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+6)=size;
    }

    /**
     * Set rx buffer for an endpoint. It is used for OUT transactions
     * \param addr address of buffer, as returned by SharedMemory::allocate()
     * \param size buffer size. Due to hardware restrictions, size must respect
     * these constraints:
     * - if size is less or equal 62 bytes, it must be divisible by 2
     * - if size is more than 62 bytes, it must be a multiple of 32
     */
    void IRQsetRxBuffer(shmem_ptr addr, unsigned short size);

    /**
     * Set alternate rx buffer 0 for an endpoint.
     * It is used for double buffered BULK OUT endpoints.
     * \param addr address of buffer, as returned by SharedMemory::allocate()
     * \param size buffer size. Due to hardware restrictions, size must respect
     * these constraints:
     * - if size is less or equal 62 bytes, it must be divisible by 2
     * - if size is more than 62 bytes, it must be a multiple of 32
     */
    void IRQsetRxBuffer0(shmem_ptr addr, unsigned short size);

    /**
     * Set alternate rx buffer 1 for an endpoint.
     * It is used for double buffered BULK OUT endpoints.
     * \param addr address of buffer, as returned by SharedMemory::allocate()
     * \param size buffer size. Due to hardware restrictions, size must respect
     * these constraints:
     * - if size is less or equal 62 bytes, it must be divisible by 2
     * - if size is more than 62 bytes, it must be a multiple of 32
     */
    void IRQsetRxBuffer1(shmem_ptr addr, unsigned short size)
    {
        IRQsetRxBuffer(addr,size);
    }

    /**
     * When servicing an OUT transaction, get the number of bytes that the
     * host PC sent.
     * \return the number of bytes received
     */
    unsigned short IRQgetReceivedBytes() const
    {
        int ep=EPR & USB_EP0R_EA;
        return SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+6) & 0x3ff;
    }

    /**
     * When servicing an OUT transaction on a double buffered BULK endpoint,
     * get the number of bytes that the host PC sent on buffer 0.
     * \return the number of bytes received
     */
    unsigned short IRQgetReceivedBytes0() const
    {
        int ep=EPR & USB_EP0R_EA;
        return SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+2) & 0x3ff;
    }

    /**
     * When servicing an OUT transaction on a double buffered BULK endpoint,
     * get the number of bytes that the host PC sent on buffer 1.
     * \return the number of bytes received
     */
    unsigned short IRQgetReceivedBytes1() const
    {
        return IRQgetReceivedBytes();
    }

    /**
     * Clear the CTR_TX bit.
     */
    void IRQclearTxInterruptFlag()
    {
        unsigned short reg=EPR;
        //Clear all toggle bits, so not to toggle any of them.
        //Additionally, clear CTR_TX
        reg &= ~(USB_EP0R_DTOG_RX | USB_EP0R_DTOG_TX | USB_EP0R_STAT_RX |
               USB_EP0R_STAT_TX | USB_EP0R_CTR_TX);
        //Explicitly set CTR_RX to avoid clearing it due to the read-modify-write op
        reg |= USB_EP0R_CTR_RX;
        EPR=reg;
    }

    /**
     * Clear the CTR_RX bit.
     */
    void IRQclearRxInterruptFlag()
    {
        unsigned short reg=EPR;
        //Clear all toggle bits, so not to toggle any of them.
        //Additionally, clear CTR_RX
        reg &= ~(USB_EP0R_DTOG_RX | USB_EP0R_DTOG_TX | USB_EP0R_STAT_RX |
               USB_EP0R_STAT_TX | USB_EP0R_CTR_RX);
        //Explicitly set CTR_TX to avoid clearing it due to the read-modify-write op
        reg |= USB_EP0R_CTR_TX;
        EPR=reg;
    }

    /**
     * Set the EP_KIND bit.
     */
    void IRQsetEpKind();

    /**
     * Clear the EP_KIND bit.
     */
    void IRQclearEpKind();
    
    /**
     * Set the DTOG_TX bit.
     * \param value if true bit will be set to 1, else to 0
     */
    void IRQsetDtogTx(bool value);

    /**
     * Optimized version of setDtogTx that toggles the bit
     */
    void IRQtoggleDtogTx()
    {
        unsigned short reg=EPR;
        //Clear all toggle bits except DTOG_TX, since we need to toggle it
        reg &= ~(USB_EP0R_DTOG_RX | USB_EP0R_STAT_RX | USB_EP0R_STAT_TX);
        //Avoid clearing an interrupt flag because of a read-modify-write
        reg |= USB_EP0R_CTR_RX | USB_EP0R_CTR_TX | USB_EP0R_DTOG_TX;
        EPR=reg;
    }

    /**
     * \return true if DTOG_TX is set
     */
    bool IRQgetDtogTx() const { return (EPR & USB_EP0R_DTOG_TX)!=0; }

    /**
     * Set the DTOG_RX bit.
     * \param value if true bit will be set to 1, else to 0
     */
    void IRQsetDtogRx(bool value);

    /**
     * Optimized version of setDtogRx that toggles the bit
     */
    void IRQtoggleDtogRx()
    {
        unsigned short reg=EPR;
        //Clear all toggle bits except DTOG_RX, since we need to toggle it
        reg &= ~(USB_EP0R_DTOG_TX | USB_EP0R_STAT_RX | USB_EP0R_STAT_TX);
        //Avoid clearing an interrupt flag because of a read-modify-write
        reg |= USB_EP0R_CTR_RX | USB_EP0R_CTR_TX | USB_EP0R_DTOG_RX;
        EPR=reg;
    }

    /**
     * \return true if DTOG_RX is set
     */
    bool IRQgetDtogRx() const { return (EPR & USB_EP0R_DTOG_RX)!=0; }

    /**
     * Allows to assign a value to the hardware register.
     * \param value value to be stored in the EPR register.
     */
    void operator= (unsigned short value)
    {
        EPR=value;
    }

    /**
     * Allows to read the hardware register directly.
     * \return the value of the EPR register
     */
    unsigned short get() const { return EPR; }

private:
    EndpointRegister(const EndpointRegister&);
    EndpointRegister& operator= (const EndpointRegister&);

    //Endpoint register. This class is meant to be overlayed to the hardware
    //register EPnR. Therefore it can't have any other data member other than
    //this register (and no virtual functions nor constructors/destructors)
    volatile unsigned int EPR;
};

} //namespace mxusb

#endif //ENDPOINT_H

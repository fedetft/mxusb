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

#include "endpoint_reg.h"

namespace mxusb {

//
// class Endpoint
//

void EndpointRegister::IRQsetType(Type type)
{
    unsigned short reg=EPR;
    //Clear all toggle bits, so not to toggle any of them
    reg &= ~(USB_EP0R_DTOG_RX | USB_EP0R_DTOG_TX | USB_EP0R_STAT_RX |
           USB_EP0R_STAT_TX);
    //Avoid clearing an interrupt flag because of a read-modify-write
    reg |= USB_EP0R_CTR_RX | USB_EP0R_CTR_TX;
    reg &= ~USB_EP0R_EP_TYPE;
    reg |= type;
    EPR=reg;
}

void EndpointRegister::IRQsetTxStatus(Status status)
{
    unsigned short reg=EPR;
    //Clear all toggle bits except STAT_TX, since we need to toggle STAT_TX
    reg &= ~(USB_EP0R_DTOG_RX | USB_EP0R_DTOG_TX | USB_EP0R_STAT_RX);
    //Avoid clearing an interrupt flag because of a read-modify-write
    reg |= USB_EP0R_CTR_RX | USB_EP0R_CTR_TX;
    if(status & (1<<0)) reg ^=USB_EP0R_STAT_TX_0;
    if(status & (1<<1)) reg ^=USB_EP0R_STAT_TX_1;
    EPR=reg;
}

void EndpointRegister::IRQsetRxStatus(Status status)
{
    unsigned short reg=EPR;
    //Clear all toggle bits except STAT_RX, since we need to toggle STAT_RX
    reg &= ~(USB_EP0R_DTOG_RX | USB_EP0R_DTOG_TX | USB_EP0R_STAT_TX);
    //Avoid clearing an interrupt flag because of a read-modify-write
    reg |= USB_EP0R_CTR_RX | USB_EP0R_CTR_TX;
    if(status & (1<<0)) reg ^=USB_EP0R_STAT_RX_0;
    if(status & (1<<1)) reg ^=USB_EP0R_STAT_RX_1;
    EPR=reg;
}

void EndpointRegister::IRQsetTxBuffer(shmem_ptr addr, unsigned short size)
{
    int ep=EPR & USB_EP0R_EA;
    SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+0)=addr & 0xfffe;
    SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+2)=size;
}

void EndpointRegister::IRQsetTxBuffer1(shmem_ptr addr, unsigned short size)
{
    int ep=EPR & USB_EP0R_EA;
    SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+4)=addr & 0xfffe;
    SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+6)=size;
}

void EndpointRegister::IRQsetRxBuffer(shmem_ptr addr, unsigned short size)
{
    int ep=EPR & USB_EP0R_EA;
    SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+4)=addr & 0xfffe;
    if(size>62)
    {
        size/=32;
        size--;
        size<<=10;
        size|=0x8000; //BL_SIZE=1
        SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+6)=size;
    } else {
        size<<=10;
        SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+6)=size;
    }
}

void EndpointRegister::IRQsetRxBuffer0(shmem_ptr addr, unsigned short size)
{
    int ep=EPR & USB_EP0R_EA;
    SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+0)=addr & 0xfffe;
    if(size>62)
    {
        size/=32;
        size--;
        size<<=10;
        size|=0x8000; //BL_SIZE=1
        SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+2)=size;
    } else {
        size<<=10;
        SharedMemory::shortAt(SharedMemory::BTABLE_ADDR+8*ep+2)=size;
    }
}

void EndpointRegister::IRQsetEpKind()
{
    unsigned short reg=EPR;
    //Clear all toggle bits, so not to toggle any of them
    reg &= ~(USB_EP0R_DTOG_RX | USB_EP0R_DTOG_TX | USB_EP0R_STAT_RX |
           USB_EP0R_STAT_TX);
    //Avoid clearing an interrupt flag because of a read-modify-write
    reg |= USB_EP0R_CTR_RX | USB_EP0R_CTR_TX;
    reg |= USB_EP0R_EP_KIND;
    EPR=reg;
}

void EndpointRegister::IRQclearEpKind()
{
    unsigned short reg=EPR;
    //Clear all toggle bits, so not to toggle any of them
    reg &= ~(USB_EP0R_DTOG_RX | USB_EP0R_DTOG_TX | USB_EP0R_STAT_RX |
           USB_EP0R_STAT_TX);
    //Avoid clearing an interrupt flag because of a read-modify-write
    reg |= USB_EP0R_CTR_RX | USB_EP0R_CTR_TX;
    reg &= ~USB_EP0R_EP_KIND;
    EPR=reg;
}

void EndpointRegister::IRQsetDtogTx(bool value)
{
    unsigned short reg=EPR;
    //Clear all toggle bits except DTOG_TX, since we need to toggle it
    reg &= ~(USB_EP0R_DTOG_RX | USB_EP0R_STAT_RX | USB_EP0R_STAT_TX);
    //Avoid clearing an interrupt flag because of a read-modify-write
    reg |= USB_EP0R_CTR_RX | USB_EP0R_CTR_TX;
    if(value) reg ^=USB_EP0R_DTOG_TX;
    EPR=reg;
}

void EndpointRegister::IRQsetDtogRx(bool value)
{
    unsigned short reg=EPR;
    //Clear all toggle bits except DTOG_RX, since we need to toggle it
    reg &= ~(USB_EP0R_DTOG_TX | USB_EP0R_STAT_RX | USB_EP0R_STAT_TX);
    //Avoid clearing an interrupt flag because of a read-modify-write
    reg |= USB_EP0R_CTR_RX | USB_EP0R_CTR_TX;
    if(value) reg ^=USB_EP0R_DTOG_RX;
    EPR=reg;
}

} //namespace mxusb

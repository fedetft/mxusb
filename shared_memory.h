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

#include <config/usb_config.h>

#ifndef SHARED_MEMORY_H
#define	SHARED_MEMORY_H

namespace mxusb {

///\internal
///Pointer to USB shared memory. Data is organized as 16bit integers, but
///aligned to 32bit boundaries, leaving 2 bytes gaps.
///Because of that, even if the access is performed as a pointer to int,
///the upper two bytes always read as zero
unsigned int* const USB_RAM=reinterpret_cast<unsigned int*>(0x40006000);

/**
 * \inetrnal
 * This type represents a pointer into the shared memory area.
 * Its values can range from 0 to SharedMemory::END-1
 * The pointer is a pointer to char *, so it can address individual bytes
 */
typedef unsigned short shmem_ptr;

/**
 * \internal
 * This class handles the memory area in stm32 microcontrollers that acts as a
 * dual-port RAM between the USB peripheral and the microcontroller.
 * This memory is used:
 * - to allocate the btable, the table with information about descriptors used
 *   by the USB peripheral
 * - to allocate the endpoint buffers
 *
 * The way this memory is handled is this:
 * - the btable is statically allocated at the bottom, with BTABLE_SIZE size
 * - then, the two buffer for endpoint 0 are statically allocated, with EP0_SIZE
 * - the rest of the memory is dynamically allocated ondemand when other
 *   endpoints are created. This memory cannot be deallocated, and is freed only
 *   when the USB device is reset or when device configuration is changed.
 */
class SharedMemory
{
public:

    /// Btable size. Must be 64 bytes to allow space for up to 8 endpoints
    static const unsigned short BTABLE_SIZE=64;
    /// Btable address. Must be 8bytes-aligned. Do not change
    static const shmem_ptr BTABLE_ADDR=0;

    /// Size of endpoint zero buffers (both tx and rx). Must be divisible by 2
    static const unsigned short EP0_SIZE=mxusb::EP0_SIZE; //in usb_config.h
    /// Address of tx buffer for endpoint zero (statically allocated)
    static const shmem_ptr EP0TX_ADDR=BTABLE_ADDR+BTABLE_SIZE;
    /// Address of rx buffer for endpoint zero (statically allocated)
    static const shmem_ptr EP0RX_ADDR=EP0TX_ADDR+EP0_SIZE;

    /// \internal base address of dynamic area
    static const shmem_ptr DYNAMIC_AREA=EP0RX_ADDR+EP0_SIZE;
    /// \internal address one past the last byte in the dynamic area
    static const shmem_ptr END=512;
    
    /**
     * Allocate space for an endpoint
     * \param size memory size required (in bytes)
     * \return the beginning of the allocated memory area. Returned pointer is
     * always two bytes aligned (last bit is zero). If not enough memory is
     * available, zero is returned
     */
    static shmem_ptr allocate(unsigned short size);

    /**
     * This invalidates all memory allocated with allocate
     */
    static void reset();

    /**
     * Copy data from the shared memory to RAM
     * \param dest pointer to a normal buffer already allocated in RAM
     * If n is an odd number, dest should be larger than n by at least one.
     * This is because for speed reasons this function might write one byte
     * more when n is odd.
     * \param src "pointer" into the shared memory, as returned by allocate()
     * Pointer must be two bytes aligned. Assuming for example that
     * result=allocate(n) was called, result, result+2, result+4 ..,
     * result+n-2 are valid pointers
     * \param n number of bytes to transfer
     */
    static void copyBytesFrom(unsigned char *dest, shmem_ptr src,
            unsigned short n);

    /**
     * Copy data from RAM to the shared memory
     * \param dest "pointer" into the shared memory, as returned by allocate()
     * Pointer must be two bytes aligned. Assuming for example that
     * result=allocate(n) was called, result, result+2, result+4 ..,
     * result+n-2 are valid pointers.<br>
     * Note: if n is an odd number, then due to hardware limitations on
     * byte accessibility of the shared memory, one byte past the input
     * buffer size will be copied to the shared memory.
     * \param src pointer to a normal buffer already allocated in RAM
     * \param n number of bytes to transfer
     */
    static void copyBytesTo(shmem_ptr dest, const unsigned char *src,
            unsigned short n);

    /**
     * Access a short int into an endpoint.
     * \param ptr pointer into shared memory. Pointer must be two bytes aligned.
     * Assuming for example that result=allocate(n) was called,
     * result, result+2, result+4 .., result+n-2 are valid pointers for that
     * endpoint. Due to restrictions on the underlying hardware, the returned
     * reference is to an int, but only the first two bytes are accessible.
     * \return a reference to read/write into that memory location.
     */
    static unsigned int& shortAt(shmem_ptr ptr)
    {
        return *(USB_RAM+(ptr>>1));
    }

    /**
     * Access a byte int into an endpoint.
     * \param ptr pointer into shared memory.
     * Assuming for example that result=allocate(n) was called,
     * result, result+1, result+2 .., result+n-1 are valid pointers for that
     * endpoint.
     * \return the byte value into that memory location. Note that write access
     * is not allowed on a byte basis. To write into the shared memory, use
     * shortAt()
     */
    static const unsigned char charAt(shmem_ptr ptr)
    {
        return *(reinterpret_cast<unsigned char *>(USB_RAM+(ptr>>1))+(ptr & 1));
    }

private:
    SharedMemory();

    static shmem_ptr currentEnd;/// Pointer to the first free byte
};

} //namespace mxusb

#endif //SHARED_MEMORY_H

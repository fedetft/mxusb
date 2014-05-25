##
## Makefile for mxusb
## This makefile builds libmxusb.a
##
MAKEFILE_VERSION := 1.04
## KPATH and CONFPATH are forwarded by the parent Makefile
include $(CONFPATH)/config/Makefile.inc

## List of all mxusb source files (both .c and .cpp)
## These files will end up in libmxusb.a
SRC :=                                                                     \
usb.cpp                                                                    \
ep0.cpp                                                                    \
usb_impl.cpp                                                               \
endpoint_reg.cpp                                                           \
def_ctrl_pipe.cpp                                                          \
shared_memory.cpp                                                          \
usb_tracer.cpp

## Replaces both "foo.cpp"-->"foo.o" and "foo.c"-->"foo.o"
OBJ := $(addsuffix .o, $(basename $(SRC)))

## Includes the miosix base directory for C/C++
CXXFLAGS := $(CXXFLAGS_BASE) -I$(CONFPATH) -I$(CONFPATH)/config/$(BOARD_INC) \
            -I. -I$(KPATH) -I$(KPATH)/arch/common -I$(KPATH)/$(ARCH_INC)     \
            -I$(KPATH)/$(BOARD_INC) -DMXUSB_LIBRARY
CFLAGS   := $(CFLAGS_BASE)   -I$(CONFPATH) -I$(CONFPATH)/config/$(BOARD_INC) \
            -I. -I$(KPATH) -I$(KPATH)/arch/common -I$(KPATH)/$(ARCH_INC)     \
            -I$(KPATH)/$(BOARD_INC) -DMXUSB_LIBRARY
AFLAGS   := $(AFLAGS_BASE)
DFLAGS   := -MMD -MP

## Build libmxusb.a
all: $(OBJ)
	$(AR) rcs libmxusb.a $(OBJ)

clean:
	rm $(OBJ) libmxusb.a $(OBJ:.o=.d)

%.o: %.s
	$(AS) $(AFLAGS) $< -o $@

%.o : %.c
	$(CC) $(DFLAGS) $(CFLAGS) $< -o $@

%.o : %.cpp
	$(CXX) $(DFLAGS) $(CXXFLAGS) $< -o $@

#pull in dependecy info for existing .o files
-include $(OBJ:.o=.d)

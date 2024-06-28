##
## Makefile for mxusb
## This makefile builds libmxusb.a
##

## KPATH and CONFPATH are forwarded by the parent Makefile
MAKEFILE_VERSION := 1.15
include $(KPATH)/Makefile.kcommon

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

CFLAGS   += -DMXUSB_LIBRARY
CXXFLAGS += -DMXUSB_LIBRARY

all: $(OBJ)
	$(ECHO) "[AR  ] libmxusb.a"
	$(Q)$(AR) rcs libmxusb.a $(OBJ)

clean:
	-rm -f $(OBJ) $(OBJ:.o=.d) libmxusb.a

-include $(OBJ:.o=.d)

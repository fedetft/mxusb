##
## Makefile for mxusb
## This makefile builds libmxusb.a
##
MAKEFILE_VERSION := 1.09
GCCMAJOR := $(shell arm-miosix-eabi-gcc --version | \
                    perl -e '$$_=<>;/\(GCC\) (\d+)/;print "$$1"')
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

ifeq ("$(VERBOSE)","1")
Q := 
ECHO := @true
else
Q := @
ECHO := @echo
endif

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
	$(ECHO) "[AR  ] libmxusb.a"
	$(Q)$(AR) rcs libmxusb.a $(OBJ)

clean:
	rm $(OBJ) libmxusb.a $(OBJ:.o=.d)

%.o: %.s
	$(ECHO) "[AS  ] $<"
	$(Q)$(AS)  $(AFLAGS) $< -o $@

%.o : %.c
	$(ECHO) "[CC  ] $<"
	$(Q)$(CC)  $(DFLAGS) $(CFLAGS) $< -o $@

%.o : %.cpp
	$(ECHO) "[CXX ] $<"
	$(Q)$(CXX) $(DFLAGS) $(CXXFLAGS) $< -o $@

#pull in dependecy info for existing .o files
-include $(OBJ:.o=.d)

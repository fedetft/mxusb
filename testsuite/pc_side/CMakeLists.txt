
cmake_minimum_required(VERSION 2.6)
project(USB_TEST)

## Target
set(TESTSUITE_SRCS testsuite.cpp libusbwrapper.cpp)
add_executable(usbtestsuite ${TESTSUITE_SRCS})

## Link libraries

## Find libusb1, taken from developer.intra2net.com/mailarchive/html/libftdi-git/2010/msg00028.html
find_package(PkgConfig)
pkg_check_modules(PC_LIBUSB libusb-1.0)
find_path(LIBUSB_INCLUDE_DIR libusb.h PATHS ${PC_LIBUSB_INCLUDEDIR} ${PC_LIBUSB_INCLUDE_DIRS})
find_library(LIBUSB_LIBRARIES NAMES usb-1.0 PATHS ${PC_LIBUSB_LIBDIR} ${PC_LIBUSB_LIBRARY_DIRS})

include_directories(${LIBUSB_INCLUDE_DIR})
target_link_libraries(usbtestsuite ${LIBUSB_LIBRARIES})

set(BOOST_LIBS date_time system)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(usbtestsuite ${Boost_LIBRARIES})

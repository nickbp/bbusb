cmake_minimum_required (VERSION 2.6)

project(bbusb)

find_path(usb-10_INCLUDE_DIR NAMES libusb-1.0/libusb.h)
find_library(usb-10_LIBRARY NAMES usb-1.0)
find_path(usb-01_INCLUDE_DIR NAMES usb.h)
find_library(usb-01_LIBRARY NAMES usb)

if(usb-10_INCLUDE_DIR AND usb-10_LIBRARY)
  message(STATUS "Found libusb-1.0")
else()
  message(STATUS "Didn't find libusb-1.0")
endif()
if(usb-01_INCLUDE_DIR AND usb-01_LIBRARY)
  message(STATUS "Found libusb-0.1")
else()
  message(STATUS "Didn't find libusb-0.1")
endif()

if(usb-10_INCLUDE_DIR AND usb-10_LIBRARY)
  set(FOUND_LIBUSB_10 ON)
elseif(usb-01_INCLUDE_DIR AND usb-01_LIBRARY)
  set(FOUND_LIBUSB_01 ON)
else()
  message(WARNING "Neither libusb-1.0 nor libusb-0.1 were found. Falling back to nousb.")
  set(FOUND_NOUSB ON)
endif()

option(USE_LIBUSB_10 "Use libusb-1.0" ${FOUND_LIBUSB_10})
option(USE_LIBUSB_01 "Use libusb-0.1" ${FOUND_LIBUSB_01})
option(USE_NOUSB "Disable usb" ${FOUND_NOUSB})

set(bbusb_VERSION_MAJOR 1)
set(bbusb_VERSION_MINOR 0)
set(bbusb_VERSION_PATCH 0)

set(SRCS
  config.in.h
  config.c
  hardware.h
  hardware.c
  infile.h
  infile.c
  main.c
  packet.h
  packet.c
  usbsign.h
  #tech-specific usbsign.c's added below
  )
configure_file (
  "${PROJECT_SOURCE_DIR}/config.in.h"
  "${PROJECT_BINARY_DIR}/config.h"
  )

if(USE_LIBUSB_10) # libusb-1.0

  message(STATUS "Using libusb-1.0")
  list(APPEND INCLUDES ${usb-10_INCLUDE_DIR})
  list(APPEND LIBS ${usb-10_LIBRARY})
  list(APPEND SRCS usbsign-newusb.c)

elseif(USE_LIBUSB_01) # libusb-0.1

  message(STATUS "Using libusb-0.1")
  list(APPEND INCLUDES ${usb-01_INCLUDE_DIR})
  list(APPEND LIBS ${usb-01_LIBRARY})
  list(APPEND SRCS usbsign-oldusb.c)

elseif(USE_NOUSB) # nousb (debug printf)

  message(STATUS "Using nousb")
  list(APPEND SRCS usbsign-nousb.c)

else()

  message(FATAL_ERROR "At least one USE_USB mode must be selected.")

endif()


include_directories(${PROJECT_BINARY_DIR} ${INCLUDES})
add_executable(bbusb ${SRCS})
target_link_libraries(bbusb ${LIBS})


include (InstallRequiredSystemLibraries)
set (CPACK_RESOURCE_FILE_LICENSE
  "${CMAKE_CURRENT_SOURCE_DIR}/../LICENCE")
set (CPACK_PACKAGE_VERSION_MAJOR "${bbusb_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${bbusb_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${bbusb_VERSION_PATCH}")
include (CPack)

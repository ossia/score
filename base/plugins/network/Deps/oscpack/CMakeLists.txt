cmake_minimum_required(VERSION 2.8)
PROJECT(TestOscpack)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

# separate versions of NetworkingUtils.cpp and UdpSocket.cpp are provided for Win32 and POSIX
# the IpSystemTypePath selects the correct ones based on the current platform

IF(WIN32)
 set(IpSystemTypePath ip/win32)
 set(LIBS ${LIBS} Ws2_32 winmm)
ELSE(WIN32)
 set(IpSystemTypePath ip/posix)
ENDIF(WIN32)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
ADD_LIBRARY(oscpack 

${CMAKE_CURRENT_SOURCE_DIR}/ip/IpEndpointName.h
${CMAKE_CURRENT_SOURCE_DIR}/ip/IpEndpointName.cpp

${CMAKE_CURRENT_SOURCE_DIR}/ip/NetworkingUtils.h
${CMAKE_CURRENT_SOURCE_DIR}/${IpSystemTypePath}/NetworkingUtils.cpp

${CMAKE_CURRENT_SOURCE_DIR}/ip/UdpSocket.h
${CMAKE_CURRENT_SOURCE_DIR}/${IpSystemTypePath}/UdpSocket.cpp

${CMAKE_CURRENT_SOURCE_DIR}/ip/PacketListener.h
${CMAKE_CURRENT_SOURCE_DIR}/ip/TimerListener.h

${CMAKE_CURRENT_SOURCE_DIR}/osc/OscTypes.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscTypes.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscHostEndianness.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscException.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscPacketListener.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/MessageMappingOscPacketListener.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscReceivedElements.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscReceivedElements.cpp
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscPrintReceivedElements.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscPrintReceivedElements.cpp
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscOutboundPacketStream.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/OscOutboundPacketStream.cpp

)

install(TARGETS oscpack DESTINATION ${CMAKE_BINARY_DIR})

#~ 
#~ ADD_EXECUTABLE(OscUnitTests tests/OscUnitTests.cpp)
#~ TARGET_LINK_LIBRARIES(OscUnitTests oscpack ${LIBS})
#~ 
#~ ADD_EXECUTABLE(OscSendTests tests/OscSendTests.cpp)
#~ TARGET_LINK_LIBRARIES(OscSendTests oscpack ${LIBS})
#~ 
#~ ADD_EXECUTABLE(OscReceiveTest tests/OscReceiveTest.cpp)
#~ TARGET_LINK_LIBRARIES(OscReceiveTest oscpack ${LIBS})
#~ 
#~ 
#~ ADD_EXECUTABLE(OscDump examples/OscDump.cpp)
#~ TARGET_LINK_LIBRARIES(OscDump oscpack ${LIBS})
#~ 
#~ ADD_EXECUTABLE(SimpleReceive examples/SimpleReceive.cpp)
#~ TARGET_LINK_LIBRARIES(SimpleReceive oscpack ${LIBS})
#~ 
#~ ADD_EXECUTABLE(SimpleSend examples/SimpleSend.cpp)
#~ TARGET_LINK_LIBRARIES(SimpleSend oscpack ${LIBS})
#~ 

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-long-long -pedantic")
endif()

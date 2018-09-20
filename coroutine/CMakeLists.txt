#cmake file for project kernel
#author:ooeyusea
#created:2015-07-08 09:38:00

cmake_minimum_required(VERSION 2.8)
PROJECT(coroutine)
IF(UNIX)
    SET(CMAKE_CXX_COMPILER "/usr/bin/g++")
    SET(CMAKE_CXX_FLAGS "-Wall -std=c++17 -lpthread -lcurl -rdynamic -ldl -Dlinux -Wl,-rpath,./")
    SET(CMAKE_CXX_FLAGS_DEBUG "-g -D_DEBUG -O0 -pg")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -DNDEBUG -O2")
	SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3")
    MESSAGE(STATUS "Linux")
ENDIF(UNIX)

IF(WIN32)
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
    SET(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /D_WINSOCK_DEPRECATED_NO_WARNINGS /Zm200 /MTd /Zi /Ob0 /Od /RTC1") 
    SET(CMAKE_CXX_FLAGS_MINSIZEREL "/D_WINSOCK_DEPRECATED_NO_WARNINGS /MT /Zm200 /O1 /Ob1 /D NDEBUG") 
    SET(CMAKE_CXX_FLAGS_RELEASE "/D_WINSOCK_DEPRECATED_NO_WARNINGS /MT /Zm200 /O2 /Ob2 /D NDEBUG") 
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/D_WINSOCK_DEPRECATED_NO_WARNINGS /MT /Zm200 /Zi /O2 /Ob1 /D NDEBUG")
    MESSAGE(STATUS "WINDOWS")
    IF(CMAKE_CL_64)
        MESSAGE(STATUS "x64")
    ELSE()
        MESSAGE(STATUS "x86")
    ENDIF(CMAKE_CL_64)
	SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS On)
ENDIF(WIN32)

INCLUDE_DIRECTORIES(
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src/coroutine"
	"${PROJECT_SOURCE_DIR}/src/timer"
	"${PROJECT_SOURCE_DIR}/src/options"
	"${PROJECT_SOURCE_DIR}/src/net"
	"${PROJECT_SOURCE_DIR}/src/comutex"
	"${PROJECT_SOURCE_DIR}/src/public"
)

IF(WIN32)
INCLUDE_DIRECTORIES(
	"${PROJECT_SOURCE_DIR}/src/net/windows"
)
ENDIF(WIN32)

SET(SRC
    "${PROJECT_SOURCE_DIR}/include/hnet.h"
	"${PROJECT_SOURCE_DIR}/src/main.cpp"
)

SET(COROUTINE
    "${PROJECT_SOURCE_DIR}/src/coroutine/coroutine.h"
	"${PROJECT_SOURCE_DIR}/src/coroutine/coroutine.cpp"
    "${PROJECT_SOURCE_DIR}/src/coroutine/scheduler.h"
	"${PROJECT_SOURCE_DIR}/src/coroutine/scheduler.cpp"
    "${PROJECT_SOURCE_DIR}/src/coroutine/processer.h"
	"${PROJECT_SOURCE_DIR}/src/coroutine/processer.cpp"
)

SET(COMUTEX
    "${PROJECT_SOURCE_DIR}/src/comutex/comutex.h"
	"${PROJECT_SOURCE_DIR}/src/comutex/comutex.cpp"
)

SET(TIMER
    "${PROJECT_SOURCE_DIR}/src/timer/timer.h"
	"${PROJECT_SOURCE_DIR}/src/timer/timer.cpp"
    "${PROJECT_SOURCE_DIR}/src/timer/timer_detail.h"
	"${PROJECT_SOURCE_DIR}/src/timer/timer_detail.cpp"
)

SET(NET
    "${PROJECT_SOURCE_DIR}/src/net/net.h"
)

IF(WIN32)
SET(NET_IO
    "${PROJECT_SOURCE_DIR}/src/net/windows/net.cpp"
)
ENDIF(WIN32)

SET(PUBLIC
    "${PROJECT_SOURCE_DIR}/src/public/util.h"
	"${PROJECT_SOURCE_DIR}/src/public/util.cpp"
	"${PROJECT_SOURCE_DIR}/src/public/slist.h"
)

SET(OPTIONS
    "${PROJECT_SOURCE_DIR}/src/options/options.h"
	"${PROJECT_SOURCE_DIR}/src/options/options.cpp"
)

SOURCE_GROUP(public FILES ${PUBLIC})
SOURCE_GROUP(coroutine FILES ${COROUTINE})
SOURCE_GROUP(options FILES ${OPTIONS})
SOURCE_GROUP(net FILES ${NET})
IF(WIN32)
SOURCE_GROUP(net\\windows FILES ${NET_IO})
ENDIF(WIN32)
SOURCE_GROUP(timer FILES ${TIMER})
SOURCE_GROUP(comutex FILES ${COMUTEX})
SOURCE_GROUP(\\ FILES ${SRC})

ADD_LIBRARY(hnet STATIC
	${SRC}
	${COROUTINE}
	${NET}
	${NET_IO}
	${TIMER}
	${COMUTEX}
	${PUBLIC}
	${OPTIONS}
)

SET(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/../bin")
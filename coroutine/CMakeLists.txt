#cmake file for project kernel
#author:ooeyusea
#created:2015-07-08 09:38:00

cmake_minimum_required(VERSION 2.8)
PROJECT(coroutine)

IF(UNIX)
	ENABLE_LANGUAGE(ASM)
	OPTION(USE_FCONTEXT "use boost context" On)
    SET(CMAKE_CXX_COMPILER "/usr/bin/g++")
    SET(CMAKE_CXX_FLAGS "-Wall -std=c++17 -lpthread -lcurl -rdynamic -ldl -Dlinux -Wl,-rpath,./ -DUSE_FCONTEXT")
    SET(CMAKE_CXX_FLAGS_DEBUG "-g -D_DEBUG -O0 -pg")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -DNDEBUG -O2")
	SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3")
    MESSAGE(STATUS "Linux")
ENDIF(UNIX)

IF(WIN32)
	ENABLE_LANGUAGE(ASM_MASM)
	OPTION(USE_FCONTEXT "use boost context" Off)
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	IF(${USE_FCONTEXT} STREQUAL "ON")
		SET(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /DUSE_FCONTEXT /D_WINSOCK_DEPRECATED_NO_WARNINGS /Zm200 /MTd /Zi /Ob0 /Od /RTC1") 
		SET(CMAKE_CXX_FLAGS_MINSIZEREL "/DUSE_FCONTEXT /D_WINSOCK_DEPRECATED_NO_WARNINGS /MT /Zm200 /O1 /Ob1 /D NDEBUG") 
		SET(CMAKE_CXX_FLAGS_RELEASE "/DUSE_FCONTEXT /D_WINSOCK_DEPRECATED_NO_WARNINGS /MT /Zm200 /O2 /Ob2 /D NDEBUG") 
		SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/DUSE_FCONTEXT /D_WINSOCK_DEPRECATED_NO_WARNINGS /MT /Zm200 /Zi /O2 /Ob1 /D NDEBUG")
	ELSE()
		SET(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /D_WINSOCK_DEPRECATED_NO_WARNINGS /Zm200 /MTd /Zi /Ob0 /Od /RTC1") 
		SET(CMAKE_CXX_FLAGS_MINSIZEREL "/D_WINSOCK_DEPRECATED_NO_WARNINGS /MT /Zm200 /O1 /Ob1 /D NDEBUG") 
		SET(CMAKE_CXX_FLAGS_RELEASE "/D_WINSOCK_DEPRECATED_NO_WARNINGS /MT /Zm200 /O2 /Ob2 /D NDEBUG") 
		SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/D_WINSOCK_DEPRECATED_NO_WARNINGS /MT /Zm200 /Zi /O2 /Ob1 /D NDEBUG")
	ENDIF()
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
	"${PROJECT_SOURCE_DIR}/src/async"
	"${PROJECT_SOURCE_DIR}/src/public"
	"${PROJECT_SOURCE_DIR}/src/logger"
	"${PROJECT_SOURCE_DIR}/../thirdparty/boost/include"
	"${PROJECT_SOURCE_DIR}/../thirdparty/spdlog/include"
)

IF(WIN32)
INCLUDE_DIRECTORIES(
	"${PROJECT_SOURCE_DIR}/src/net/windows"
)
ENDIF(WIN32)

IF(UNIX)
INCLUDE_DIRECTORIES(
	"${PROJECT_SOURCE_DIR}/src/net/linux"
)
ENDIF(UNIX)

SET(SRC
    "${PROJECT_SOURCE_DIR}/include/hnet.h"
	"${PROJECT_SOURCE_DIR}/include/hnet_net.h"
	"${PROJECT_SOURCE_DIR}/include/hnet_time.h"
	"${PROJECT_SOURCE_DIR}/include/hnet_mutex.h"
	"${PROJECT_SOURCE_DIR}/include/hnet_async.h"
	"${PROJECT_SOURCE_DIR}/include/hnet_channel.h"
	"${PROJECT_SOURCE_DIR}/include/hnet_stream.h"
	"${PROJECT_SOURCE_DIR}/include/hnet_rpc.h"
	"${PROJECT_SOURCE_DIR}/include/hnet_logger.h"
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

IF(USE_FCONTEXT)
INCLUDE_DIRECTORIES(
	"${PROJECT_SOURCE_DIR}/src/coroutine/fcontext"
)

SET(FCONTEXT
	"${PROJECT_SOURCE_DIR}/src/coroutine/fcontext/fcontext.hpp"
)

IF(WIN32)
SET(FCONTEXT_ASM
	"${PROJECT_SOURCE_DIR}/src/coroutine/fcontext/asm/make_x86_64_ms_pe_masm.asm"
	"${PROJECT_SOURCE_DIR}/src/coroutine/fcontext/asm/jump_x86_64_ms_pe_masm.asm"
)
ENDIF(WIN32)

IF(UNIX)
SET(FCONTEXT_ASM
	"${PROJECT_SOURCE_DIR}/src/coroutine/fcontext/asm/make_x86_64_sysv_elf_gas.S"
	"${PROJECT_SOURCE_DIR}/src/coroutine/fcontext/asm/jump_x86_64_sysv_elf_gas.S"
)
ENDIF(UNIX)
ELSE()
SET(FCONTEXT
)
SET(FCONTEXT_ASM
)
ENDIF(USE_FCONTEXT)

SET(COMUTEX
    "${PROJECT_SOURCE_DIR}/src/comutex/comutex.h"
	"${PROJECT_SOURCE_DIR}/src/comutex/comutex.cpp"
)

SET(LOGGER
    "${PROJECT_SOURCE_DIR}/src/logger/logger.h"
	"${PROJECT_SOURCE_DIR}/src/logger/logger.cpp"
)

SET(COCHANNEL
    "${PROJECT_SOURCE_DIR}/src/channel/cochannel.h"
	"${PROJECT_SOURCE_DIR}/src/channel/cochannel.cpp"
)

SET(CORPC
    "${PROJECT_SOURCE_DIR}/src/rpc/corpc.h"
	"${PROJECT_SOURCE_DIR}/src/rpc/corpc.cpp"
)

SET(TIMER
    "${PROJECT_SOURCE_DIR}/src/timer/timer.h"
	"${PROJECT_SOURCE_DIR}/src/timer/timer.cpp"
    "${PROJECT_SOURCE_DIR}/src/timer/timer_detail.h"
	"${PROJECT_SOURCE_DIR}/src/timer/timer_detail.cpp"
)

SET(ASYNC
    "${PROJECT_SOURCE_DIR}/src/async/async_queue.h"
	"${PROJECT_SOURCE_DIR}/src/async/async_queue.cpp"
)

IF(WIN32)
SET(NET
	"${PROJECT_SOURCE_DIR}/src/net/windows/net.h"
    "${PROJECT_SOURCE_DIR}/src/net/windows/net.cpp"
)
ENDIF(WIN32)

IF(UNIX)
SET(NET
	"${PROJECT_SOURCE_DIR}/src/net/linux/net.h"
    "${PROJECT_SOURCE_DIR}/src/net/linux/net.cpp"
)
ENDIF(UNIX)

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
IF(USE_FCONTEXT)
SOURCE_GROUP(coroutine\\fcontext FILES ${FCONTEXT})
SOURCE_GROUP(coroutine\\fcontext\\asm FILES ${FCONTEXT_ASM})
ENDIF(USE_FCONTEXT)
SOURCE_GROUP(options FILES ${OPTIONS})
SOURCE_GROUP(net FILES ${NET})
SOURCE_GROUP(timer FILES ${TIMER})
SOURCE_GROUP(comutex FILES ${COMUTEX})
SOURCE_GROUP(channel FILES ${COCHANNEL})
SOURCE_GROUP(async FILES ${ASYNC})
SOURCE_GROUP(rpc FILES ${CORPC})
SOURCE_GROUP(logger FILES ${LOGGER})
SOURCE_GROUP(\\ FILES ${SRC})

ADD_LIBRARY(hnet STATIC
	${SRC}
	${COROUTINE}
	${NET}
	${TIMER}
	${COMUTEX}
	${ASYNC}
	${PUBLIC}
	${OPTIONS}
	${COCHANNEL}
	${CORPC}
	${LOGGER}
	${FCONTEXT}
	${FCONTEXT_ASM}
)

SET(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/../bin")
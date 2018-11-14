#cmake file for project kernel
#author:ooeyusea
#created:2015-07-08 09:38:00

cmake_minimum_required(VERSION 2.8)
PROJECT(gate)
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
    "${PROJECT_SOURCE_DIR}/../../coroutine/include"
	"${PROJECT_SOURCE_DIR}/../common"
	"${PROJECT_SOURCE_DIR}/../common/http"
	"${PROJECT_SOURCE_DIR}/../common/argument"
	"${PROJECT_SOURCE_DIR}/../common/websocket"
	"${PROJECT_SOURCE_DIR}/../common/sha1"
	"${PROJECT_SOURCE_DIR}/../common/md5"
	"${PROJECT_SOURCE_DIR}/../common/base64"
	"${PROJECT_SOURCE_DIR}/../common/node"
	"${PROJECT_SOURCE_DIR}/../common/xml"
	"${PROJECT_SOURCE_DIR}/../common/xml/tinyxml"
)

LINK_DIRECTORIES(
	"${PROJECT_SOURCE_DIR}/../../thirdparty/boost/lib"
)

SET(ARGUMENT
	"${PROJECT_SOURCE_DIR}/../common/argument/argument.h"
	"${PROJECT_SOURCE_DIR}/../common/argument/argument.cpp"
)

SET(WEBSOCKET
	"${PROJECT_SOURCE_DIR}/../common/websocket/websocket.h"
	"${PROJECT_SOURCE_DIR}/../common/websocket/websocket.cpp"
)

SET(WS_SHA1
	"${PROJECT_SOURCE_DIR}/../common/sha1/sha1.cpp"
	"${PROJECT_SOURCE_DIR}/../common/sha1/sha1.h"
)

SET(WS_MD5
	"${PROJECT_SOURCE_DIR}/../common/md5/md5.c"
	"${PROJECT_SOURCE_DIR}/../common/md5/md5.h"
	"${PROJECT_SOURCE_DIR}/../common/md5/md5.hpp"
)

SET(WS_BASE64
	"${PROJECT_SOURCE_DIR}/../common/base64/base64.cpp"
	"${PROJECT_SOURCE_DIR}/../common/base64/base64.h"
)

SET(SERVERNODE
	"${PROJECT_SOURCE_DIR}/../common/node/servernode.h"
	"${PROJECT_SOURCE_DIR}/../common/node/servernode.cpp"
)

SET(XML
	"${PROJECT_SOURCE_DIR}/../common/xml/XmlReader.h"
	"${PROJECT_SOURCE_DIR}/../common/xml/XmlReader.cpp"
)

SET(TINYXML
	"${PROJECT_SOURCE_DIR}/../common/xml/tinyxml/tinystr.cpp"
	"${PROJECT_SOURCE_DIR}/../common/xml/tinyxml/tinystr.h"
	"${PROJECT_SOURCE_DIR}/../common/xml/tinyxml/tinyxml.cpp"
	"${PROJECT_SOURCE_DIR}/../common/xml/tinyxml/tinyxml.h"
	"${PROJECT_SOURCE_DIR}/../common/xml/tinyxml/tinyxmlerror.cpp"
	"${PROJECT_SOURCE_DIR}/../common/xml/tinyxml/tinyxmlparser.cpp"
)


SET(SRC
	"${PROJECT_SOURCE_DIR}/main.cpp"
	"${PROJECT_SOURCE_DIR}/Gate.cpp"
	"${PROJECT_SOURCE_DIR}/Gate.h"
	"${PROJECT_SOURCE_DIR}/Session.cpp"
	"${PROJECT_SOURCE_DIR}/Session.h"
	"${PROJECT_SOURCE_DIR}/../common/util.h"
	"${PROJECT_SOURCE_DIR}/../common/nodedefine.h"
	"${PROJECT_SOURCE_DIR}/../common/rpcdefine.h"
	"${PROJECT_SOURCE_DIR}/../common/clientdefine.h"
)

SOURCE_GROUP(\\ FILES ${SRC})
SOURCE_GROUP(argument FILES ${ARGUMENT})
SOURCE_GROUP(websocket FILES ${WEBSOCKET})
SOURCE_GROUP(sha1 FILES ${WS_SHA1})
SOURCE_GROUP(md5 FILES ${WS_MD5})
SOURCE_GROUP(base64 FILES ${WS_BASE64})
SOURCE_GROUP(node FILES ${SERVERNODE})
SOURCE_GROUP(xml FILES ${XML})
SOURCE_GROUP(xml\\tinyxml FILES ${TINYXML})

ADD_EXECUTABLE(gate
	${SRC}
	${ARGUMENT}
	${WEBSOCKET}
	${WS_SHA1}
	${WS_MD5}
	${WS_BASE64}
	${SERVERNODE}
	${XML}
	${TINYXML}
)

IF(WIN32)
TARGET_LINK_LIBRARIES(gate
	"${PROJECT_SOURCE_DIR}/../../bin/$<CONFIGURATION>/hnet.lib"
)
SET_PROPERTY(TARGET gate PROPERTY FOLDER "chief")
ENDIF(WIN32)

IF(UNIX)
TARGET_LINK_LIBRARIES(gate
	"${PROJECT_SOURCE_DIR}/../../bin/libhnet.a"
	"${PROJECT_SOURCE_DIR}/../../thirdparty/boost/lib/libboost_context.a"
)
ENDIF(UNIX)

SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/../../bin")
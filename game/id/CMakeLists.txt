#cmake file for project kernel
#author:ooeyusea
#created:2015-07-08 09:38:00

cmake_minimum_required(VERSION 2.8)
PROJECT(logic)
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
	"${PROJECT_SOURCE_DIR}/../../thirdparty/mysql/include"
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
	"${PROJECT_SOURCE_DIR}/../common/locktable"
	"${PROJECT_SOURCE_DIR}/../common/initializor"
	"${PROJECT_SOURCE_DIR}/../common/framework"
	"${PROJECT_SOURCE_DIR}/../common/mysql"
	"${PROJECT_SOURCE_DIR}/"
)

LINK_DIRECTORIES(
	"${PROJECT_SOURCE_DIR}/../../thirdparty/boost/lib"
)

SET(ARGUMENT
	"${PROJECT_SOURCE_DIR}/../common/argument/argument.h"
	"${PROJECT_SOURCE_DIR}/../common/argument/argument.cpp"
)

SET(SERVERNODE
	"${PROJECT_SOURCE_DIR}/../common/node/servernode.h"
	"${PROJECT_SOURCE_DIR}/../common/node/servernode.cpp"
	"${PROJECT_SOURCE_DIR}/../common/node/zone.h"
	"${PROJECT_SOURCE_DIR}/../common/node/zone.cpp"
)

SET(LOCKTABLE
	"${PROJECT_SOURCE_DIR}/../common/locktable/locktable.h"
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

SET(INITIALIZOR
	"${PROJECT_SOURCE_DIR}/../common/initializor/initializor.h"
	"${PROJECT_SOURCE_DIR}/../common/initializor/initializor.cpp"
)

SET(MYSQL
	"${PROJECT_SOURCE_DIR}/../common/mysql/mysqlmgr.h"
	"${PROJECT_SOURCE_DIR}/../common/mysql/mysqlmgr.cpp"
)

SET(FRAMEWORK
	"${PROJECT_SOURCE_DIR}/../common/framework/framework.h"
	"${PROJECT_SOURCE_DIR}/../common/framework/framework.cpp"
)

SET(SRC
	"${PROJECT_SOURCE_DIR}/idmgr.h"
	"${PROJECT_SOURCE_DIR}/idmgr.cpp"
	"${PROJECT_SOURCE_DIR}/../common/util.h"
	"${PROJECT_SOURCE_DIR}/../common/nodedefine.h"
	"${PROJECT_SOURCE_DIR}/../common/rpcdefine.h"
)

SOURCE_GROUP(\\ FILES ${SRC})
SOURCE_GROUP(argument FILES ${ARGUMENT})
SOURCE_GROUP(node FILES ${SERVERNODE})
SOURCE_GROUP(locktable FILES ${LOCKTABLE})
SOURCE_GROUP(xml FILES ${XML})
SOURCE_GROUP(xml\\tinyxml FILES ${TINYXML})
SOURCE_GROUP(initializor FILES ${INITIALIZOR})
SOURCE_GROUP(framework FILES ${FRAMEWORK})
SOURCE_GROUP(mysql FILES ${MYSQL})

ADD_EXECUTABLE(id
	${SRC}
	${ARGUMENT}
	${SERVERNODE}
	${XML}
	${TINYXML}
	${INITIALIZOR}
	${FRAMEWORK}
	${MYSQL}
)

IF(WIN32)
TARGET_LINK_LIBRARIES(id
	"${PROJECT_SOURCE_DIR}/../../bin/$<CONFIGURATION>/hnet.lib"
	"${PROJECT_SOURCE_DIR}/../../thirdparty/mysql/lib/libmysql.lib"
)
SET_PROPERTY(TARGET id PROPERTY FOLDER "chief")
ENDIF(WIN32)

IF(UNIX)
TARGET_LINK_LIBRARIES(id
	"${PROJECT_SOURCE_DIR}/../../bin/libhnet.a"
	"${PROJECT_SOURCE_DIR}/../../thirdparty/boost/lib/libboost_context.a"
)
ENDIF(UNIX)

SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/../../bin")
cmake_minimum_required(VERSION 3.16)
project(nimrod VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets WebChannel Sql)
find_package(OpenSSL REQUIRED)

# resources/autofill.js elérési útja fordítási időben
add_compile_definitions(NIMROD_RESOURCES_DIR="${CMAKE_SOURCE_DIR}/resources")

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/browsertab.cpp
    src/configmanager.cpp
    src/cookiestore.cpp
    src/credentialstore.cpp
    src/nimrodbridge.cpp
    src/jsconsole.cpp
    src/scriptmanager.cpp
)

set(HEADERS
    src/mainwindow.h
    src/browsertab.h
    src/configmanager.h
    src/cookiestore.h
    src/credentialstore.h
    src/nimrodbridge.h
    src/jsconsole.h
    src/scriptmanager.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    Qt6::Sql
    OpenSSL::SSL
    OpenSSL::Crypto
)

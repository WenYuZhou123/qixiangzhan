cmake_minimum_required(VERSION 3.21)

project(relay_qt_client VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QXZ_CLIENT_DEFAULT_API_BASE "http://127.0.0.1:8000/api/v1" CACHE STRING "Default API base used by the Qt client")
if(DEFINED ENV{QXZ_CLIENT_DEFAULT_API_BASE} AND NOT "$ENV{QXZ_CLIENT_DEFAULT_API_BASE}" STREQUAL "")
    set(QXZ_CLIENT_DEFAULT_API_BASE "$ENV{QXZ_CLIENT_DEFAULT_API_BASE}" CACHE STRING "Default API base used by the Qt client" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Mqtt Network Sql)
find_package(Qt6 6.5 COMPONENTS SerialPort QUIET)
find_package(Qt6 6.5 COMPONENTS WebSockets QUIET)

qt_standard_project_setup(REQUIRES 6.5)

set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)
set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/qtkeychain third_party/qtkeychain-build)

qt_add_executable(relay_qt_client
    src/main.cpp
    src/app/AppController.h
    src/app/AppController.cpp
    src/auth/AuthSession.h
    src/auth/AuthSession.cpp
    src/data/SQLiteCache.h
    src/data/SQLiteCache.cpp
    src/data/SecureCredentialStore.h
    src/data/SecureCredentialStore.cpp
    src/data/ApiClient.h
    src/data/ApiClient.cpp
    src/data/DeviceRepository.h
    src/data/DeviceRepository.cpp
    src/data/HistoryRepository.h
    src/data/HistoryRepository.cpp
    src/data/AlarmRepository.h
    src/data/AlarmRepository.cpp
    src/data/RemoteSyncService.h
    src/data/RemoteSyncService.cpp
    src/data/UserAdminService.h
    src/data/UserAdminService.cpp
    src/domain/CommandTracker.h
    src/domain/CommandTracker.cpp
    src/domain/DeviceStateStore.h
    src/domain/DeviceStateStore.cpp
    src/models/DeviceModels.h
    src/realtime/RealtimeGateway.h
    src/realtime/RealtimeGateway.cpp
    src/serial/SerialConsoleService.h
    src/serial/SerialConsoleService.cpp
)

qt_add_qml_module(relay_qt_client
    URI RelayClient
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ui/AlarmCenterPage.qml
        qml/ui/CommandHistoryPage.qml
        qml/ui/ConnectionPage.qml
        qml/ui/ControlPage.qml
        qml/ui/DeviceOverviewPage.qml
        qml/ui/DevicePage.qml
        qml/ui/EngineeringPage.qml
        qml/ui/LoginPage.qml
        qml/ui/LogPanel.qml
        qml/ui/SettingsPage.qml
        qml/ui/SerialConsolePage.qml
        qml/ui/TaskTheme.qml
        qml/ui/UserManagementPage.qml
        qml/ui/WeatherPage.qml
)

target_include_directories(relay_qt_client PRIVATE src)
target_include_directories(relay_qt_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qtkeychain
    ${CMAKE_CURRENT_BINARY_DIR}/third_party/qtkeychain-build
)

target_link_libraries(relay_qt_client
    PRIVATE
        Qt6::Quick
        Qt6::Mqtt
        Qt6::Network
        Qt6::Sql
        qt6keychain
)

target_compile_definitions(relay_qt_client PRIVATE QXZ_CLIENT_DEFAULT_API_BASE="${QXZ_CLIENT_DEFAULT_API_BASE}")

if (Qt6SerialPort_FOUND)
    target_link_libraries(relay_qt_client PRIVATE Qt6::SerialPort)
    target_compile_definitions(relay_qt_client PRIVATE RELAY_CLIENT_HAS_SERIALPORT=1)
else()
    message(WARNING "Qt6 SerialPort was not found. Serial diagnostics will run in stub mode.")
endif()

if (Qt6WebSockets_FOUND)
    target_link_libraries(relay_qt_client PRIVATE Qt6::WebSockets)
    target_compile_definitions(relay_qt_client PRIVATE RELAY_CLIENT_HAS_WEBSOCKETS=1)
else()
    message(WARNING "Qt6 WebSockets was not found. Remote realtime will fall back to REST polling.")
endif()

set_target_properties(relay_qt_client PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(ANDROID)
    set_target_properties(relay_qt_client PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()

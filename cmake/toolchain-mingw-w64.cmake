
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_TRIPLET x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${MINGW_TRIPLET}-gcc)
set(CMAKE_CXX_COMPILER ${MINGW_TRIPLET}-g++)
set(CMAKE_RC_COMPILER ${MINGW_TRIPLET}-windres)

if(NOT DEFINED QT_MINGW_PREFIX)
    set(QT_MINGW_PREFIX "/opt/qt/6.8.3/mingw_64")
endif()

# moc/uic/rcc must run on the host (Linux) even though we link against the
# Windows Qt libraries below. QT_HOST_PATH must point at a *matching-version*
# native Qt install, otherwise Qt's dependency resolution rejects it as
# incompatible and silently falls back to the (non-runnable) Windows tools.
if(NOT DEFINED QT_HOST_PATH)
    set(QT_HOST_PATH "/opt/qt/6.8.3/gcc_64" CACHE PATH "Host Qt install used for moc/uic/rcc" FORCE)
endif()
if(NOT DEFINED QT_HOST_PATH_CMAKE_DIR)
    set(QT_HOST_PATH_CMAKE_DIR "${QT_HOST_PATH}/lib/cmake" CACHE PATH "Host Qt cmake package dir" FORCE)
endif()

# Belt-and-braces: pin the tool package configs to the host Qt install
# directly in case the QT_HOST_PATH auto-swap doesn't take effect.
foreach(_qt_tools_pkg Qt6CoreTools Qt6GuiTools Qt6WidgetsTools)
    if(EXISTS "${QT_HOST_PATH_CMAKE_DIR}/${_qt_tools_pkg}")
        set(${_qt_tools_pkg}_DIR "${QT_HOST_PATH_CMAKE_DIR}/${_qt_tools_pkg}" CACHE PATH "" FORCE)
    endif()
endforeach()

set(CMAKE_PREFIX_PATH ${QT_MINGW_PREFIX} ${CMAKE_PREFIX_PATH})
set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_TRIPLET} ${QT_MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

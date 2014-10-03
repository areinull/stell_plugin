CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

########### Project name ###########
PROJECT(StellPlugins)
SET(STELLPLUGINS_MAJOR "0")
SET(STELLPLUGINS_MINOR "1")
SET(STELLPLUGINS_PATCH "1")
SET(VERSION "${STELLPLUGINS_MAJOR}.${STELLPLUGINS_MINOR}.${STELLPLUGINS_PATCH}")
SET(STELL_PATH "$ENV{HOME}/stellarium/")
SET(CMAKE_INSTALL_PREFIX $ENV{HOME}/.stellarium)

########### Main global variables ###########
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: Debug Release."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

# Please indicate below where your stellarium source and build trees are
#SET(STELLARIUM_SOURCE_DIR "$ENV{STELROOT}/src" CACHE PATH
#    "Should point to the src/ directory where the stellarium source tree is located")
SET(STELLARIUM_SOURCE_DIR "${STELL_PATH}/src")

# work out where we have built the main Stellarium code
# NOTE: plugins bild type (Release/Debug) must match the Stellarium
# build type, else they probably won't build.
SET(BUILD_SUBDIR unix)

#SET(STELLARIUM_BUILD_DIR "$ENV{STELROOT}/builds/${BUILD_SUBDIR}" CACHE PATH
#    "Should point to the directory where stellarium is build in Release mode.")
SET(STELLARIUM_BUILD_DIR "${STELL_PATH}/builds/${BUILD_SUBDIR}")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=c99")  # additional C compile flags
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++0x")  # additional CPP compile flags

########### Find packages ###########
SET(QT_MIN_VERSION "4.6.0")
FIND_PACKAGE(Qt4 REQUIRED)
SET(QT_USE_QTOPENGL TRUE)
SET(QT_USE_QTNETWORK TRUE)
INCLUDE(${QT_USE_FILE})

FIND_PACKAGE(OpenGL REQUIRED)

########### Add macros ###########
ADD_DEFINITIONS(-DQT_PLUGIN)
ADD_DEFINITIONS(-D_REENTRANT)
ADD_DEFINITIONS(-DPLUGIN_VERSION="${VERSION}")
ADD_DEFINITIONS(-D__STDC_FORMAT_MACROS)

IF(CMAKE_BUILD_TYPE STREQUAL "Release")
 ADD_DEFINITIONS(-DQT_NO_DEBUG)
 ADD_DEFINITIONS(-DNDEBUG)
ENDIF(CMAKE_BUILD_TYPE STREQUAL "Release")

########### Subdirectories ###############
ADD_SUBDIRECTORY(sat_traj_src)
ADD_SUBDIRECTORY(tle_traj_src)

INSTALL(FILES user_locations.txt DESTINATION "data/")

FILE(GLOB landscape "${CMAKE_CURRENT_SOURCE_DIR}/rt75c/*.png"
                    "${CMAKE_CURRENT_SOURCE_DIR}/rt75c/*.ini")
INSTALL(FILES ${landscape} DESTINATION "landscapes/rt75c")

## configure uninstall
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
## uninstall
ADD_CUSTOM_TARGET(stellplugins_uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

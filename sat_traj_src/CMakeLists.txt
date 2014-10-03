SET(PACKAGE SatTrajMgr)

INCLUDE_DIRECTORIES(
  ${STELLARIUM_BUILD_DIR}
  ${STELLARIUM_SOURCE_DIR}
  ${STELLARIUM_SOURCE_DIR}/core
  ${STELLARIUM_SOURCE_DIR}/core/external
  ${STELLARIUM_SOURCE_DIR}/gui
  ${STELLARIUM_SOURCE_DIR}/modules
  ${STELLARIUM_SOURCE_DIR}/stelutils
  ${STELLARIUM_SOURCE_DIR}/external
  ${STELLARIUM_SOURCE_DIR}/scripting
  ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/sat_traj_src
  ${CMAKE_SOURCE_DIR}/sat_traj_src
  ${CMAKE_SOURCE_DIR}/sat_traj_src/gui
  )

SET(SatTraj_SRCS
  AntTraj.cpp
  GenObject.cpp
  GenTraj.cpp
  GoodSample.cpp
  MeasTraj.cpp
  SatTrajMgr.cpp
  SimpleTraj.cpp
  xNtpTime.cpp
  gui/SatTrajDialog.cpp
  )

# Add here all the .ui files
SET(SatTraj_UIS
  gui/satTrajDialog.ui
  )
QT4_WRAP_UI(SatTraj_UIS_H ${SatTraj_UIS})

# Compile the resource files
SET(SatTraj_RES
  ${CMAKE_SOURCE_DIR}/SatTrajMgr.qrc)
QT4_ADD_RESOURCES(SatTraj_RES_CXX ${SatTraj_RES})

# Add here all the files which require moc compilation
# that is, all the headers with SIGNAL/SLOTS/PLUGIN_INTERFACE etc..
SET(SatTraj_MOC_HDRS
  SatTrajMgr.hpp
  gui/SatTrajDialog.hpp
  )

# After this call, SatTraj_MOC_SRCS = moc_SatTraj.cxx
QT4_WRAP_CPP(SatTraj_MOC_SRCS ${SatTraj_MOC_HDRS})

SET(extLinkerOption ${QT_LIBRARIES} ${JPEG_LIBRARIES} ${PNG_LIBRARIES} ${OPENGL_LIBRARIES} ${ICONV_LIBRARIES} vu_tools mysqlclient ntptime coord_conv pthread shmsbuf)


ADD_LIBRARY(SatTrajMgr MODULE ${SatTraj_SRCS} ${SatTraj_MOC_SRCS} ${SatTraj_RES_CXX} ${SatTraj_UIS_H})
TARGET_LINK_LIBRARIES(SatTrajMgr ${extLinkerOption})

INSTALL(TARGETS SatTrajMgr DESTINATION "modules/${PACKAGE}")

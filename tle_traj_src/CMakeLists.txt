SET(PACKAGE TleTrajMgr)

INCLUDE_DIRECTORIES(
  ${STELLARIUM_SOURCE_DIR}
  ${STELLARIUM_SOURCE_DIR}/core
  ${STELLARIUM_SOURCE_DIR}/core/external
  ${STELLARIUM_SOURCE_DIR}/core/modules
  ${STELLARIUM_SOURCE_DIR}/gui
  ${STELLARIUM_SOURCE_DIR}/scripting
  ${CMAKE_SOURCE_DIR}/tle_traj_src
  ${CMAKE_SOURCE_DIR}/tle_traj_src/gui
  ${CMAKE_SOURCE_DIR}/tle_traj_src/ccw
  ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/tle_traj_src
  ${CMAKE_BINARY_DIR}/tle_traj_src/gui
  ${CMAKE_BINARY_DIR}/tle_traj_src/ccw
  )

SET(TleTraj_SRCS
  TleTraj.hpp
  TleTraj.cpp
  TleTrajMgr.hpp
  TleTrajMgr.cpp
  gui/TleTrajDialog.hpp
  gui/TleTrajDialog.cpp
  ccw/colorchooserwidget.hpp
  ccw/colorchooserwidget.cpp
  ccw/colorviewer.hpp
  ccw/colorviewer.cpp
  ccw/qtcolortriangle.h
  ccw/qtcolortriangle.cpp
  )

# Add here all the .ui files
SET(TleTraj_UIS
  gui/tleTrajDialog.ui
  )
QT4_WRAP_UI(TleTraj_UIS_H ${TleTraj_UIS})

# Compile the resource files
SET(TleTraj_RES
  ${CMAKE_SOURCE_DIR}/TleTrajMgr.qrc)
QT4_ADD_RESOURCES(TleTraj_RES_CXX ${TleTraj_RES})

# Add here all the files which require moc compilation
# that is, all the headers with SIGNAL/SLOTS/PLUGIN_INTERFACE etc..
SET(TleTraj_MOC_HDRS
  TleTrajMgr.hpp
  gui/TleTrajDialog.hpp
  ccw/colorchooserwidget.hpp
  ccw/colorviewer.hpp
  ccw/qtcolortriangle.h
  )

# After this call, TleTraj_MOC_SRCS = moc_TleTraj.cxx
QT4_WRAP_CPP(TleTraj_MOC_SRCS ${TleTraj_MOC_HDRS})

SET(extLinkerOption ${QT_LIBRARIES} ${PNG_LIBRARIES} ${OPENGL_LIBRARIES} sat_predict)

ADD_LIBRARY(TleTrajMgr MODULE ${TleTraj_SRCS} ${TleTraj_MOC_SRCS} ${TleTraj_RES_CXX} ${TleTraj_UIS_H})
TARGET_LINK_LIBRARIES(TleTrajMgr ${extLinkerOption})

INSTALL(TARGETS TleTrajMgr DESTINATION "modules/${PACKAGE}")

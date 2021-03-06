CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

IF(POLICY CMP0020)
  CMAKE_POLICY(SET CMP0020 NEW)
ENDIF()

PROJECT(PVrui)

set(CMAKE_MODULE_PATH
  "${MooseViewer_SOURCE_DIR}/CMake/Modules"
  ${CMAKE_MODULE_PATH}
)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Use c++11:
set(CMAKE_CXX_STANDARD 11)

INCLUDE(FindPkgConfig)

IF(NOT VRUI_PKGCONFIG_DIR)
  FIND_PATH(VRUI_PKGCONFIG_DIR Vrui.pc)
ENDIF()
SET(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${VRUI_PKGCONFIG_DIR}")

PKG_SEARCH_MODULE(VRUI REQUIRED Vrui>=3000000)
STRING(REPLACE "-framework;" "-framework " VRUI_LDFLAGS "${VRUI_LDFLAGS}")
INCLUDE_DIRECTORIES(${VRUI_INCLUDE_DIRS})
ADD_DEFINITIONS(${VRUI_CFLAGS_OTHER})


# Find ParaView
find_package(ParaView REQUIRED)
include(${PARAVIEW_USE_FILE})

# Fint Qt4
find_package(Qt4 REQUIRED QtGui)

# Find VTK
FIND_PACKAGE(VTK REQUIRED)
INCLUDE(${VTK_USE_FILE})
include_directories(${VTK_INCLUDE_DIRS})

IF (VTK_VERSION VERSION_LESS "6.2")
  MESSAGE(FATAL_ERROR "Require VTK version 6.2 or higher")
ENDIF ()

IF (${VTK_RENDERING_BACKEND} STREQUAL "OpenGL")
  FIND_PACKAGE (GLEW REQUIRED)
  IF (NOT GLEW_FOUND)
    MESSAGE (FATAL_ERROR "Glew required. Please set GLEW_DIR")
  ENDIF ()
  INCLUDE_DIRECTORIES (${GLEW_INCLUDE_DIR})
ENDIF ()

# Find vtkVRUI
find_package(vtkVRUI REQUIRED)
include_directories(${vtkVRUI_INCLUDE_DIRS})

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
INCLUDE(InstallRequiredSystemLibraries)

SET(${PROJECT_NAME}_SRCS
  AnimationDialog.cpp
  AnimationDialog.h
  ColorMapCallbackData.cpp
  ColorMapCallbackData.h
  ColorMapChangedCallbackData.cpp
  ColorMapChangedCallbackData.h
  ColorMap.cpp
  ColorMap.h
  Contours.cpp
  Contours.h
  ControlPointChangedCallbackData.cpp
  ControlPointChangedCallbackData.h
  ControlPoint.cpp
  ControlPoint.h
  Connection.h
  Connection.cpp
  Gaussian.cpp
  Gaussian.h
  main.cpp
  MooseViewer.cpp
  MooseViewer.h
  mvApplicationState.cpp
  mvApplicationState.h
  mvContours.cpp
  mvContours.h
  mvGeometry.cpp
  mvGeometry.h
  mvInteractor.cpp
  mvInteractor.h
  mvInteractorTool.cpp
  mvInteractorTool.h
  mvMouseRotationTool.cpp
  mvMouseRotationTool.h
  mvOutline.cpp
  mvOutline.h
  mvReader.cpp
  mvReader.h
  mvSlice.cpp
  mvSlice.h
  mvVolume.cpp
  mvVolume.h
  RGBAColor.cpp
  RGBAColor.h
  ScalarWidgetCallbackData.cpp
  ScalarWidgetCallbackData.h
  ScalarWidgetChangedCallbackData.cpp
  ScalarWidgetChangedCallbackData.h
  ScalarWidgetControlPointChangedCallbackData.cpp
  ScalarWidgetControlPointChangedCallbackData.h
  ScalarWidgetControlPoint.cpp
  ScalarWidgetControlPoint.h
  ScalarWidget.cpp
  ScalarWidget.h
  ScalarWidgetStorage.cpp
  ScalarWidgetStorage.h
  Storage.cpp
  Storage.h
  SwatchesWidget.cpp
  SwatchesWidget.h
  TransferFunction1D.cpp
  TransferFunction1D.h
  VariablesDialog.cpp
  VariablesDialog.h
  WidgetHints.cpp
  WidgetHints.h
  servermanager.h
  servermanager.cpp
  ParaView.h
  ParaView.cpp
  )

ADD_EXECUTABLE(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS})

TARGET_LINK_LIBRARIES(${PROJECT_NAME}
  ${vtkVRUI_LIBRARIES}
  ${PARAVIEW_LIBRARIES}
  ${VTK_LIBRARIES}
  "${VRUI_LDFLAGS}"
)

IF (${VTK_RENDERING_BACKEND} STREQUAL "OpenGL")
  TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${GLEW_LIBRARY})
ENDIF ()

INSTALL(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  )

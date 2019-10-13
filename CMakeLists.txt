cmake_minimum_required(VERSION 2.0)
project(Detector)

set(CMAKE_CXX_STANDARD 11)
find_package( OpenCV REQUIRED )

include_directories(./)
include_directories(./inc)
include_directories(${OpenCV_INCLUDES})

link_directories(../linux_so)
link_directories(${OpenCV_LIBS})

add_executable(Detector
        ./inc/amcomdef.h
        ./inc/arcsoft_face_sdk.h
		./inc/asvloffscreen.h
		./inc/merror.h
		./inc/shared_queue.hpp
		./detector.cc)
set_property(TARGET Detector PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(Detector
		arcsoft_face
		arcsoft_face_engine
		pistache
		${OpenCV_LIBS}
        )

cmake_minimum_required(VERSION 2.8)
project( openCV-captureImage )
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )
add_executable( openCV-captureImage  openCV_captureImage_MIPI.cpp )
target_link_libraries( openCV-captureImage ${OpenCV_LIBS} )


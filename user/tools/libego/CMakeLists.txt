project( libego )

cmake_minimum_required( VERSION 2.8.3 )

# Don't not allow in-src builds
if( "${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}" )
           message( FATAL_ERROR "** In-source builds are not allowed.")
   endif()

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake )

add_subdirectory( src )

add_subdirectory( tests )

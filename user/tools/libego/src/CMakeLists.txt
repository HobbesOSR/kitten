
include( ExternalProject )

# Remove the UPDATE_COMMAND empty string below to check if
# libworkloads was updated while building
#
ExternalProject_Add( libworkloads
        GIT_REPOSITORY "https://github.com/kbferreira/libworkloads.git"
        GIT_TAG "master"
        UPDATE_COMMAND ""
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/contrib
)

ExternalProject_Get_Property( libworkloads install_dir )

message( "** Installed directory: ${install_dir} " )

add_library( workloads STATIC IMPORTED )
set_property( TARGET workloads 
              PROPERTY IMPORTED_LOCATION ${install_dir}/lib/libworkloads.a )
add_dependencies( workloads libworkloads )

include_directories( ${install_dir}/include)

option( DEBUG_PRINT "DEBUG" OFF )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} )

# MPI Stuff
find_package( MPI REQUIRED )
include_directories( SYSTEM ${MPI_INCLUDE_PATH} )
link_directories( ${MPI_C_LINK_FLAGS} )
set( CMAKE_COMPILE_FLAGS ${CMAKE_COMPILE_FLAGS} ${MPI_C_COMPILE_FLAGS})

find_file( WRAP wrap.py 
        PATHS ${HOME} ${CMAKE_SOURCE_DIR} /usr/local/bin ENV WRAP
)

if( WRAP STREQUAL "WRAP-NOTFOUND" )
        MESSAGE( FATAL_ERROR "** Cannot find wrap.py in distribution and environment" )
else()
        MESSAGE( STATUS "Found wrap.py ... ${WRAP}" )
endif()

include( WrapConfig )

add_wrapped_file( wrapper.c wrap/wrapper.w -f )

add_library( ego STATIC wrapper.c ego.c env.c )

target_link_libraries( ego workloads )
target_link_libraries( ego ${MPI_C_LIBRARIES})

# This is all all here to do the Non-Blocking Collective test -kbf
set( CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}"
        "${MPI_C_LIBRARIES}" )
set( CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}" "${MPI_C_COMPILE_FLAGS}" )
set( CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}"
        "${MPI_C_INCLUDE_PATH}" )

include( CheckCSourceCompiles )

unset( HAVE_NB_ALLREDUCE CACHE )

check_c_source_compiles(
"
#include <mpi.h>
#include <stdlib.h>

int main( void ){
        MPI_Iallreduce( NULL, NULL, 0, MPI_INT, MPI_SUM, MPI_COMM_WORLD,
                MPI_REQUEST_NULL );
        return 0;
}
" HAVE_NB_ALLREDUCE )

if( HAVE_NB_ALLREDUCE )
        message( STATUS "Have MPI_Iallreduce(), we are good" )
else()
        message( FATAL_ERROR "Needed MPI_Iallreduce() [MPI 3.X] not found" )
endif()


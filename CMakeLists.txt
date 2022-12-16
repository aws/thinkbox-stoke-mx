# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

project( StokeMX )

find_package( thinkboxcmlibrary REQUIRED )
include( PrecompiledHeader )
include( ThinkboxCMLibrary )

set( SUPPORTED_MAX_VERSIONS 2022 2023 )

option( MAX_VERSION "The version of the 3ds Max SDK to build the library against." 2022 )

if( NOT MAX_VERSION IN_LIST SUPPORTED_MAX_VERSIONS )
  message( FATAL_ERROR "ERROR: Cannot build for unsupported 3ds Max version ${MAX_VERSION}" )
endif()

add_library( stokemx SHARED )

set_target_properties( stokemx PROPERTIES OUTPUT_NAME "StokeMX" )
set_target_properties( stokemx PROPERTIES PREFIX "" )
set_target_properties( stokemx PROPERTIES SUFFIX ".dlo" )

target_include_directories( stokemx PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/stoke>
                            $<INSTALL_INTERFACE:include> )

target_include_directories( stokemx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )

file( GLOB_RECURSE H_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        "stoke/*.h"
        "stoke/*.hpp"
)

file( GLOB_RECURSE CXX_FILES ${CMAKE_CURRENT_SOURCE}
    "src/*.cpp"
)

target_sources( stokemx PRIVATE
    "resource.h"
    "resource_ember.h"
    "resource_genome.h"
    "resource_pflow.h"
    "EmberMX.rc"
    "EmberMX_PFlowOp.rc"
    "Genome.rc"
    "StokeMX.rc"
    "StokeMXVersion.rc"
    "StokeMXVersion.h"
    "stdafx.h"
    "stdafx.cpp"
    ${H_FILES}
    ${CXX_FILES}
)

add_precompiled_header( stokemx stdafx.h SOURCE_CXX stdafx.cpp )


# The Conan version of Boost was built with this, and it changes the library names.
# As a result, we need to set this to tell Boost to look for the right libraries to
# link against.
target_compile_definitions( stokemx PUBLIC BOOST_AUTO_LINK_SYSTEM )

find_package( thinkboxlibrary REQUIRED )
find_package( thinkboxmxlibrary REQUIRED )
find_package( krakatoa REQUIRED )
find_package( krakatoamxsdk REQUIRED )
find_package( magma REQUIRED )
find_package( magmamx REQUIRED )
find_package( stoke REQUIRED )
# find_package( nodeview REQUIRED )
find_package( maxsdk REQUIRED )
find_package( libb2 REQUIRED )
find_package( Boost REQUIRED )
find_package( OpenEXR REQUIRED )
find_package( ZLIB REQUIRED )
find_package( TBB REQUIRED )
find_package( tinyxml2 REQUIRED )
find_package( xxHash REQUIRED )
find_package( OpenVDB REQUIRED )
find_package( HDF5 REQUIRED )
find_package( MKL REQUIRED )

target_include_directories( stokemx PUBLIC ${thinkboxlibrary_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${maxsdk_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${thinkboxmxlibrary_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${krakatoa_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${krakatoamxsdk_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${magma_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${magmamx_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${stoke_INCLUDE_DIRS} )
# target_include_directories( stokemx PUBLIC ${nodeview_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${libb2_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${Boost_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${OpenEXR_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${ZLIB_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${TBB_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${tinyxml2_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${xxHash_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${OpenVDB_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${HDF5_INCLUDE_DIRS} )
target_include_directories( stokemx PUBLIC ${MKL_INCLUDE} )

target_link_libraries( stokemx PUBLIC thinkboxlibrary::thinkboxlibrary )
target_link_libraries( stokemx PUBLIC maxsdk::maxsdk )
target_link_libraries( stokemx PUBLIC thinkboxmxlibrary::thinkboxmxlibrary )
target_link_libraries( stokemx PUBLIC krakatoa::krakatoa )
target_link_libraries( stokemx PUBLIC krakatoamxsdk::krakatoamxsdk )
target_link_libraries( stokemx PUBLIC magma::magma )
target_link_libraries( stokemx PUBLIC magmamx::magmamx )
target_link_libraries( stokemx PUBLIC stoke::stoke )
# target_link_libraries( stokemx PUBLIC nodeview::nodeview )
target_link_libraries( stokemx PUBLIC libb2::libb2 )
target_link_libraries( stokemx PUBLIC Boost::Boost )
target_link_libraries( stokemx PUBLIC OpenEXR::OpenEXR )
target_link_libraries( stokemx PUBLIC ZLIB::ZLIB )
target_link_libraries( stokemx PUBLIC TBB::tbb )
target_link_libraries( stokemx PUBLIC tinyxml2::tinyxml2 )
target_link_libraries( stokemx PUBLIC xxHash::xxHash )
target_link_libraries( stokemx PUBLIC OpenVDB::OpenVDB )
target_link_libraries( stokemx PUBLIC HDF5::HDF5 )
target_link_libraries( stokemx PUBLIC MKL::MKL )

target_compile_definitions( stokemx PUBLIC OPENVDB_STATICLIB )

frantic_default_source_groups( stokemx HEADERDIR stoke SOURCEDIR src )
frantic_common_platform_setup( stokemx )

# Disable optimization for the RelWithDebInfo configuration on Windows.
# This allows breakpoints to be hit reliably when debugging in Visual Studio.
if( WIN32 )
	target_compile_options( stokemx PRIVATE "$<$<CONFIG:Release>:/O2>$<$<CONFIG:RelWithDebInfo>:/Od>" )
endif()

install( TARGETS stokemx
		 RUNTIME DESTINATION bin
		 LIBRARY DESTINATION lib
		 ARCHIVE DESTINATION lib
)

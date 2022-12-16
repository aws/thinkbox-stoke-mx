// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/magma/nodes/magma_input_geometry_interface.hpp>
#include <frantic/magma/nodes/magma_node_property.hpp>
#include <frantic/magma/nodes/magma_simple_operator.hpp>

namespace frantic {
namespace magma {
namespace nodes {
namespace max3d {

/**
 * This magma node is used exclusively in Genome to expose information about the modifier.
 */
class magma_genome_extras_node : public frantic::magma::nodes::magma_input_node {
  public:
    MAGMA_REQUIRED_METHODS( magma_genome_extras_node );

    virtual int get_num_outputs() const;

    virtual void compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler );
};

// class magma_adjacent_faces_node : public frantic::magma::nodes::magma_simple_operator<4>{
// public:
//	MAGMA_REQUIRED_METHODS( magma_adjacent_faces_node );
//
//	virtual void compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler );
// };
//
// class magma_adjacent_verts_node : public frantic::magma::nodes::magma_simple_operator<4>{
// public:
//	MAGMA_REQUIRED_METHODS( magma_adjacent_verts_node );
//
//	virtual void compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler );
// };
//
// class magma_adjacent_vert_faces_node : public frantic::magma::nodes::magma_simple_operator<4>{
// public:
//	MAGMA_REQUIRED_METHODS( magma_adjacent_vert_faces_node );
//
//	virtual void compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler );
// };
//
// class magma_input_derivative_node : public frantic::magma::nodes::magma_input_node{
// public:
//	MAGMA_REQUIRED_METHODS( magma_input_derivative_node );
//
//	MAGMA_PROPERTY( dataChannelName, frantic::tstring )   //A channel defined at the corners of a face
//	MAGMA_PROPERTY( uvChannelName, frantic::tstring )     //A surface parameterization of the face, relative to the
//entire mesh. UV coordinates, ideally mapping a specifc UV to a unique point.
//	//MAGMA_PROPERTY( uvPermutation, std::string )  //Since 3ds Max uses UVWs we can use UV, UW, or VW as the
//parameters.
//
//	virtual int get_num_outputs() const;
//
//	virtual void compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler );
// };

} // namespace max3d
} // namespace nodes
} // namespace magma
} // namespace frantic

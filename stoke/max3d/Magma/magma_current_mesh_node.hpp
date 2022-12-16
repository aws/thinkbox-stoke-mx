// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/magma/magma_compiler_interface.hpp>
#include <frantic/magma/nodes/magma_node_property.hpp>
#include <frantic/magma/nodes/magma_simple_operator.hpp>

class Mesh;

namespace frantic {
namespace magma {
namespace nodes {
namespace max3d {

/**
 * This magma node is used exclusively in Genome to expose the entire 3ds Max Mesh object that is currently being
 * modified in a call to ModifyObject. A copy of the original mesh is made before modification, so it will not be
 * affected by previous iterations during the magma invocation. The mesh is exposed as via
 * magma_input_geometry_interface, just like the InputGeometry node.
 */
class magma_current_mesh_node : public frantic::magma::nodes::magma_input_node {
  public:
    static void create_type_definition( frantic::magma::magma_node_type& outType );

    magma_current_mesh_node() {}

    virtual void compile( frantic::magma::magma_compiler_interface& compiler );

    virtual void compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler );
};

} // namespace max3d
} // namespace nodes
} // namespace magma
} // namespace frantic

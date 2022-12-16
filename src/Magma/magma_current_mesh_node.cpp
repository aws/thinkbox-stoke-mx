// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/Magma/magma_current_mesh_node.hpp>

#include <frantic/magma/simple_compiler/simple_mesh_compiler.hpp>

#include <frantic/magma/magma_node_base.hpp>
#include <frantic/magma/nodes/magma_node_impl.hpp>

#include <frantic/max3d/geometry/max_mesh_interface.hpp>

#include <max.h>

namespace frantic {
namespace magma {
namespace nodes {
namespace max3d {

MAGMA_DEFINE_TYPE( "CurrentMesh", "Input", magma_current_mesh_node )
MAGMA_TYPE_ATTR( disableable, false )
MAGMA_DESCRIPTION( "Exposes a copy of the mesh currently being iterated over by Genome" )
MAGMA_OUTPUT_NAMES( "CurrentMesh" )
MAGMA_DEFINE_TYPE_END

void magma_current_mesh_node::compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler ) {
    if( frantic::magma::simple_compiler::simple_mesh_compiler* sc =
            dynamic_cast<frantic::magma::simple_compiler::simple_mesh_compiler*>( &compiler ) ) {
        sc->compile_current_mesh( this->get_id() );
    } else {
        magma_node_base::compile_as_extension_type( compiler );
    }
}

} // namespace max3d
} // namespace nodes
} // namespace magma
} // namespace frantic

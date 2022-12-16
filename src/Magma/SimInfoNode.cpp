// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/magma/SimInfoNode.hpp>

#include <ember/ember_compiler.hpp>

#include <frantic/magma/nodes/magma_node_impl.hpp>

#include <frantic/max3d/time.hpp>

namespace ember {
namespace max3d {

MAGMA_DEFINE_TYPE( "SimInfo", "Stoke", SimInfoNode )
MAGMA_OUTPUT_NAMES( "CurrentTime", "TimeStep", "Min", "Max", "Spacing" );
MAGMA_DESCRIPTION(
    "Exposes simulation information. CurrentTime & TimeStep in seconds, Min, Max & Spacing in world units." )
MAGMA_DEFINE_TYPE_END;

int SimInfoNode::get_num_outputs() const { return 4; }

void SimInfoNode::compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler ) {
    float time = 0.f;
    float timeStep = std::numeric_limits<float>::quiet_NaN();
    float spacing = std::numeric_limits<float>::quiet_NaN();
    frantic::graphics::boundbox3f bounds;

    TimeValue maxTime = 0;

    compiler.get_context_data().get_property( _T("Time"), maxTime );
    compiler.get_context_data().get_property( _T("TimeStep"), timeStep );
    compiler.get_context_data().get_property( _T("Spacing"), spacing );
    compiler.get_context_data().get_property( _T("Bounds"), bounds );

    compiler.compile_constant( this->get_id(), static_cast<float>( frantic::max3d::to_seconds<double>( maxTime ) ) );
    compiler.compile_constant( this->get_id(), timeStep );
    compiler.compile_constant( this->get_id(), bounds.minimum() );
    compiler.compile_constant( this->get_id(), bounds.maximum() );
}

} // namespace max3d
} // namespace ember

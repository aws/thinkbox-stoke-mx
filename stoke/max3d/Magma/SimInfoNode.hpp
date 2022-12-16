// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/magma/nodes/magma_node_property.hpp>
#include <frantic/magma/nodes/magma_simple_operator.hpp>

namespace ember {
namespace max3d {

class SimInfoNode : public frantic::magma::nodes::magma_input_node {
  public:
    MAGMA_REQUIRED_METHODS( DivergenceFreeCacheNode );

    SimInfoNode() {}

    virtual ~SimInfoNode() {}

    virtual int get_num_outputs() const;

    // virtual void get_output_description( int i, frantic::tstring& outDescription ) const;

    virtual void compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler );
};

} // namespace max3d
} // namespace ember

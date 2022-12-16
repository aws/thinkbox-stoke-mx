// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/max3d/convert.hpp>

#include <frantic/magma/magma_compiler_interface.hpp>

#include <frantic/graphics/boundbox3f.hpp>

namespace ember {
namespace max3d {

class EmberMagmaContext : public frantic::magma::magma_compiler_interface::context_base {
  public:
    const RenderGlobalContext* globContext;
    INode* node;
    frantic::graphics::boundbox3f bounds;
    float spacing;

  public:
    EmberMagmaContext( const RenderGlobalContext& _globContext, INode* _node = NULL )
        : node( _node )
        , globContext( &_globContext ) {}

    virtual ~EmberMagmaContext() {}

    virtual frantic::tstring get_name() const { return _T("EmberContext"); }

    /**
     * Returns a matrix that transforms from the current object's space into world space.
     * @param inverse If true, the matrix is inverted, transforming from worldspace into object space.
     */
    virtual frantic::graphics::transform4f get_world_transform( bool inverse = false ) const {
        return node ? frantic::max3d::from_max_t( inverse ? Inverse( node->GetNodeTM( globContext->time ) )
                                                          : node->GetNodeTM( globContext->time ) )
                    : frantic::graphics::transform4f::identity();
    }

    /**
     * Returns a matrix that transforms from world space into the current camera's space.
     * @param inverse If true, the matrix is inverted, transforming from cameraspace into world space.
     */
    virtual frantic::graphics::transform4f get_camera_transform( bool inverse = false ) const {
        return frantic::max3d::from_max_t( inverse ? globContext->camToWorld : globContext->camToWorld );
    }

    /**
     * Returns a property that nodes might need, in a type agnostic container.
     */
    virtual boost::any get_property( const frantic::tstring& name ) const {
        if( name == _T("Time") )
            return boost::any( globContext->time );
        else if( name == _T("MaxRenderGlobalContext") )
            return boost::any( globContext );
        else if( name == _T("CurrentINode") )
            return boost::any( node );
        else if( name == _T("InWorldSpace") )
            return boost::any( node == NULL );
        else if( name == _T("Bounds") )
            return boost::any( bounds );
        else if( name == _T("Spacing") )
            return boost::any( spacing );
        return frantic::magma::magma_compiler_interface::context_base::get_property( name );
    }
};

} // namespace max3d
} // namespace ember

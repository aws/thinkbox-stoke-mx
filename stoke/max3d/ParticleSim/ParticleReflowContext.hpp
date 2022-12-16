// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdlib>

#include <boost/any.hpp>

#include <maxversion.h>
#if MAX_VERSION_MAJOR >= 19
#include <Rendering/RenderGlobalContext.h>
#endif

#include <frantic/magma/magma_compiler_interface.hpp>

#include <frantic/max3d/convert.hpp>
#include <frantic/max3d/rendering/renderplugin_utils.hpp>

namespace stoke {
namespace max3d {

class ParticleReflowContext : public frantic::magma::magma_compiler_interface::context_base {
  public:
    RenderGlobalContext globContext;
    INode* node;

  public:
    ParticleReflowContext( TimeValue t, INode* _node = NULL )
        : node( _node ) {
        frantic::max3d::rendering::initialize_renderglobalcontext( globContext );
        globContext.time = t;
        globContext.camToWorld.IdentityMatrix();
        globContext.worldToCam.IdentityMatrix();
    }

    virtual ~ParticleReflowContext() {}

    virtual frantic::tstring get_name() const { return _T( "ParticleReflowContext" ); }

    virtual frantic::graphics::transform4f get_world_transform( bool inverse = false ) const {
        return node ? frantic::max3d::from_max_t( inverse ? Inverse( node->GetNodeTM( globContext.time ) )
                                                          : node->GetNodeTM( globContext.time ) )
                    : frantic::graphics::transform4f::identity();
    }

    virtual frantic::graphics::transform4f get_camera_transform( bool inverse = false ) const {
        return frantic::max3d::from_max_t( inverse ? globContext.camToWorld : globContext.camToWorld );
    }

    virtual boost::any get_property( const frantic::tstring& name ) const {
        if( name == _T( "Time" ) )
            return boost::any( globContext.time );
        else if( name == _T( "MaxRenderGlobalContext" ) )
            return boost::any( static_cast<const RenderGlobalContext*>( &globContext ) );
        else if( name == _T( "CurrentINode" ) )
            return boost::any( node );
        else if( name == _T( "InWorldSpace" ) )
            return boost::any( node == NULL );
        return frantic::magma::magma_compiler_interface::context_base::get_property( name );
    }
};

} // namespace max3d
} // namespace stoke
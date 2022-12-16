// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include <frantic/graphics/camera.hpp>
#include <frantic/magma/magma_compiler_interface.hpp>
#include <frantic/max3d/convert.hpp>

#include <frantic/max3d/node_transform.hpp>

#include <max.h>

class GenomeContext : public frantic::magma::magma_compiler_interface::context_base {
    frantic::graphics::camera<float> m_camera;
    INode* m_node;

    RenderGlobalContext m_maxContext;

    const Mesh* m_currentMesh;
    const ModContext* m_modContext;

  public:
    GenomeContext()
        : m_node( NULL )
        , m_currentMesh( NULL )
        , m_modContext( NULL ) {
        m_maxContext.time = TIME_NegInfinity;
        m_maxContext.antialias = FALSE;
        m_maxContext.atmos = NULL;
        m_maxContext.devAspect = 1.f;
        m_maxContext.devHeight = 480;
        m_maxContext.devWidth = 640;
        m_maxContext.envMap = NULL;
        m_maxContext.farRange = FLT_MAX;
        m_maxContext.fieldRender = FALSE;
        m_maxContext.field_order = 0;
        m_maxContext.first_field = 0;
        m_maxContext.frameDur = 1.f;
        m_maxContext.globalLightLevel.White();
        m_maxContext.inMtlEdit = FALSE;
        m_maxContext.nBlurFrames = 0;
        m_maxContext.nearRange = 1e-4f;
        m_maxContext.objMotBlur = FALSE;
        m_maxContext.projType = PROJ_PERSPECTIVE;
        m_maxContext.pToneOp = NULL;
        m_maxContext.renderer = NULL;
        m_maxContext.simplifyAreaLights = TRUE;
        m_maxContext.wireMode = FALSE;
        m_maxContext.wire_thick = 1.f;
        m_maxContext.xc = 320.f;
        m_maxContext.yc = 280.f;
        m_maxContext.xscale = -m_maxContext.xc;
        m_maxContext.yscale = m_maxContext.yc * m_maxContext.devAspect;

        m_maxContext.camToWorld.IdentityMatrix();
        m_maxContext.worldToCam.IdentityMatrix();
    }

    virtual ~GenomeContext() {}

    inline void set_time( TimeValue t ) { m_maxContext.time = t; }

    inline void set_camera( const frantic::graphics::camera<float>& cam ) {
        m_camera = cam;

        m_maxContext.camToWorld = frantic::max3d::to_max_t( cam.world_transform() );
        m_maxContext.worldToCam = frantic::max3d::to_max_t( cam.world_transform_inverse() );
        m_maxContext.nearRange = cam.near_distance();
        m_maxContext.farRange = cam.far_distance();
        m_maxContext.devAspect = cam.pixel_aspect();
        m_maxContext.devWidth = cam.get_output_size().xsize;
        m_maxContext.devHeight = cam.get_output_size().ysize;
        m_maxContext.projType = ( cam.projection_mode() == frantic::graphics::projection_mode::orthographic )
                                    ? PROJ_PARALLEL
                                    : PROJ_PERSPECTIVE;

        m_maxContext.xc = (float)m_maxContext.devWidth * 0.5f;
        m_maxContext.yc = (float)m_maxContext.devHeight * 0.5f;

        if( m_maxContext.projType == PROJ_PERSPECTIVE ) {
            float v = m_maxContext.xc / std::tan( 0.5f * cam.horizontal_fov() );
            m_maxContext.xscale = -v;
            m_maxContext.yscale = v * m_maxContext.devAspect;
        } else {
            const float VIEW_DEFAULT_WIDTH = 400.f;
            m_maxContext.xscale = (float)m_maxContext.devWidth / ( VIEW_DEFAULT_WIDTH * cam.orthographic_width() );
            m_maxContext.yscale = -m_maxContext.devAspect * m_maxContext.xscale;
        }
    }

    /*inline void set_node_transform( const frantic::graphics::transform4f& tm ){
            m_transform = tm;
    }*/
    inline void set_node( INode* node ) { m_node = node; }

    inline void set_mesh( const Mesh& theMesh ) { m_currentMesh = &theMesh; }

    inline void set_modcontext( const ModContext& mc ) { m_modContext = &mc; }

    virtual frantic::tstring get_name() const { return _M( "GenomeContext" ); }

    virtual frantic::graphics::transform4f get_world_transform( bool inverse = false ) const {
        // return inverse ? m_transform.to_inverse() : m_transform;
        if( m_node ) {
            Interval valid = FOREVER;
            frantic::graphics::transform4f result =
                frantic::max3d::from_max_t( frantic::max3d::get_node_transform( m_node, m_maxContext.time, valid ) );
            if( inverse )
                result = result.to_inverse();
            return result;
        }

        return frantic::graphics::transform4f();
    }

    virtual frantic::graphics::transform4f get_camera_transform( bool inverse = false ) const {
        return inverse ? m_camera.world_transform() : m_camera.world_transform_inverse();
    }

    virtual boost::any get_property( const frantic::tstring& name ) const {
        if( name == _M( "Time" ) ) {
            return boost::any( m_maxContext.time );
        } else if( name == _M( "MaxRenderGlobalContext" ) ) {
            return boost::any( &m_maxContext );
        } else if( name == _M( "CurrentINode" ) ) {
            return boost::any( m_node );
        } else if( name == _M( "InWorldSpace" ) ) {
            return boost::any( false );
        } else if( name == _M( "Camera" ) ) {
            return boost::any( m_camera );
        } else if( name == _M( "CurrentMesh" ) ) {
            return boost::any( m_currentMesh );
        } else if( m_modContext != NULL ) {
            if( m_modContext->box != NULL ) {
                if( name == _M( "BoundsMin" ) )
                    return boost::any( frantic::max3d::from_max_t( m_modContext->box->pmin ) );
                else if( name == _M( "BoundsMax" ) )
                    return boost::any( frantic::max3d::from_max_t( m_modContext->box->pmax ) );
                else if( name == _M( "Center" ) )
                    return boost::any( frantic::max3d::from_max_t( m_modContext->box->Center() ) );
            }
        }

        return frantic::magma::magma_compiler_interface::context_base::get_property( name );
    }
};

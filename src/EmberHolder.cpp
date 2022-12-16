// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/magma/max3d/nodes/magma_input_field_node.hpp>
#include <stoke/max3d/EmberHolder.hpp>
#include <stoke/max3d/IEmberHolder.hpp>

// Dunno how these are sneaking in here...
#undef min
#undef max

#include <ember/ember_compiler.hpp>
#include <ember/nodes/AdvectionNode.hpp>
#include <ember/nodes/CurlNode.hpp>
#include <ember/nodes/DivergenceNode.hpp>
#include <ember/nodes/GradientNode.hpp>
#include <ember/nodes/GridCacheNode.hpp>
#include <ember/nodes/LevelSet.hpp>
#include <ember/nodes/ParticleSplatNode.hpp>
#include <ember/nodes/PressureSolveNode.hpp>

#include <frantic/magma/max3d/MagmaHolder.hpp>
#include <frantic/magma/max3d/nodes/magma_curve_op_node.hpp>
#include <frantic/magma/max3d/nodes/magma_geometry_input_node.hpp>
#include <frantic/magma/max3d/nodes/magma_input_object_node.hpp>
#include <frantic/magma/max3d/nodes/magma_input_particles_node.hpp>
#include <frantic/magma/max3d/nodes/magma_input_value_node.hpp>
#include <frantic/magma/max3d/nodes/magma_script_op_node.hpp>
#include <frantic/magma/max3d/nodes/magma_texmap_node.hpp>
#include <frantic/magma/max3d/nodes/magma_transform_node.hpp>

#include <frantic/magma/magma_singleton.hpp>

#include <algorithm>

#define EmberHolder_CLASS Class_ID( 0x2b493510, 0x5134620d )
#define EmberHolder_NAME "EmberHolder"

extern HINSTANCE hInstance;

namespace frantic {
namespace magma {
namespace nodes {
namespace max3d {
void define_max_nodes( frantic::magma::magma_singleton& ms );

Class_ID magma_from_space_node::max_impl::s_ClassID( 0x468a3de0, 0x20ca47fa );
Class_ID magma_to_space_node::max_impl::s_ClassID( 0x49b0678e, 0x14b53a6b );
Class_ID magma_input_value_node::max_impl::s_ClassID( 0xfbb7879, 0x593b65b3 );
Class_ID magma_input_particles_node::max_impl::s_ClassID( 0x47a77aef, 0x5d2a19f2 );
Class_ID magma_input_object_node::max_impl::s_ClassID( 0xc4f2b62, 0x6b753f85 );
Class_ID magma_curve_op_node::max_impl::s_ClassID( 0x4f5e5b97, 0x67027f6d );
Class_ID magma_input_geometry_node::max_impl::s_ClassID( 0x9135415, 0x146a1b4c );
Class_ID magma_script_op_node::max_impl::s_ClassID( 0x47f762e1, 0x6d474a15 );
Class_ID magma_texmap_node::max_impl::s_ClassID( 0x2a9e0821, 0x6dff6e23 );
} // namespace max3d
} // namespace nodes
} // namespace magma
} // namespace frantic

namespace frantic {
namespace magma {
namespace nodes {
namespace max3d {
Class_ID magma_field_input_node::max_impl::s_ClassID( 0x627a02bb, 0x13a2ea9 );
}
} // namespace nodes
} // namespace magma
} // namespace frantic

namespace ember {
namespace max3d {

EmberHolder::EmberHolderClassDesc::EmberHolderClassDesc()
    : frantic::magma::max3d::MagmaHolderClassDesc() {}

EmberHolder::EmberHolderClassDesc::~EmberHolderClassDesc() {}

const MCHAR* EmberHolder::EmberHolderClassDesc::ClassName() { return _T( EmberHolder_NAME ); }

HINSTANCE EmberHolder::EmberHolderClassDesc::HInstance() { return hInstance; }

Class_ID EmberHolder::EmberHolderClassDesc::ClassID() { return EmberHolder_CLASS; }

void* EmberHolder::EmberHolderClassDesc::Create( BOOL loading ) { return new EmberHolder( loading ); }

EmberHolder::EmberHolderMagmaSingleton::EmberHolderMagmaSingleton()
    : frantic::magma::magma_singleton( true ) {
    frantic::magma::nodes::max3d::define_max_nodes( *this );

    this->define_node_type<frantic::magma::nodes::max3d::magma_texmap_node>();

    this->define_node_type<ember::nodes::LevelSetNode>();
    this->define_node_type<ember::nodes::AdvectionNode>();
    this->define_node_type<ember::nodes::GradientNode>();
    this->define_node_type<ember::nodes::DivergenceNode>();
    this->define_node_type<ember::nodes::CurlNode>();
    this->define_node_type<ember::nodes::GridCacheNode>();
    this->define_node_type<ember::nodes::StaggeredGridCacheNode>();
    this->define_node_type<ember::nodes::DivergenceFreeCacheNode>();
    this->define_node_type<ember::nodes::ParticleSplatNode>();
    this->define_node_type<ember::nodes::ParticleSplatVelocityNode>();
    this->define_node_type<frantic::magma::nodes::max3d::magma_field_input_node>();
}

frantic::magma::magma_singleton& EmberHolder::EmberHolderMagmaSingleton::get_instance() {
    static EmberHolderMagmaSingleton theEmberHolderMagmaSingleton;
    return theEmberHolderMagmaSingleton;
}

EmberHolderMagmaSingleton::EmberHolderMagmaSingleton()
    : frantic::magma::magma_singleton( true ) {
    frantic::magma::nodes::max3d::define_max_nodes( *this );

    this->define_node_type<frantic::magma::nodes::max3d::magma_texmap_node>();

    this->define_node_type<ember::nodes::LevelSetNode>();
    this->define_node_type<ember::nodes::AdvectionNode>();
    this->define_node_type<ember::nodes::GradientNode>();
    this->define_node_type<ember::nodes::DivergenceNode>();
    this->define_node_type<ember::nodes::CurlNode>();
    this->define_node_type<ember::nodes::GridCacheNode>();
    this->define_node_type<ember::nodes::StaggeredGridCacheNode>();
    this->define_node_type<ember::nodes::DivergenceFreeCacheNode>();
    this->define_node_type<ember::nodes::ParticleSplatNode>();
    this->define_node_type<ember::nodes::ParticleSplatVelocityNode>();
    this->define_node_type<frantic::magma::nodes::max3d::magma_field_input_node>();
}

frantic::magma::magma_singleton& EmberHolderMagmaSingleton::get_instance() {
    static EmberHolderMagmaSingleton theEmberHolderMagmaSingleton;
    return theEmberHolderMagmaSingleton;
}

StokeHolderMagmaSingleton::StokeHolderMagmaSingleton()
    : frantic::magma::magma_singleton( true ) {
    frantic::magma::nodes::max3d::define_max_nodes( *this );

    this->define_node_type<frantic::magma::nodes::max3d::magma_field_input_node>();
    this->define_node_type<frantic::magma::nodes::max3d::magma_texmap_node>();
}

frantic::magma::magma_singleton& StokeHolderMagmaSingleton::get_instance() {
    static StokeHolderMagmaSingleton theStokeHolderMagmaSingleton;
    return theStokeHolderMagmaSingleton;
}

EmberHolder::EmberHolder( boost::int64_t nodeSetID )
    : MagmaHolder( EmberHolder::create_magma_interface( nodeSetID ) )
    , m_nodeSetID( nodeSetID ) {
    GetClassDescInstance().MakeAutoParamBlocks( this );
}

EmberHolder::EmberHolder( BOOL /*loading*/ )
    : MagmaHolder( EmberHolderMagmaSingleton::get_instance().create_magma_instance() )
    , m_nodeSetID( EMBER_BASIC_NODE_SET ) {
    GetClassDescInstance().MakeAutoParamBlocks( this );
}

ClassDesc2* EmberHolder::GetClassDesc() { return &GetClassDescInstance(); }

ClassDesc2& EmberHolder::GetClassDescInstance() {
    static EmberHolderClassDesc theEmberHolderClassDesc;
    return theEmberHolderClassDesc;
}

BaseInterface* EmberHolder::GetInterface( Interface_ID id ) {
    if( id == EmberHolder_INTERFACE )
        return static_cast<IEmberHolder*>( this );
    return frantic::magma::max3d::MagmaHolder::GetInterface( id );
}

#define NODE_SET_ID_CHUNK 30262

extern std::unique_ptr<frantic::magma::magma_interface> CreateSimEmberMagmaInterface();

IOResult EmberHolder::Load( ILoad* iload ) {
    if( iload->PeekNextChunkID() == NODE_SET_ID_CHUNK ) {
        IOResult result;
        ULONG nRead;

        result = iload->OpenChunk();
        if( result != IO_OK )
            return result;

        result = iload->Read( &m_nodeSetID, 8, &nRead );
        if( result != IO_OK )
            return result;

        try {
            this->reset( EmberHolder::create_magma_interface( m_nodeSetID ) );
        } catch( const std::exception& e ) {
            FF_LOG( error ) << e.what() << std::endl;
            return IO_ERROR;
        }

        result = iload->CloseChunk();
        if( result != IO_OK )
            return result;
    }

    return MagmaHolder::Load( iload );
}

IOResult EmberHolder::Save( ISave* isave ) {
    IOResult result;
    ULONG nWritten;

    isave->BeginChunk( NODE_SET_ID_CHUNK );

    result = isave->Write( &m_nodeSetID, 8, &nWritten );
    if( result != IO_OK )
        return result;

    isave->EndChunk();

    return MagmaHolder::Save( isave );
}

std::unique_ptr<frantic::magma::magma_interface> EmberHolder::create_magma_interface( boost::int64_t nodeSetID ) {
    switch( nodeSetID ) {
    case EMBER_BASIC_NODE_SET:
        return std::move( EmberHolderMagmaSingleton::get_instance().create_magma_instance() );
        break;
    case EMBER_SIM_NODE_SET:
        return CreateSimEmberMagmaInterface();
    case STOKE_SIM_NODE_SET:
        return std::move( StokeHolderMagmaSingleton::get_instance().create_magma_instance() );
    default:
        BOOST_THROW_EXCEPTION( std::runtime_error( "Invalid magma node set ID" ) );
    }
}

class EmberMagmaContext : public frantic::magma::magma_compiler_interface::context_base {
  public:
    const RenderGlobalContext* globContext;
    INode* node;

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
        return frantic::magma::magma_compiler_interface::context_base::get_property( name );
    }
};

void EmberHolder::build_expression( INode* node, const RenderGlobalContext& globContext, ember_compiler& outExpr,
                                    Interval& outValidity ) {
    boost::shared_ptr<EmberMagmaContext> ctx( new EmberMagmaContext( globContext, node ) );

    // outExpr.build( this->get_magma_interface(), ctx );
    outExpr.set_abstract_syntax_tree( this->get_magma_interface() );
    outExpr.set_compilation_context( ctx );
    outExpr.build();

    outValidity = this->get_validity( globContext.time );
}

// For use in dllmain
ClassDesc* GetEmberHolderClassDesc() { return &ember::max3d::EmberHolder::GetClassDescInstance(); }

} // namespace max3d
} // namespace ember

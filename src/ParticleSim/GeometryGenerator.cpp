// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/particle_generator.hpp>

#include <stoke/max3d/ParticleSim/BaseInterfacePtr.hpp>
#include <stoke/max3d/ParticleSim/GeometryGenerator.hpp>
#include <stoke/max3d/ParticleSim/MaxTime.hpp>
#include <stoke/max3d/ParticleSim/ParticleGenerator.hpp>
#include <stoke/max3d/ParticleSim/ParticleSet.hpp>

#include <frantic/max3d/geometry/max_mesh_interface.hpp>
#include <frantic/max3d/geometry/mesh.hpp>

#include <stoke/particle_generator_interface.hpp>
#include <stoke/particle_set.hpp>
#include <stoke/rk2_advector.hpp>

#include <frantic/geometry/triangle_utils.hpp>
#include <frantic/graphics/camera.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/empty_particle_istream.hpp>
#include <frantic/particles/streams/rle_levelset_particle_istream.hpp>
#include <frantic/particles/streams/set_channel_particle_istream.hpp>
#include <frantic/particles/streams/surface_particle_istream.hpp>
#include <frantic/particles/streams/transformed_particle_istream.hpp>
#include <frantic/volumetrics/levelset/geometry_to_levelset.hpp>
#include <frantic/volumetrics/levelset/rle_defined_box_iterator.hpp>
#include <frantic/volumetrics/levelset/rle_level_set.hpp>
#include <frantic/volumetrics/levelset/rle_trilerp.hpp>

#include <boost/make_shared.hpp>
#include <boost/random.hpp>

#include <inode.h>

typedef boost::shared_ptr<frantic::particles::streams::particle_istream> particle_istream_ptr;

namespace stoke {
namespace max3d {

bool IsGeometrySource( INode* pNode, TimeValue t ) {
    if( pNode != NULL ) {
        ObjectState os = pNode->EvalWorldState( t );

        if( os.obj && os.obj->CanConvertToType( triObjectClassID ) )
            return true;
    }

    return false;
}

class geometry_generator_impl : public particle_generator {
  public:
    geometry_generator_impl( INode* pNode, const geometry_generator::property_map& properties );

    virtual ~geometry_generator_impl();

    virtual void get_generator_channels( frantic::channels::channel_map& outMap );

    virtual void update( const time_interface& updateTime );

  protected:
    virtual particle_istream_ptr
    generate_next_particles_impl( const frantic::channels::channel_map& requestedChannels );

  private:
    void init( const geometry_generator::property_map& properties );

    boost::shared_ptr<frantic::geometry::mesh_interface>
    get_mesh_interface( TimeValue t, frantic::graphics::transform4f& outTM,
                        frantic::graphics::transform4f& outTMDerivative );

  private:
    AnimHandle m_sourceHandle; // Can't use a reference since this object is not an animatable subclass.

    TimeValue m_currentTime;

    geometry_generator::mode::value_t m_genMode;
    geometry_generator::selection_type::value_t m_selectionType;
    float m_volumeSpacing;
    float m_vertexJitterRadius;
};

FPInterface* CreateGeometryGenerator( INode* pNode, const geometry_generator::property_map& properties ) {
    return new ParticleGenerator( boost::make_shared<geometry_generator_impl>( pNode, properties ) );
}

geometry_generator_impl::geometry_generator_impl( INode* pNode, const geometry_generator::property_map& properties ) {
    m_sourceHandle = Animatable::GetHandleByAnim( pNode );
    m_currentTime =
        GetCOREInterface()->GetAnimRange().Start(); // Reasonable value for initializing the current time. Probably also
                                                    // reasonable to use GetCOREInterface()->GetTime()

    this->init( properties );
    this->set_generator_rate_enabled( true );
}

geometry_generator_impl::~geometry_generator_impl() {}

void geometry_generator_impl::init( const geometry_generator::property_map& properties ) {
    m_genMode = geometry_generator::get_mode( properties );
    m_volumeSpacing = geometry_generator::get_volume_spacing( properties );
    m_selectionType = geometry_generator::get_selection_type( properties );
    m_vertexJitterRadius = geometry_generator::get_vertex_jitter_radius( properties );
}

boost::shared_ptr<frantic::geometry::mesh_interface>
geometry_generator_impl::get_mesh_interface( TimeValue t, frantic::graphics::transform4f& outTM,
                                             frantic::graphics::transform4f& outTMDerivative ) {
    Animatable* pAnim = Animatable::GetAnimByHandle( m_sourceHandle );
    if( !pAnim || !pAnim->IsRefMaker() )
        throw MAXException( _T("Generator source is invalid"), 0 );

    INode* pNode = static_cast<INode*>( pAnim->GetInterface( INODE_INTERFACE ) );
    if( !pNode )
        throw MAXException( _T("Generator source is not a scene node"), 0 );

    ObjectState os = pNode->EvalWorldState( t );

    if( !os.obj->CanConvertToType( triObjectClassID ) )
        throw MAXException( _T("Generator source does not produce geometry"), 0 );

    TriObject* pTriObj = static_cast<TriObject*>( os.obj->ConvertToType( t, triObjectClassID ) );
    if( !pTriObj )
        throw MAXException( _T("Generator source does not produce geometry"), 0 );

    boost::shared_ptr<frantic::max3d::geometry::MaxMeshInterface> pResult(
        new frantic::max3d::geometry::MaxMeshInterface );

    pResult->set_tri_object( pTriObj, ( pTriObj != os.obj ) );

    outTM = frantic::max3d::from_max_t( pNode->GetObjectTM( t ) );

    // Hardcoding the TM derivative to +- 8 ticks, gives (1/16) = 0.0625
    outTMDerivative = 0.0625f * ( frantic::max3d::from_max_t( pNode->GetNodeTM( t + 8 ) ) -
                                  frantic::max3d::from_max_t( pNode->GetNodeTM( t - 8 ) ) );

    return pResult;
}

void geometry_generator_impl::get_generator_channels( frantic::channels::channel_map& outMap ) {
    outMap.reset();

    frantic::graphics::transform4f tm, tmDerivative;

    boost::shared_ptr<frantic::geometry::mesh_interface> pMesh =
        this->get_mesh_interface( m_currentTime, tm, tmDerivative );

    for( frantic::geometry::mesh_interface::mesh_channel_map::const_iterator it = pMesh->get_vertex_channels().begin(),
                                                                             itEnd = pMesh->get_vertex_channels().end();
         it != itEnd; ++it )
        outMap.define_channel( it->first, it->second->get_data_arity(), it->second->get_data_type() );

    for( frantic::geometry::mesh_interface::mesh_channel_map::const_iterator it = pMesh->get_face_channels().begin(),
                                                                             itEnd = pMesh->get_face_channels().end();
         it != itEnd; ++it )
        outMap.define_channel( it->first, it->second->get_data_arity(), it->second->get_data_type() );

    std::vector<frantic::max3d::geometry::MaxMeshInterface::channel_info> channels;

    // Getting the face-vertex channels should get ALL the channels, since that is the most general.
    frantic::max3d::geometry::MaxMeshInterface::get_predefined_channels(
        channels, frantic::geometry::mesh_channel::face_vertex, false );

    for( std::vector<frantic::max3d::geometry::MaxMeshInterface::channel_info>::const_iterator it = channels.begin(),
                                                                                               itEnd = channels.end();
         it != itEnd; ++it ) {
        if( !outMap.has_channel( it->get<0>() ) )
            outMap.define_channel( it->get<0>(), it->get<2>(), it->get<1>() );
    }

    outMap.define_channel<boost::uint16_t>( _T( "ObjectID" ) );
    outMap.define_channel<boost::uint64_t>( _T( "NodeHandle" ) );

    outMap.end_channel_definition();
}

void geometry_generator_impl::update( const time_interface& updateTime ) { m_currentTime = to_ticks( updateTime ); }

class mesh_interface_surface {
  public:
    mesh_interface_surface( boost::shared_ptr<frantic::geometry::mesh_interface> pMesh );

    void set_selection_channel( const frantic::tstring& channelName, bool vertexSelection );

    void get_native_map( frantic::channels::channel_map& outNativeMap ) const;

    void set_channel_map( const frantic::channels::channel_map& seedMap );

    std::size_t surface_count() const;

    std::size_t element_count( std::size_t surfaceIndex ) const;

    float element_area( std::size_t surfaceIndex, std::size_t elementIndex ) const;

    template <class RandomGen>
    void seed_particle( char* outParticle, std::size_t surfaceIndex, std::size_t elementIndex,
                        RandomGen& randomnessGenerator ) const;

  private:
    boost::shared_ptr<frantic::geometry::mesh_interface> m_pMesh;

    const frantic::geometry::mesh_channel* m_pAreaChannel;
    const frantic::geometry::mesh_channel* m_pSelectionChannel;
    const frantic::geometry::mesh_channel* m_pSoftSelectionChannel;

    typedef boost::tuple<frantic::channels::channel_general_accessor, const frantic::geometry::mesh_channel*,
                         frantic::channels::channel_type_convertor_function_t,
                         frantic::channels::channel_weighted_sum_combine_function_t>
        channel_adapter_type;

    std::size_t m_allocSize;
    std::vector<channel_adapter_type> m_vertexChannelAdapters;
    std::vector<channel_adapter_type> m_faceChannelAdapters;
};

inline mesh_interface_surface::mesh_interface_surface( boost::shared_ptr<frantic::geometry::mesh_interface> pMesh ) {
    m_pMesh = pMesh;
    m_pMesh->request_channel( _T("FaceArea"), false, false, true );

    m_pAreaChannel = m_pMesh->get_face_channels().get_channel( _T("FaceArea") );
    m_pSelectionChannel = NULL;
    m_pSoftSelectionChannel = NULL;
}

void mesh_interface_surface::set_selection_channel( const frantic::tstring& channelName, bool vertexSelection ) {
    // if( m_pMesh->get_vertex_channels().has_channel( channelName ) ){
    //	m_pSelectionChannel = m_pMesh->get_vertex_channels().get_channel( channelName );
    // }else if( m_pMesh->get_face_channels().has_channel( channelName ) ){
    // }else if( m_pMesh->request_channel( channelName, true, false, false ) ){
    // }else if( m_pMesh->request_channel( channelName, false, false, false ) ){
    // }

    m_pSelectionChannel = m_pSoftSelectionChannel = NULL;

    if( vertexSelection ) {
        if( m_pMesh->request_channel( channelName, true, false, false ) ) {
            m_pSoftSelectionChannel = m_pMesh->get_vertex_channels().get_channel( channelName );
            if( m_pSoftSelectionChannel->get_data_type() != frantic::channels::data_type_float32 ||
                m_pSoftSelectionChannel->get_data_arity() != 1u )
                m_pSoftSelectionChannel = NULL;
        }
    } else {
        if( m_pMesh->request_channel( channelName, false, false, false ) ) {
            m_pSelectionChannel = m_pMesh->get_face_channels().get_channel( channelName );
            if( m_pSelectionChannel->get_data_type() != frantic::channels::data_type_int8 ||
                m_pSelectionChannel->get_data_arity() != 1u )
                m_pSelectionChannel = NULL;
        }
    }
}

inline void mesh_interface_surface::get_native_map( frantic::channels::channel_map& outNativeMap ) const {
    for( frantic::geometry::mesh_interface::mesh_channel_map::const_iterator
             it = m_pMesh->get_vertex_channels().begin(),
             itEnd = m_pMesh->get_vertex_channels().end();
         it != itEnd; ++it )
        outNativeMap.define_channel( it->first, it->second->get_data_arity(), it->second->get_data_type() );

    for( frantic::geometry::mesh_interface::mesh_channel_map::const_iterator it = m_pMesh->get_face_channels().begin(),
                                                                             itEnd = m_pMesh->get_face_channels().end();
         it != itEnd; ++it )
        outNativeMap.define_channel( it->first, it->second->get_data_arity(), it->second->get_data_type() );

    // TODO: Use the MaxMeshInterface::get_predefined_channels() to get more channels!
}

inline void mesh_interface_surface::set_channel_map( const frantic::channels::channel_map& seedMap ) {
    m_allocSize = 0;
    m_vertexChannelAdapters.clear();
    m_faceChannelAdapters.clear();

    for( std::size_t i = 0, iEnd = seedMap.channel_count(); i < iEnd; ++i ) {
        if( m_pMesh->request_channel( seedMap[i].name(), true, false, false ) ) {
            frantic::channels::channel_general_accessor acc = seedMap.get_general_accessor( i );

            const frantic::geometry::mesh_channel* chMesh =
                m_pMesh->get_vertex_channels().get_channel( seedMap[i].name() );

            frantic::channels::channel_type_convertor_function_t convertFn =
                frantic::channels::get_channel_type_convertor_function( chMesh->get_data_type(), acc.data_type(),
                                                                        seedMap[i].name() );
            frantic::channels::channel_weighted_sum_combine_function_t combineFn =
                frantic::channels::channel_weighted_sum_combine_function( chMesh->get_data_type() );

            if( acc.arity() != chMesh->get_data_arity() )
                continue;

            if( chMesh->get_element_size() > m_allocSize )
                m_allocSize = chMesh->get_element_size();

            m_vertexChannelAdapters.push_back( boost::make_tuple( acc, chMesh, convertFn, combineFn ) );
        } else if( m_pMesh->request_channel( seedMap[i].name(), false, false, false ) ) {
            frantic::channels::channel_general_accessor acc = seedMap.get_general_accessor( i );

            const frantic::geometry::mesh_channel* chMesh =
                m_pMesh->get_face_channels().get_channel( seedMap[i].name() );

            frantic::channels::channel_type_convertor_function_t convertFn =
                frantic::channels::get_channel_type_convertor_function( chMesh->get_data_type(), acc.data_type(),
                                                                        seedMap[i].name() );

            if( acc.arity() != chMesh->get_data_arity() )
                continue;

            if( chMesh->get_element_size() > m_allocSize )
                m_allocSize = chMesh->get_element_size();

            m_faceChannelAdapters.push_back(
                boost::make_tuple( acc, chMesh, convertFn,
                                   static_cast<frantic::channels::channel_weighted_sum_combine_function_t>( NULL ) ) );
        }
    }
}

inline std::size_t mesh_interface_surface::surface_count() const { return 1u; }

std::size_t mesh_interface_surface::element_count( std::size_t /*surfaceIndex*/ ) const {
    return m_pMesh->get_num_faces();
}

float mesh_interface_surface::element_area( std::size_t /*surfaceIndex*/, std::size_t elementIndex ) const {
    if( m_pSoftSelectionChannel ) {
        float selection[3];
        m_pSoftSelectionChannel->get_value( m_pSoftSelectionChannel->get_fv_index( elementIndex, 0 ), selection );
        m_pSoftSelectionChannel->get_value( m_pSoftSelectionChannel->get_fv_index( elementIndex, 1 ), selection + 1 );
        m_pSoftSelectionChannel->get_value( m_pSoftSelectionChannel->get_fv_index( elementIndex, 2 ), selection + 2 );

        float averageWeight =
            ( std::max( 0.f, selection[0] ) + std::max( 0.f, selection[1] ) + std::max( 0.f, selection[2] ) ) / 3.f;
        float area = m_pAreaChannel->frantic::geometry::mesh_channel_interface::get_value<float>( elementIndex );

        // Integral of weights lerped over triangle is the average weight * triangle area. Trust me, I worked it out.
        return averageWeight * area;
    } else {
        if( !m_pSelectionChannel ||
            m_pSelectionChannel->frantic::geometry::mesh_channel_interface::get_value<boost::int8_t>( elementIndex ) !=
                0 )
            return m_pAreaChannel->frantic::geometry::mesh_channel_interface::get_value<float>( elementIndex );
        return 0.f;
    }
}

template <class RandomGen>
void mesh_interface_surface::seed_particle( char* outParticle, std::size_t /*surfaceIndex*/, std::size_t elementIndex,
                                            RandomGen& randomnessGenerator ) const {
    float baryCoords[3];

    frantic::geometry::random_barycentric_coordinate( baryCoords, randomnessGenerator );

    if( m_pSoftSelectionChannel != NULL ) {
        float selection[3];
        m_pSoftSelectionChannel->get_value( m_pSoftSelectionChannel->get_fv_index( elementIndex, 0 ), selection );
        m_pSoftSelectionChannel->get_value( m_pSoftSelectionChannel->get_fv_index( elementIndex, 1 ), selection + 1 );
        m_pSoftSelectionChannel->get_value( m_pSoftSelectionChannel->get_fv_index( elementIndex, 2 ), selection + 2 );

        // We will generate a value between 0 and the largest selection value (clamped to [0,1]).
        // If verts are selected 0, 0, and 0.5, we want to generate test samples in [0,0.5) for example.
        float maxSelection =
            std::max( 0.f, std::min( 1.f, std::max( selection[0], std::max( selection[1], selection[2] ) ) ) );

        // This should never become 0, because that would mean this triangle should not have been selected!

        boost::variate_generator<RandomGen&, boost::uniform_real<float>> rejectionGen(
            randomnessGenerator, boost::uniform_real<float>( 0.f, maxSelection ) );

        // Keep picking new barycentric coordinates until we find one that is selected enough
        while( rejectionGen() >=
               ( baryCoords[0] * selection[0] + baryCoords[1] * selection[1] + baryCoords[2] * selection[2] ) )
            frantic::geometry::random_barycentric_coordinate( baryCoords, randomnessGenerator );
    }

    char* buffer = (char*)alloca( 4 * m_allocSize );

    for( std::vector<channel_adapter_type>::const_iterator it = m_vertexChannelAdapters.begin(),
                                                           itEnd = m_vertexChannelAdapters.end();
         it != itEnd; ++it ) {
        const frantic::geometry::mesh_channel* chMesh = it->get<1>();

        char* pVertValues[] = { buffer, buffer + chMesh->get_element_size(), buffer + 2 * chMesh->get_element_size() };

        chMesh->get_value( chMesh->get_fv_index( elementIndex, 0 ), pVertValues[0] );
        chMesh->get_value( chMesh->get_fv_index( elementIndex, 1 ), pVertValues[1] );
        chMesh->get_value( chMesh->get_fv_index( elementIndex, 2 ), pVertValues[2] );

        char* pCombined = buffer + 3 * chMesh->get_element_size();

        it->get<3>()( baryCoords, pVertValues, 3, chMesh->get_data_arity(), pCombined );
        it->get<2>()( it->get<0>().get_channel_data_pointer( outParticle ), pCombined, it->get<0>().arity() );
    }

    for( std::vector<channel_adapter_type>::const_iterator it = m_faceChannelAdapters.begin(),
                                                           itEnd = m_faceChannelAdapters.end();
         it != itEnd; ++it ) {
        it->get<1>()->get_value( it->get<1>()->get_fv_index( elementIndex, 0 ), buffer );
        it->get<2>()( it->get<0>().get_channel_data_pointer( outParticle ), buffer, it->get<0>().arity() );
    }
}

class edge_surface {
  public:
    edge_surface( boost::shared_ptr<frantic::max3d::geometry::MaxMeshInterface> pMesh );

    void set_selection_channel( const frantic::tstring& channelName, bool vertexSelection );

    void get_native_map( frantic::channels::channel_map& outNativeMap ) const;

    void set_channel_map( const frantic::channels::channel_map& seedMap );

    std::size_t surface_count() const;

    std::size_t element_count( std::size_t surfaceIndex ) const;

    float element_area( std::size_t surfaceIndex, std::size_t elementIndex ) const;

    template <class RandomGen>
    void seed_particle( char* outParticle, std::size_t surfaceIndex, std::size_t elementIndex,
                        RandomGen& randomnessGenerator ) const;

  private:
    boost::shared_ptr<frantic::max3d::geometry::MaxMeshInterface> m_pMesh;

    const AdjEdgeList* m_pEdgeList;

    bool m_useEdgeSelection;

    const frantic::geometry::mesh_channel* m_pFaceSelectionChannel;
    const frantic::geometry::mesh_channel* m_pVertexSelectionChannel;

    typedef boost::tuple<frantic::channels::channel_general_accessor, const frantic::geometry::mesh_channel*,
                         frantic::channels::channel_type_convertor_function_t,
                         frantic::channels::channel_weighted_sum_combine_function_t>
        channel_adapter_type;

    std::size_t m_allocSize;
    std::vector<channel_adapter_type> m_vertexChannelAdapters;
};

edge_surface::edge_surface( boost::shared_ptr<frantic::max3d::geometry::MaxMeshInterface> pMesh )
    : m_pMesh( pMesh ) {
    m_pEdgeList = m_pMesh->get_edge_list();

    m_useEdgeSelection = false;
    m_pFaceSelectionChannel = NULL;
    m_pVertexSelectionChannel = NULL;
}

void edge_surface::set_selection_channel( const frantic::tstring& channelName, bool vertexSelection ) {
    if( vertexSelection ) {
        if( m_pMesh->request_channel( channelName, true, false, false ) ) {
            m_pVertexSelectionChannel = m_pMesh->get_vertex_channels().get_channel( channelName );

            if( m_pVertexSelectionChannel &&
                ( m_pVertexSelectionChannel->get_data_type() != frantic::channels::data_type_float32 ||
                  m_pVertexSelectionChannel->get_data_arity() != 1u ) )
                m_pVertexSelectionChannel = NULL;
        }
    } else {
        m_useEdgeSelection = ( m_pMesh->get_mesh()->selLevel == MESH_EDGE || !m_pMesh->get_mesh()->edgeSel.IsEmpty() );
        /*if( m_pMesh->request_channel( channelName, false, false, false ) ){
                m_pFaceSelectionChannel = m_pMesh->get_face_channels().get_channel( channelName );

                if( m_pFaceSelectionChannel && (m_pFaceSelectionChannel->get_data_type() !=
        frantic::channels::data_type_int8 || m_pFaceSelectionChannel->get_data_arity() != 1u) ) m_pFaceSelectionChannel
        = NULL;
        }*/
    }
}

void edge_surface::get_native_map( frantic::channels::channel_map& outNativeMap ) const {
    for( frantic::geometry::mesh_interface::mesh_channel_map::const_iterator
             it = m_pMesh->get_vertex_channels().begin(),
             itEnd = m_pMesh->get_vertex_channels().end();
         it != itEnd; ++it )
        outNativeMap.define_channel( it->first, it->second->get_data_arity(), it->second->get_data_type() );
}

void edge_surface::set_channel_map( const frantic::channels::channel_map& seedMap ) {
    m_allocSize = 0;
    m_vertexChannelAdapters.clear();

    for( std::size_t i = 0, iEnd = seedMap.channel_count(); i < iEnd; ++i ) {
        if( m_pMesh->get_vertex_channels().has_channel( seedMap[i].name() ) ) {
            frantic::channels::channel_general_accessor acc = seedMap.get_general_accessor( i );

            const frantic::geometry::mesh_channel* chMesh =
                m_pMesh->get_vertex_channels().get_channel( seedMap[i].name() );

            frantic::channels::channel_type_convertor_function_t convertFn =
                frantic::channels::get_channel_type_convertor_function( chMesh->get_data_type(), acc.data_type(),
                                                                        seedMap[i].name() );
            frantic::channels::channel_weighted_sum_combine_function_t combineFn =
                frantic::channels::channel_weighted_sum_combine_function( chMesh->get_data_type() );

            if( acc.arity() != chMesh->get_data_arity() )
                continue;

            if( chMesh->get_element_size() > m_allocSize )
                m_allocSize = chMesh->get_element_size();

            m_vertexChannelAdapters.push_back( boost::make_tuple( acc, chMesh, convertFn, combineFn ) );
        }
    }
}

std::size_t edge_surface::surface_count() const { return 1; }

std::size_t edge_surface::element_count( std::size_t /*surfaceIndex*/ ) const {
    return static_cast<std::size_t>( m_pEdgeList->edges.Count() );
}

float edge_surface::element_area( std::size_t /*surfaceIndex*/, std::size_t elementIndex ) const {
    MEdge& edge = m_pEdgeList->edges[static_cast<INT_PTR>( elementIndex )];

    if( !edge.Visible( const_cast<Face*>( m_pMesh->get_mesh()->faces ) ) )
        return 0.f;

    if( m_pVertexSelectionChannel ) {
        float selection[2];
        m_pVertexSelectionChannel->get_value( edge.v[0], selection );
        m_pVertexSelectionChannel->get_value( edge.v[1], selection + 1 );

        float verts[2][3];
        m_pMesh->get_vert( edge.v[0], verts[0] );
        m_pMesh->get_vert( edge.v[1], verts[1] );

        float length = std::sqrt( frantic::math::square( verts[0][0] - verts[1][0] ) +
                                  frantic::math::square( verts[0][1] - verts[1][1] ) +
                                  frantic::math::square( verts[0][2] - verts[1][2] ) );
        float averageWeight = 0.5f * ( std::max( 0.f, selection[0] ) + std::max( 0.f, selection[1] ) );

        // Integral of weights lerped over line-segment is the average weight * length. Trust me, I worked it out.
        return averageWeight * length;
    } else {
        // if( !m_pFaceSelectionChannel ||
        //	m_pFaceSelectionChannel->frantic::geometry::mesh_channel_interface::get_value<boost::int8_t>( edge.f[0]
        //) != 0 || 	m_pFaceSelectionChannel->frantic::geometry::mesh_channel_interface::get_value<boost::int8_t>(
        //edge.f[1] ) != 0 )
        if( !m_useEdgeSelection || edge.Selected( const_cast<Face*>( m_pMesh->get_mesh()->faces ),
                                                  const_cast<BitArray&>( m_pMesh->get_mesh()->edgeSel ) ) ) {
            float verts[2][3];
            m_pMesh->get_vert( edge.v[0], verts[0] );
            m_pMesh->get_vert( edge.v[1], verts[1] );

            float length = std::sqrt( frantic::math::square( verts[0][0] - verts[1][0] ) +
                                      frantic::math::square( verts[0][1] - verts[1][1] ) +
                                      frantic::math::square( verts[0][2] - verts[1][2] ) );

            return length;
        }
        return 0.f;
    }
}

/**
 * This function will generate a sample from a linear distribution with domain [0,1) with selection weights weightA and
 * weightB respectively. A linear distribution's pdf has relative weights at either end and is linear interpolated in
 * between. A graph of this pdf would look like a triangle on the top of a box.
 *
 * pdf(x) in [0,1) = [ weightA + x*(weightB - weightA) ]/[ 0.5*(weightA + weightB) ]
 * cdf(x) = [ 0.5*(weightB - weightA)*x^2 + weightA*x ] / [ 0.5*(weightA + weightB) ]
 *
 * We use the inversion method to generate a sample from the linear distribution using a sample from the standard
 * uniform distribution.
 * @param weightA The relative weight on the left end of the distribution. Must be non-negative.
 * @param weightB The relative weight on the right end of the distribution. Must be non-negative.
 * @param uniformSample A sample drawn from the standard uniform distribution. ie. uniform [0,1)
 * @return A sample drawn from a linear distribution with the given parameters.
 */
float sample_linear_distribution( float weightA, float weightB, float uniformSample ) {
    float resultSample = uniformSample;

    if( weightA != weightB ) {
        float a = 0.5f * ( weightB - weightA );
        float b = weightA;
        float c = 0.5f * ( weightA + weightB ) * resultSample;

        frantic::math::polynomial_roots::get_quadratic_larger_root( a, b, -c, resultSample );

        assert( 0 <= resultSample && resultSample < 1.f );
    }

    return resultSample;
}

template <class RandomGen>
void edge_surface::seed_particle( char* outParticle, std::size_t /*surfaceIndex*/, std::size_t elementIndex,
                                  RandomGen& randomnessGenerator ) const {
    boost::variate_generator<RandomGen&, boost::uniform_01<float>> rng( randomnessGenerator,
                                                                        boost::uniform_01<float>() );

    MEdge& edge = m_pEdgeList->edges[static_cast<INT_PTR>( elementIndex )];

    float parameter = rng();

    if( m_pVertexSelectionChannel != NULL ) {
        float selection[2];
        m_pVertexSelectionChannel->get_value( edge.v[0], selection );
        m_pVertexSelectionChannel->get_value( edge.v[1], selection + 1 );

        parameter =
            sample_linear_distribution( std::max( 0.f, selection[0] ), std::max( 0.f, selection[1] ), parameter );
    }

    float weights[] = { 1.f - parameter, parameter };

    char* buffer = (char*)alloca(
        3 * m_allocSize ); // Allocate space for the two vertex values, plus somewhere to store the accumulated value.

    for( std::vector<channel_adapter_type>::const_iterator it = m_vertexChannelAdapters.begin(),
                                                           itEnd = m_vertexChannelAdapters.end();
         it != itEnd; ++it ) {
        const frantic::geometry::mesh_channel* chMesh = it->get<1>();

        char* pVertValues[] = { buffer, buffer + chMesh->get_element_size() };

        chMesh->get_value( edge.v[0], pVertValues[0] );
        chMesh->get_value( edge.v[1], pVertValues[1] );

        char* pCombined = buffer + 2 * chMesh->get_element_size();

        it->get<3>()( weights, pVertValues, 2, chMesh->get_data_arity(), pCombined );
        it->get<2>()( it->get<0>().get_channel_data_pointer( outParticle ), pCombined, it->get<0>().arity() );
    }
}

class vertex_surface {
  public:
    vertex_surface( boost::shared_ptr<frantic::geometry::mesh_interface> pMesh );

    void set_selection_channel( const frantic::tstring& channelName );

    void set_jitter_radius( float radius );

    void get_native_map( frantic::channels::channel_map& outNativeMap ) const;

    void set_channel_map( const frantic::channels::channel_map& seedMap );

    std::size_t surface_count() const;

    std::size_t element_count( std::size_t surfaceIndex ) const;

    float element_area( std::size_t surfaceIndex, std::size_t elementIndex ) const;

    template <class RandomGen>
    void seed_particle( char* outParticle, std::size_t surfaceIndex, std::size_t elementIndex,
                        RandomGen& randomnessGenerator ) const;

  private:
    boost::shared_ptr<frantic::geometry::mesh_interface> m_pMesh;

    const frantic::geometry::mesh_channel* m_pVertexSelectionChannel;

    typedef boost::tuple<frantic::channels::channel_general_accessor, const frantic::geometry::mesh_channel*,
                         frantic::channels::channel_type_convertor_function_t>
        channel_adapter_type;

    std::size_t m_allocSize;
    std::vector<channel_adapter_type> m_channelAdapters;

    float m_jitterRadius;
    frantic::channels::channel_accessor<frantic::graphics::vector3f> m_posAccessor;
};

vertex_surface::vertex_surface( boost::shared_ptr<frantic::geometry::mesh_interface> pMesh )
    : m_pMesh( pMesh ) {
    m_pVertexSelectionChannel = NULL;
}

void vertex_surface::set_selection_channel( const frantic::tstring& channelName ) {
    if( m_pMesh->request_channel( channelName, true, false, false ) ) {
        m_pVertexSelectionChannel = m_pMesh->get_vertex_channels().get_channel( channelName );

        if( m_pVertexSelectionChannel &&
            ( m_pVertexSelectionChannel->get_data_type() != frantic::channels::data_type_float32 ||
              m_pVertexSelectionChannel->get_data_arity() != 1u ) )
            m_pVertexSelectionChannel = NULL;
    }
}

void vertex_surface::set_jitter_radius( float radius ) {
    m_jitterRadius = radius;

    if( m_jitterRadius <= 0.f )
        m_posAccessor.reset();
}

void vertex_surface::get_native_map( frantic::channels::channel_map& outNativeMap ) const {
    for( frantic::geometry::mesh_interface::mesh_channel_map::const_iterator
             it = m_pMesh->get_vertex_channels().begin(),
             itEnd = m_pMesh->get_vertex_channels().end();
         it != itEnd; ++it )
        outNativeMap.define_channel( it->first, it->second->get_data_arity(), it->second->get_data_type() );
}

void vertex_surface::set_channel_map( const frantic::channels::channel_map& seedMap ) {
    m_allocSize = 0;
    m_channelAdapters.clear();

    if( m_jitterRadius > 0.f && seedMap.has_channel( _T("Position") ) )
        m_posAccessor = seedMap.get_accessor<frantic::graphics::vector3f>( _T("Position") );

    for( std::size_t i = 0, iEnd = seedMap.channel_count(); i < iEnd; ++i ) {
        if( m_pMesh->get_vertex_channels().has_channel( seedMap[i].name() ) ) {
            frantic::channels::channel_general_accessor acc = seedMap.get_general_accessor( i );

            const frantic::geometry::mesh_channel* chMesh =
                m_pMesh->get_vertex_channels().get_channel( seedMap[i].name() );

            frantic::channels::channel_type_convertor_function_t convertFn =
                frantic::channels::get_channel_type_convertor_function( chMesh->get_data_type(), acc.data_type(),
                                                                        seedMap[i].name() );

            if( acc.arity() != chMesh->get_data_arity() )
                continue;

            if( chMesh->get_element_size() > m_allocSize )
                m_allocSize = chMesh->get_element_size();

            m_channelAdapters.push_back( boost::make_tuple( acc, chMesh, convertFn ) );
        }
    }
}

std::size_t vertex_surface::surface_count() const { return 1; }

std::size_t vertex_surface::element_count( std::size_t /*surfaceIndex*/ ) const { return m_pMesh->get_num_verts(); }

float vertex_surface::element_area( std::size_t /*surfaceIndex*/, std::size_t elementIndex ) const {
    if( m_pVertexSelectionChannel ) {
        float selection;
        m_pVertexSelectionChannel->get_value( elementIndex, &selection );

        return std::max( 0.f, selection );
    } else {
        return 1.f;
    }
}

template <class RandomGen>
void vertex_surface::seed_particle( char* outParticle, std::size_t /*surfaceIndex*/, std::size_t elementIndex,
                                    RandomGen& randomnessGenerator ) const {
    char* buffer = (char*)alloca( m_allocSize );

    for( std::vector<channel_adapter_type>::const_iterator it = m_channelAdapters.begin(),
                                                           itEnd = m_channelAdapters.end();
         it != itEnd; ++it ) {
        const frantic::geometry::mesh_channel* chMesh = it->get<1>();

        chMesh->get_value( elementIndex, buffer );

        it->get<2>()( it->get<0>().get_channel_data_pointer( outParticle ), buffer, it->get<0>().arity() );
    }

    if( m_posAccessor.is_valid() )
        m_posAccessor( outParticle ) +=
            frantic::graphics::vector3f::from_random_in_sphere_rejection( randomnessGenerator, m_jitterRadius );
}

class volume_particle_istream : public frantic::particles::streams::particle_istream {
  private:
    /**
     * This internal struct holds the data required to copy a rle_levelset channel into a
     * particle channel, with possibly a different data type.
     */
    struct channel_connection {
        frantic::channels::channel_general_accessor particleAccessor;
        frantic::volumetrics::levelset::rle_channel_general_accessor levelsetAccessor;
        frantic::channels::channel_type_convertor_function_t conversionFunction;
    };

    boost::shared_ptr<frantic::volumetrics::levelset::rle_level_set> m_pVolume;
    frantic::volumetrics::levelset::rle_defined_iterator m_iter, m_endIter;

    boost::int32_t m_cachedIndices[8];

    frantic::channels::channel_map m_outMap;
    frantic::channels::channel_map m_nativeMap;

    boost::int64_t m_particleIndex;
    boost::int64_t m_particleCount;
    boost::scoped_array<char> m_defaultParticle;

    // Accessors for the various extra channels.
    frantic::channels::channel_accessor<frantic::graphics::vector3f> m_posAccessor;
    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> m_normalAccessor;
    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> m_signedDistanceGradientAccessor;
    frantic::channels::channel_cvt_accessor<float> m_distAccessor;
    frantic::channels::channel_cvt_accessor<float> m_densityAccessor;
    std::vector<channel_connection> m_channels;

    float m_totalVolume; // Stores the total volume inside the levelset

    std::vector<float> m_voxelVolumes; // Stores the volume fraction of each voxel. The volume is for the 8 closest
                                       // voxels with the indexed one as the mininum.

    std::vector<std::size_t> m_voxelSeedCounts; // Stores the number of particles to seed in each voxel.

    boost::mt19937 m_rndGen;

  private:
    void init();

    void set_channel_map_impl( const frantic::channels::channel_map& pcm );

  protected:
    /**
     * This function will copy the channel data for a given voxel into the provided particle. This
     * version will trilerp the data values using the 8 indices in trilerpIndices, and the 8 weights
     * in trilerpWeights.
     *
     * @param localCoord The voxel coordinate (relative the centered sample location).
     * @param localDistance The signed-distance value at localCoord.
     * @param trilerpIndices The 8 indices of the voxels bracketing localCoord.
     * @param trilerpWeights The 8 trilerp weights of the voxels bracketing localCoord.
     * @param pParticle A pointer to the particle to copy the data channels into.
     */
    void copy_channel_data( const frantic::graphics::vector3f& localCoord, float localDistance,
                            boost::int32_t trilerpIndices[], float trilerpWeights[], char* pParticle );

  public:
    volume_particle_istream( const frantic::channels::channel_map& pcm,
                             boost::shared_ptr<frantic::volumetrics::levelset::rle_level_set> pLevelset,
                             boost::int64_t numParticles, boost::uint32_t seed );

    virtual ~volume_particle_istream();

    virtual void close() {}
    virtual frantic::tstring name() const { return _T("volume_particle_istream"); }

    virtual std::size_t particle_size() const { return m_outMap.structure_size(); }

    virtual boost::int64_t particle_count() const { return m_particleCount; }
    virtual boost::int64_t particle_index() const { return m_particleIndex; }
    virtual boost::int64_t particle_count_left() const { return m_particleCount - m_particleIndex - 1; }
    virtual boost::int64_t particle_progress_count() const { return m_particleCount; }
    virtual boost::int64_t particle_progress_index() const { return m_particleIndex; }

    virtual const frantic::channels::channel_map& get_channel_map() const { return m_outMap; }
    virtual const frantic::channels::channel_map& get_native_channel_map() const { return m_nativeMap; }

    virtual void set_channel_map( const frantic::channels::channel_map& particleChannelMap );
    virtual void set_default_particle( char* rawParticleBuffer );

    virtual bool get_particle( char* rawParticleBuffer );
    virtual bool get_particles( char* rawParticleBuffer, std::size_t& numParticles );
};

volume_particle_istream::volume_particle_istream(
    const frantic::channels::channel_map& pcm,
    boost::shared_ptr<frantic::volumetrics::levelset::rle_level_set> pLevelset, boost::int64_t numParticles,
    boost::uint32_t seed )
    : m_pVolume( pLevelset )
    , m_iter( pLevelset->get_rle_index_spec(), false )
    , m_endIter( pLevelset->get_rle_index_spec(), true )
    , m_rndGen( seed ) {
    m_particleIndex = -1;
    m_particleCount = std::max( 0i64, numParticles );

    std::vector<frantic::tstring> levelsetChannels;
    pLevelset->get_channel_names( levelsetChannels );

    m_nativeMap.define_channel<frantic::graphics::vector3f>( _T("Position") );
    m_nativeMap.define_channel<frantic::graphics::vector3f>( _T("Normal") );
    m_nativeMap.define_channel<frantic::graphics::vector3f>( _T("SignedDistanceGradient") );
    m_nativeMap.define_channel<float>( _T("SignedDistance") );
    m_nativeMap.define_channel<float>( _T("Density") );
    for( std::vector<frantic::tstring>::iterator it = levelsetChannels.begin(), itEnd = levelsetChannels.end();
         it != itEnd; ++it ) {
        frantic::volumetrics::levelset::rle_channel_general_accessor acc =
            pLevelset->get_channel_general_accessor( *it );
        if( !m_nativeMap.has_channel( *it ) )
            m_nativeMap.define_channel( *it, acc.arity(), acc.data_type() );
    }
    m_nativeMap.end_channel_definition();

    this->init();
    this->set_channel_map_impl( pcm );
}

volume_particle_istream::~volume_particle_istream() {}

void volume_particle_istream::init() {
    m_totalVolume = 0.f;
    m_voxelVolumes.assign( m_pVolume->size(), 0.f );
    m_voxelSeedCounts.assign( m_pVolume->size(), 0u );

    frantic::volumetrics::levelset::rle_defined_iterator it( m_pVolume->get_rle_index_spec() ),
        itEnd( m_pVolume->get_rle_index_spec(), true );

    // frantic::volumetrics::levelset::rle_defined_box_iterator it( m_pVolume->get_rle_index_spec(),
    // frantic::graphics::boundbox3(m_iter.get_coord(), m_iter.get_coord()+frantic::graphics::size3(1,1,1)) );

    double totalVolume = 0.0;
    double voxelLength = m_pVolume->get_voxel_coord_system().voxel_length();

    for( ; it != itEnd; ++it ) {
        boost::int32_t indices[8];

        m_pVolume->get_rle_index_spec().fill_2x2x2_data_index_box( it.get_coord(), indices );

        bool empty = true;
        double volume = 0.0;

        float d[8];
        for( std::size_t i = 0; i < 8; ++i ) {
            d[i] = m_pVolume->get_using_data_index( indices[i] );

            if( empty &&
                d[i] < 0 ) // Since we are using linear interpolation, make sure we have at least one negative entry
                empty = false;
        }

        if( !empty ) {
            // I'm subdividing the cube 2x2x2 and generating the distance at each sub-voxel using linear interpolation.
            double dSub[] = {
                ( 27.0 * d[0] + 9.0 * ( d[1] + d[2] + d[4] ) + 3.0 * ( d[3] + d[5] + d[6] ) + d[7] ) / 64.0,
                ( 27.0 * d[1] + 9.0 * ( d[0] + d[3] + d[5] ) + 3.0 * ( d[2] + d[4] + d[7] ) + d[6] ) / 64.0,
                ( 27.0 * d[2] + 9.0 * ( d[0] + d[3] + d[6] ) + 3.0 * ( d[1] + d[4] + d[7] ) + d[5] ) / 64.0,
                ( 27.0 * d[3] + 9.0 * ( d[1] + d[2] + d[7] ) + 3.0 * ( d[0] + d[5] + d[6] ) + d[4] ) / 64.0,

                ( 27.0 * d[4] + 9.0 * ( d[5] + d[6] + d[0] ) + 3.0 * ( d[1] + d[2] + d[7] ) + d[3] ) / 64.0,
                ( 27.0 * d[5] + 9.0 * ( d[1] + d[4] + d[7] ) + 3.0 * ( d[0] + d[3] + d[6] ) + d[2] ) / 64.0,
                ( 27.0 * d[6] + 9.0 * ( d[2] + d[4] + d[7] ) + 3.0 * ( d[0] + d[3] + d[5] ) + d[1] ) / 64.0,
                ( 27.0 * d[7] + 9.0 * ( d[3] + d[5] + d[6] ) + 3.0 * ( d[1] + d[2] + d[4] ) + d[0] ) / 64.0 };

            for( std::size_t i = 0; i < 8; ++i )
                volume += 0.5 - 0.5 * frantic::math::clamp( 4.0 * dSub[i] / voxelLength, -1.0, 1.0 );
            // Don't need this division since we can be off by a constant factor for the entire volume and get the same
            // result.
            // volume /= 8.0;
        }

        // We want to be able to place a point in at least 100 trys. If the volume fraction is < 1e-2 that means, on
        // average it would take more than 100 tries with a uniform random sampling, thus we discard cells with small
        // volume. Multiply by 8 to compensate for the missing divide above.
        if( volume > 8e-2 )
            totalVolume += volume;

        m_voxelVolumes[it.get_data_index()] = static_cast<float>( totalVolume );
    }

    // TODO: If it.get_data_index() is not monotonically increasing, we need to compute the cumulative volumes as a
    // separate pass.

    // TODO: We could do another pass, using the per-voxel volumes to assign whole particles, then randomly distribute
    // the remainder.

    // If the toal volume is quite small, we won't bother seeding anything.
    if( totalVolume > 1e-2 ) {
        boost::variate_generator<boost::mt19937&, boost::uniform_real<float>> rng(
            m_rndGen, boost::uniform_real<float>( 0.f, static_cast<float>( totalVolume ) ) );

        for( boost::int64_t i = 0; i < m_particleCount; ++i )
            ++m_voxelSeedCounts[static_cast<std::size_t>(
                std::upper_bound( m_voxelVolumes.begin(), m_voxelVolumes.end(), rng() ) - m_voxelVolumes.begin() )];
    } else {
        m_particleCount = 0;
    }
}

void volume_particle_istream::set_channel_map_impl( const frantic::channels::channel_map& pcm ) {
    { // Swap in a new default particle.
        boost::scoped_array<char> newDefault( new char[pcm.structure_size()] );
        pcm.construct_structure( newDefault.get() );

        if( m_defaultParticle ) {
            frantic::channels::channel_map_adaptor tempAdaptor( pcm, m_outMap );
            tempAdaptor.copy_structure( newDefault.get(), m_defaultParticle.get() );
        }

        m_defaultParticle.swap( newDefault );
    }

    m_outMap = pcm;
    m_posAccessor = m_outMap.get_accessor<frantic::graphics::vector3f>( _T("Position") );

    m_normalAccessor.reset();
    m_signedDistanceGradientAccessor.reset();
    m_distAccessor.reset();
    m_densityAccessor.reset();
    m_channels.clear();

    if( m_outMap.has_channel( _T("Normal") ) )
        m_normalAccessor = m_outMap.get_cvt_accessor<frantic::graphics::vector3f>( _T("Normal") );
    if( m_outMap.has_channel( _T("SignedDistanceGradient") ) )
        m_signedDistanceGradientAccessor =
            m_outMap.get_cvt_accessor<frantic::graphics::vector3f>( _T("SignedDistanceGradient") );
    if( m_outMap.has_channel( _T("SignedDistance") ) )
        m_distAccessor = m_outMap.get_cvt_accessor<float>( _T("SignedDistance") );

    for( std::size_t i = 0, iEnd = m_outMap.channel_count(); i != iEnd; ++i ) {
        const frantic::channels::channel& ch = m_outMap[i];
        // TODO: Should I try to handle this?
        // if( ch.name() == "Position" || ch.name() == "Normal" || ch.name() == "SignedDistance" )
        //	continue;
        if( m_pVolume->has_channel( ch.name() ) ) {
            channel_connection con;
            con.levelsetAccessor = m_pVolume->get_channel_general_accessor( ch.name() );
            con.particleAccessor = m_outMap.get_general_accessor( ch.name() );

            if( con.levelsetAccessor.arity() != con.particleAccessor.arity() )
                throw std::runtime_error( "volume_particle_istream() - Could not match the volume's channel \"" +
                                          frantic::strings::to_string( ch.name() ) + "\" of arity " +
                                          boost::lexical_cast<std::string>( con.levelsetAccessor.arity() ) +
                                          " to the requested particle channel's arity of " +
                                          boost::lexical_cast<std::string>( con.particleAccessor.arity() ) );
            if( con.levelsetAccessor.primitive_size() > 16 )
                throw std::runtime_error(
                    "volume_particle_istream() - Channel \"" + frantic::strings::to_string( ch.name() ) + "\" " +
                    frantic::strings::to_string( ch.type_str() ) + " is too large for the current implementation" );

            con.conversionFunction = frantic::channels::get_channel_type_convertor_function(
                con.levelsetAccessor.data_type(), con.particleAccessor.data_type(), ch.name() );
            m_channels.push_back( con );
        }
    }
}

void volume_particle_istream::set_channel_map( const frantic::channels::channel_map& particleChannelMap ) {
    this->set_channel_map_impl( particleChannelMap );
}

void volume_particle_istream::set_default_particle( char* rawParticleBuffer ) {
    m_defaultParticle.reset( new char[m_outMap.structure_size()] );
    m_outMap.copy_structure( m_defaultParticle.get(), rawParticleBuffer );
}

bool volume_particle_istream::get_particle( char* rawParticleBuffer ) {
    if( ++m_particleIndex >= m_particleCount )
        return false;

    if( m_voxelSeedCounts[m_iter.get_data_index()] == 0 ) {
        do {
            ++m_iter;
        } while( m_iter != m_endIter && m_voxelSeedCounts[m_iter.get_data_index()] == 0 );

        if( m_iter == m_endIter ) {
            m_particleIndex = m_particleCount;
            return false;
        }

        m_pVolume->get_rle_index_spec().fill_2x2x2_data_index_box( m_iter.get_coord(), m_cachedIndices );
    }

    const frantic::volumetrics::voxel_coord_system& vcs = m_pVolume->get_voxel_coord_system();

    boost::variate_generator<boost::mt19937&, boost::uniform_01<float>> rng( m_rndGen, boost::uniform_01<float>() );

    float distance;
    float trilerpWeights[8];
    float deltas[3];

    int watchdog = 0;
    do {
        deltas[0] = rng();
        deltas[1] = rng();
        deltas[2] = rng();

        frantic::volumetrics::levelset::get_trilerp_weights( deltas, trilerpWeights );

        distance = 0;
        for( std::size_t i = 0; i < 8; ++i )
            distance += trilerpWeights[i] * m_pVolume->get_using_data_index( m_cachedIndices[i] );

        if( ++watchdog > 100 ) { // ie. Have we tried and failed 100 times. If we see this a lot, we might need a
                                 // fancier approach to finding points.
            // TODO: We could use cube cases to pick an optimal strategy here.
            //        #1 subdivide the volume (2x2x2 or 4x4x4 or whatever) and make volume fraction estimates to guide
            //        the random placement. #2 For certain cube cases, it would be optimal to build a bounding box of
            //        inside corners + edge crossings and place inside the box.

            // TODO: If this algorithm is a problem, we should develop something that recursively subdivides to the
            // appropriate depth in order to find a point.

            float d[8];
            for( std::size_t i = 0; i < 8; ++i )
                d[i] = m_pVolume->get_using_data_index( m_cachedIndices[i] );

            // Calculate the distance at at a 2x2x2 subgrid inside this voxel so we can approximate volume fractions.
            double dSub[] = {
                ( 27.0 * d[0] + 9.0 * ( d[1] + d[2] + d[4] ) + 3.0 * ( d[3] + d[5] + d[6] ) + d[7] ) / 64.0,
                ( 27.0 * d[1] + 9.0 * ( d[0] + d[3] + d[5] ) + 3.0 * ( d[2] + d[4] + d[7] ) + d[6] ) / 64.0,
                ( 27.0 * d[2] + 9.0 * ( d[0] + d[3] + d[6] ) + 3.0 * ( d[1] + d[4] + d[7] ) + d[5] ) / 64.0,
                ( 27.0 * d[3] + 9.0 * ( d[1] + d[2] + d[7] ) + 3.0 * ( d[0] + d[5] + d[6] ) + d[4] ) / 64.0,

                ( 27.0 * d[4] + 9.0 * ( d[5] + d[6] + d[0] ) + 3.0 * ( d[1] + d[2] + d[7] ) + d[3] ) / 64.0,
                ( 27.0 * d[5] + 9.0 * ( d[1] + d[4] + d[7] ) + 3.0 * ( d[0] + d[3] + d[6] ) + d[2] ) / 64.0,
                ( 27.0 * d[6] + 9.0 * ( d[2] + d[4] + d[7] ) + 3.0 * ( d[0] + d[3] + d[5] ) + d[1] ) / 64.0,
                ( 27.0 * d[7] + 9.0 * ( d[3] + d[5] + d[6] ) + 3.0 * ( d[1] + d[2] + d[4] ) + d[0] ) / 64.0 };

            float v[8];

            double volume = 0.0;
            for( std::size_t i = 0; i < 8; ++i ) {
                volume += 0.5 - 0.5 * frantic::math::clamp( 4.0 * dSub[i] / vcs.voxel_length(), -1.0, 1.0 );

                v[i] = static_cast<float>( volume );
            }

            do {
                float targetVolume = boost::variate_generator<boost::mt19937&, boost::uniform_real<float>>(
                    m_rndGen, boost::uniform_real<float>( 0, v[7] ) )();

                int loc = static_cast<int>( std::upper_bound( v, v + 8, targetVolume ) - v );

                deltas[0] = 0.5f * rng();
                deltas[1] = 0.5f * rng();
                deltas[2] = 0.5f * rng();

                if( loc & 1 )
                    deltas[0] += 0.5f;
                if( loc & 2 )
                    deltas[1] += 0.5f;
                if( loc & 4 )
                    deltas[2] += 0.5f;

                frantic::volumetrics::levelset::get_trilerp_weights( deltas, trilerpWeights );

                distance = 0;
                for( std::size_t i = 0; i < 8; ++i )
                    distance += trilerpWeights[i] * m_pVolume->get_using_data_index( m_cachedIndices[i] );

                if( ++watchdog > 1000 ) {
                    FF_LOG( debug ) << _T("Failed to place a point") << std::endl;
                    break;
                }
            } while( distance > 0.f );

            break;
        }
    } while( distance > 0.f );

    // Copy the default values.
    m_outMap.copy_structure( rawParticleBuffer, m_defaultParticle.get() );

    // Set the world-space position
    m_posAccessor.get( rawParticleBuffer ) = vcs.get_world_coord(
        m_iter.get_coord() + frantic::graphics::vector3f( deltas[0] + 0.5f, deltas[1] + 0.5f, deltas[2] + 0.5f ) );

    // Set the signed distance channel ... TODO: Should this be positive instead?
    if( m_distAccessor.is_valid() )
        m_distAccessor.set( rawParticleBuffer, distance );

    if( m_signedDistanceGradientAccessor.is_valid() ) {
        frantic::graphics::vector3f signedDistGrad;
        frantic::volumetrics::levelset::trilerp_staggered_centered_signed_distance_gradient(
            *m_pVolume, m_iter.get_coord(), signedDistGrad );

        m_signedDistanceGradientAccessor.set( rawParticleBuffer, signedDistGrad );

        if( m_normalAccessor.is_valid() )
            m_normalAccessor.set( rawParticleBuffer, frantic::graphics::vector3f::normalize( signedDistGrad ) );
    } else if( m_normalAccessor.is_valid() ) {
        frantic::graphics::vector3f signedDistGrad;
        frantic::volumetrics::levelset::trilerp_staggered_centered_signed_distance_gradient(
            *m_pVolume, m_iter.get_coord(), signedDistGrad );

        m_normalAccessor.set( rawParticleBuffer, frantic::graphics::vector3f::normalize( signedDistGrad ) );
    }

    // TODO: currently this will barf on channels with more than 128 bits. The correct version
    //        would allocate storage for the largest channel that needs to get copied.
    float tempSpace[4];
    for( std::vector<channel_connection>::iterator it = m_channels.begin(), itEnd = m_channels.end(); it != itEnd;
         ++it ) {
        char* particleChannel = it->particleAccessor.get_channel_data_pointer( rawParticleBuffer );
        char* levelsetChannel = reinterpret_cast<char*>( tempSpace );

        std::size_t arity = it->particleAccessor.arity();
        it->levelsetAccessor.get_trilinear_interpolated( trilerpWeights, m_cachedIndices, levelsetChannel );
        it->conversionFunction( particleChannel, levelsetChannel, arity );
    }

    --m_voxelSeedCounts[m_iter.get_data_index()];

    return true;
}

bool volume_particle_istream::get_particles( char* rawParticleBuffer, std::size_t& numParticles ) {
    std::size_t particleSize = particle_size();
    for( std::size_t i = 0; i < numParticles; ++i, rawParticleBuffer += particleSize ) {
        if( !get_particle( rawParticleBuffer ) ) {
            numParticles = i;
            return false;
        }
    }
    return true;
}

boost::shared_ptr<frantic::volumetrics::levelset::rle_level_set>
create_level_set( const frantic::geometry::trimesh3& theTriMesh, float voxelSpacing ) {
    using namespace frantic::geometry;
    using namespace frantic::graphics;
    using namespace frantic::volumetrics;
    using namespace frantic::volumetrics::levelset;

    boost::shared_ptr<rle_level_set> pResult;

    voxel_coord_system vcs( vector3f(), voxelSpacing );

    pResult.reset( new rle_level_set( vcs ) );

    std::vector<frantic::tstring> meshVertexChannels;
    theTriMesh.get_vertex_channel_names( meshVertexChannels );

    for( std::vector<frantic::tstring>::const_iterator it = meshVertexChannels.begin(),
                                                       itEnd = meshVertexChannels.end();
         it != itEnd; ++it ) {
        const_trimesh3_vertex_channel_general_accessor accessor = theTriMesh.get_vertex_channel_general_accessor( *it );
        pResult->add_channel( *it, accessor.arity(), accessor.data_type() );
    }

    static const float SQRT3 = 1.7320508075688772935274463415059f; // sqrt 3

    convert_geometry_to_levelset( theTriMesh, -SQRT3, SQRT3, *pResult );

    // We want all the levelset values in the volume to be filled, so now we do that using extrapolation.
    rle_index_spec ris;
    ris.build_by_filling( pResult->get_rle_index_spec() );

    pResult->switch_rle_index_spec_with_swap( ris, _T("Populated") );
    pResult->duplicate_channel( _T("PopulatedChannelData"), _T("Populated") );

    pResult->reinitialize_signed_distance_from_populated( _T("Populated"), -std::numeric_limits<float>::max(),
                                                          vcs.voxel_length() * SQRT3 );
    pResult->erase_channel( _T("Populated") );

    pResult->extrapolate_channels( _T("PopulatedChannelData") );
    pResult->erase_channel( _T("PopulatedChannelData") );

    return pResult;
}

particle_istream_ptr
geometry_generator_impl::generate_next_particles_impl( const frantic::channels::channel_map& requestedChannels ) {
    typedef boost::shared_ptr<frantic::particles::streams::particle_istream> particle_istream_ptr;

    frantic::graphics::transform4f tm, tmDerivative;

    boost::shared_ptr<frantic::geometry::mesh_interface> pMesh =
        this->get_mesh_interface( m_currentTime, tm, tmDerivative );

    particle_istream_ptr pSeeds;

    if( m_genMode == geometry_generator::mode::surface ) {
        mesh_interface_surface surfaceWrapper( pMesh );

        if( m_selectionType == geometry_generator::selection_type::face )
            surfaceWrapper.set_selection_channel( _T("FaceSelection"), false );
        else if( m_selectionType == geometry_generator::selection_type::vertex )
            surfaceWrapper.set_selection_channel( _T("Selection"), true );

        boost::shared_ptr<frantic::particles::streams::surface_particle_istream<mesh_interface_surface>> pSurfaceStream;

        pSurfaceStream.reset(
            new frantic::particles::streams::surface_particle_istream<mesh_interface_surface>( surfaceWrapper ) );

        pSurfaceStream->set_particle_count( this->get_generator_rate() );

        pSurfaceStream->set_random_seed( this->get_random_seed() +
                                         m_currentTime ); // HACK: This gives a nicely changing sampling pattern, but I
                                                          // wonder if its a good idea. Is it a bad way to pick a seed?

        pSurfaceStream->set_channel_map( requestedChannels );

        pSeeds = pSurfaceStream;
    } else if( m_genMode == geometry_generator::mode::volume ) {
        // pSeeds.reset( new frantic::particles::streams::empty_particle_istream( requestedChannels ) );
        frantic::geometry::trimesh3 theTriMesh;

        frantic::max3d::geometry::mesh_copy(
            theTriMesh,
            *const_cast<Mesh*>(
                boost::static_pointer_cast<frantic::max3d::geometry::MaxMeshInterface>( pMesh )->get_mesh() ) );

        boost::shared_ptr<frantic::volumetrics::levelset::rle_level_set> pLevelset =
            create_level_set( theTriMesh, m_volumeSpacing );

        boost::shared_ptr<volume_particle_istream> pVolumeStream( new volume_particle_istream(
            requestedChannels, pLevelset, this->get_generator_rate(), this->get_random_seed() + m_currentTime ) );

        pSeeds = pVolumeStream;
    } else if( m_genMode == geometry_generator::mode::edges ) {
        edge_surface surfaceWrapper( boost::static_pointer_cast<frantic::max3d::geometry::MaxMeshInterface>( pMesh ) );

        if( m_selectionType == geometry_generator::selection_type::face )
            surfaceWrapper.set_selection_channel( _T("FaceSelection"), false );
        else if( m_selectionType == geometry_generator::selection_type::vertex )
            surfaceWrapper.set_selection_channel( _T("Selection"), true );

        boost::shared_ptr<frantic::particles::streams::surface_particle_istream<edge_surface>> pSurfaceStream;

        pSurfaceStream.reset(
            new frantic::particles::streams::surface_particle_istream<edge_surface>( surfaceWrapper ) );

        pSurfaceStream->set_particle_count( this->get_generator_rate() );

        pSurfaceStream->set_random_seed( this->get_random_seed() +
                                         m_currentTime ); // HACK: This gives a nicely changing sampling pattern, but I
                                                          // wonder if its a good idea. Is it a bad way to pick a seed?

        pSurfaceStream->set_channel_map( requestedChannels );

        pSeeds = pSurfaceStream;
    } else if( m_genMode == geometry_generator::mode::vertices ) {
        vertex_surface surfaceWrapper( pMesh );

        if( m_selectionType == geometry_generator::selection_type::vertex )
            surfaceWrapper.set_selection_channel( _T("Selection") );

        surfaceWrapper.set_jitter_radius( m_vertexJitterRadius );

        boost::shared_ptr<frantic::particles::streams::surface_particle_istream<vertex_surface>> pVertexStream;

        pVertexStream.reset(
            new frantic::particles::streams::surface_particle_istream<vertex_surface>( surfaceWrapper ) );

        pVertexStream->set_particle_count( this->get_generator_rate() );

        pVertexStream->set_random_seed( this->get_random_seed() +
                                        m_currentTime ); // HACK: This gives a nicely changing sampling pattern, but I
                                                         // wonder if its a good idea. Is it a bad way to pick a seed?

        pVertexStream->set_channel_map( requestedChannels );

        pSeeds = pVertexStream;
    }

    Animatable* pAnim = Animatable::GetAnimByHandle( m_sourceHandle );
    if( !pAnim || !pAnim->IsRefMaker() )
        throw MAXException( _T( "Generator source is invalid" ), 0 );

    INode* pNode = static_cast<INode*>( pAnim->GetInterface( INODE_INTERFACE ) );
    if( !pNode )
        throw MAXException( _T( "Generator source is not a scene node" ), 0 );

    if( !pMesh->has_vertex_channel( _T( "Color" ) ) && requestedChannels.has_channel( _T( "Color" ) ) ) {
        const Color wireColor( pNode->GetWireColor() );
        const frantic::graphics::color3f newColor = frantic::max3d::from_max_t( wireColor );
        pSeeds =
            boost::make_shared<frantic::particles::streams::set_channel_particle_istream<frantic::graphics::color3f>>(
                pSeeds, _T( "Color" ), newColor );
    }

    if( requestedChannels.has_channel( _T( "ObjectID" ) ) ) {
        const UWORD objectID = pNode->GetRenderID();
        pSeeds = boost::make_shared<frantic::particles::streams::set_channel_particle_istream<boost::uint16_t>>(
            pSeeds, _T( "ObjectID" ), static_cast<boost::uint16_t>( objectID ) );
    }

    if( requestedChannels.has_channel( _T( "NodeHandle" ) ) ) {
        const ULONG handle = pNode->GetHandle();
        pSeeds = boost::make_shared<frantic::particles::streams::set_channel_particle_istream<boost::uint64_t>>(
            pSeeds, _T( "NodeHandle" ), static_cast<boost::uint64_t>( handle ) );
    }

    pSeeds = frantic::particles::streams::apply_transform_to_particle_istream( pSeeds, tm, tmDerivative );

    return pSeeds;
}

} // namespace max3d
} // namespace stoke

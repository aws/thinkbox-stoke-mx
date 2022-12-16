// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#if defined( FUMEFX_SDK_AVAILABLE )

#include <stoke/dummy_particle_generator.hpp>
#include <stoke/particle_generator.hpp>

#include <stoke/max3d/ParticleSim/BaseInterfacePtr.hpp>
#include <stoke/max3d/ParticleSim/MaxTime.hpp>
#include <stoke/max3d/ParticleSim/ParticleGenerator.hpp>
#include <stoke/max3d/ParticleSim/ParticleSet.hpp>

#include <frantic/max3d/particles/max3d_particle_utils.hpp>
#include <frantic/max3d/volumetrics/fumefx_field_factory.hpp>

#include <frantic/graphics/camera.hpp>
#include <frantic/particles/particle_array.hpp>

#pragma warning( push, 3 )
#include <inode.h>
#pragma warning( pop )

#include <boost/make_shared.hpp>

using frantic::particles::particle_istream_ptr;

namespace stoke {
namespace max3d {

bool IsFumeFXSource( INode* pNode, TimeValue t ) { return frantic::max3d::volumetrics::is_fumefx_node( pNode, t ); }

class fumefx_generator_impl : public particle_generator {
  public:
    fumefx_generator_impl( INode* pNode );

    virtual ~fumefx_generator_impl();

    virtual void get_generator_channels( frantic::channels::channel_map& outMap );

  protected:
    virtual void update( const time_interface& updateTime );

    virtual particle_istream_ptr
    generate_next_particles_impl( const frantic::channels::channel_map& requestedChannels );

  private:
    AnimHandle m_sourceHandle; // Can't use a reference since this object is not an animatable subclass.

    TimeValue m_currentTime;
};

FPInterface* CreateFumeFXGenerator( INode* pNode ) {
    particle_generator_interface_ptr pImpl;

    pImpl = boost::make_shared<fumefx_generator_impl>( pNode );

    return new ParticleGenerator( pImpl );
}

fumefx_generator_impl::fumefx_generator_impl( INode* pNode ) {
    m_sourceHandle = Animatable::GetHandleByAnim( pNode );
    m_currentTime =
        GetCOREInterface()->GetAnimRange().Start(); // Reasonable value for initializing the current time. Probably also
                                                    // reasonable to use GetCOREInterface()->GetTime()
}

fumefx_generator_impl::~fumefx_generator_impl() {}

void fumefx_generator_impl::get_generator_channels( frantic::channels::channel_map& outMap ) {
    particle_generator::get_generator_channels( outMap, true, true );
}

void fumefx_generator_impl::update( const time_interface& updateTime ) { m_currentTime = to_ticks( updateTime ); }

particle_istream_ptr
fumefx_generator_impl::generate_next_particles_impl( const frantic::channels::channel_map& requestedChannels ) {
    Animatable* pAnim = Animatable::GetAnimByHandle( m_sourceHandle );
    if( !pAnim || !pAnim->IsRefMaker() )
        throw MAXException( _T("Generator source is invalid"), 0 );

    INode* pNode = static_cast<INode*>( pAnim->GetInterface( INODE_INTERFACE ) );
    if( !pNode )
        throw MAXException( _T("Generator source is not a scene node"), 0 );

    if( !frantic::max3d::volumetrics::is_fumefx_node( pNode, m_currentTime ) )
        throw MAXException( _T("Generator source is not a FumeFX node"), 0 );

    particle_istream_ptr fumeStream;

    fumeStream =
        frantic::max3d::volumetrics::get_fumefx_source_particle_istream( pNode, m_currentTime, requestedChannels );
    boost::dynamic_pointer_cast<frantic::max3d::volumetrics::fumefx_source_particle_istream>( fumeStream )
        ->set_particle_count( this->get_generator_rate() );
    boost::dynamic_pointer_cast<frantic::max3d::volumetrics::fumefx_source_particle_istream>( fumeStream )
        ->set_random_seed( this->get_random_seed() + m_currentTime );

    if( requestedChannels.has_channel( _T( "ObjectID" ) ) ) {
        const UWORD objectID = pNode->GetRenderID();
        fumeStream = boost::make_shared<frantic::particles::streams::set_channel_particle_istream<boost::uint16_t>>(
            fumeStream, _T( "ObjectID" ), static_cast<boost::uint16_t>( objectID ) );
    }

    if( requestedChannels.has_channel( _T( "NodeHandle" ) ) ) {
        const ULONG handle = pNode->GetHandle();
        fumeStream = boost::make_shared<frantic::particles::streams::set_channel_particle_istream<boost::uint64_t>>(
            fumeStream, _T( "NodeHandle" ), static_cast<boost::uint64_t>( handle ) );
    }

    return frantic::max3d::particles::transform_stream_with_inode( pNode, m_currentTime, 10, fumeStream );
}

} // namespace max3d
} // namespace stoke
#endif

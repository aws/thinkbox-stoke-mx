// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/dummy_particle_generator.hpp>
#include <stoke/particle_generator.hpp>

#include <stoke/max3d/ParticleSim/BaseInterfacePtr.hpp>
#include <stoke/max3d/ParticleSim/KrakatoaGenerator.hpp>
#include <stoke/max3d/ParticleSim/MaxTime.hpp>
#include <stoke/max3d/ParticleSim/ParticleGenerator.hpp>
#include <stoke/max3d/ParticleSim/ParticleSet.hpp>

#include <frantic/max3d/particles/IMaxKrakatoaPRTObject.hpp>
#include <frantic/max3d/particles/particle_stream_factory.hpp>

#include <frantic/graphics/camera.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/empty_particle_istream.hpp>
#include <frantic/particles/streams/id_culled_particle_istream.hpp>
#include <frantic/particles/streams/shared_particle_container_particle_istream.hpp>
#include <frantic/sort/sort.hpp>

#pragma warning( push, 3 )
#include <inode.h>
#pragma warning( pop )

#include <boost/dynamic_bitset.hpp>
#include <boost/make_shared.hpp>
#include <boost/random.hpp>

typedef boost::shared_ptr<frantic::particles::streams::particle_istream> particle_istream_ptr;

typedef frantic::particles::streams::culling_particle_istream<
    frantic::particles::streams::id_culling_policy<frantic::particles::streams::detail::bitset_traits<boost::int64_t>>>
    id_bitset_culled_particle_istream;

namespace stoke {
namespace max3d {

bool IsParticleSource( INode* pNode, TimeValue t ) {
    return frantic::max3d::particles::is_particle_istream_source( pNode, t, true );
}

class krakatoa_generator_impl : public particle_generator {
  public:
    krakatoa_generator_impl( INode* pNode, float jitterRadius = 0.f, bool ignoreIDs = false );

    virtual ~krakatoa_generator_impl();

    virtual void get_generator_channels( frantic::channels::channel_map& outMap );

  protected:
    virtual void update( const time_interface& updateTime );

    virtual particle_istream_ptr
    generate_next_particles_impl( const frantic::channels::channel_map& requestedChannels );

    virtual void mark_particle_id( boost::int64_t id );

  private:
    AnimHandle m_sourceHandle; // Can't use a reference since this object is not an animatable subclass.

    TimeValue m_currentTime;

    // If the source stream contains IDs we will only seed a particle when a new ID is seen that wasn't present before.
    boost::shared_ptr<boost::dynamic_bitset<>> m_pStaleIDs; // TODO: Switch this to a sparse representation.

    boost::shared_ptr<std::vector<unsigned>> m_pCounts; // Maps from ID to seeding count.
};

FPInterface* CreateKrakatoaGenerator( INode* pNode, float jitterRadius, bool ignoreIDs ) {
    particle_generator_interface_ptr pImpl;

    pImpl = boost::make_shared<krakatoa_generator_impl>( pNode, jitterRadius, ignoreIDs );

    return new ParticleGenerator( pImpl );
}

krakatoa_generator_impl::krakatoa_generator_impl( INode* pNode, float jitterRadius, bool ignoreIDs ) {
    m_sourceHandle = Animatable::GetHandleByAnim( pNode );
    m_currentTime =
        GetCOREInterface()->GetAnimRange().Start(); // Reasonable value for initializing the current time. Probably also
                                                    // reasonable to use GetCOREInterface()->GetTime()

    this->set_ignore_ids( ignoreIDs );
    this->set_jitter_radius( jitterRadius );
}

krakatoa_generator_impl::~krakatoa_generator_impl() {}

void krakatoa_generator_impl::get_generator_channels( frantic::channels::channel_map& outMap ) {
    particle_generator::get_generator_channels( outMap, true, true );
}

void krakatoa_generator_impl::update( const time_interface& updateTime ) { m_currentTime = to_ticks( updateTime ); }

namespace {
class sort_by_external_rank {
    const std::vector<unsigned>* m_pRankArray;
    frantic::channels::channel_accessor<boost::int64_t> m_idAccessor;

  public:
    sort_by_external_rank( const frantic::channels::channel_accessor<boost::int64_t>& idAcc,
                           const std::vector<unsigned>& rankArray )
        : m_pRankArray( &rankArray )
        , m_idAccessor( idAcc ) {}

    bool operator()( const char* lhs, const char* rhs ) const {
        std::size_t lhsID = static_cast<std::size_t>( m_idAccessor( lhs ) );
        std::size_t rhsID = static_cast<std::size_t>( m_idAccessor( rhs ) );

        if( m_pRankArray->size() > lhsID && m_pRankArray->size() > rhsID ) {
            return ( *m_pRankArray )[lhsID] < ( *m_pRankArray )[rhsID];
        } else {
            // We have a less than ordering only if the right side is valid.
            return ( m_pRankArray->size() > rhsID );
        }
    }
};
} // namespace

particle_istream_ptr create_N_particles_from( particle_istream_ptr pStream, std::size_t numRequested,
                                              const boost::shared_ptr<std::vector<unsigned>>& pPrevCount ) {
    std::vector<unsigned>& prevCount = *pPrevCount;

    // 1. Load all particles into a particle_array assigning them a rank from prevCount[ id ];
    frantic::particles::particle_array rankedParticles( pStream->get_channel_map() );
    frantic::channels::channel_accessor<boost::int64_t> idAccessor(
        rankedParticles.get_channel_map().get_accessor<boost::int64_t>( _T("ID") ) );

    rankedParticles.insert_particles( pStream );

    std::size_t numRanked = rankedParticles.size();
    if( numRanked == 0 )
        return particle_istream_ptr( new frantic::particles::streams::empty_particle_istream(
            pStream->get_channel_map(), pStream->get_native_channel_map() ) );

    // 2. Sort the particle_array by non-decreasing rank.
    frantic::sort::parallel_sort( rankedParticles.begin(), rankedParticles.end(),
                                  sort_by_external_rank( idAccessor, prevCount ) );

    // Find the maximum ID that was "new" this time. We only need to search particles with rank 0 to do this.
    if( prevCount.empty() ) {
        prevCount.reserve( 256u );
        prevCount.resize( 1u, 0 );
    }

    boost::int64_t maxID = 0;
    for( frantic::particles::particle_array::const_iterator it = rankedParticles.begin(), itEnd = rankedParticles.end();
         it != itEnd; ++it ) {
        std::size_t id = static_cast<std::size_t>( idAccessor( *it ) );
        if( id >= prevCount.size() ) {
            if( static_cast<boost::int64_t>( id ) > maxID )
                maxID = static_cast<boost::int64_t>( id );
        } else if( prevCount[id] > 0 ) {
            break;
        }
    }

    // Double the vector's size until our highest ID can fit.
    // TODO: Investigate efficient sparse storage for this. Maybe Boost.ICL?
    std::size_t targetSize = prevCount.size();
    if( static_cast<std::size_t>( maxID ) >= targetSize ) {
        do {
            targetSize *= 2;
        } while( static_cast<std::size_t>( maxID ) >= targetSize );

        prevCount.resize( targetSize, 0 );
    }

    // 3. Create a new stream that chooses 'numRequested' particles by always picking a random particle from the set
    // with minimum rank.

    boost::shared_ptr<frantic::particles::particle_array> newParticles(
        new frantic::particles::particle_array( rankedParticles.get_channel_map() ) );

    newParticles->reserve( numRequested );

    unsigned curRank = 0;
    while( newParticles->size() < numRequested ) {
        frantic::particles::particle_array::const_iterator it = rankedParticles.begin(), itEnd = rankedParticles.end();

        std::size_t id = static_cast<std::size_t>( idAccessor( *it ) );
        unsigned pRank = prevCount[id];

        while( newParticles->size() < numRequested && pRank == curRank ) {
            newParticles->push_back( *it );
            ++prevCount[id];

            if( ++it == itEnd )
                break;

            id = static_cast<std::size_t>( idAccessor( *it ) );
            pRank = prevCount[id];
        }

        ++curRank;
        // TODO: Shuffle the particles so we don't always choose the first N?
    }

    pStream.reset(
        new frantic::particles::streams::shared_particle_container_particle_istream<frantic::particles::particle_array>(
            newParticles ) );

    return pStream;
}

particle_istream_ptr
krakatoa_generator_impl::generate_next_particles_impl( const frantic::channels::channel_map& requestedChannels ) {
    using namespace frantic::max3d::particles;

    Animatable* pAnim = Animatable::GetAnimByHandle( m_sourceHandle );
    if( !pAnim || !pAnim->IsRefMaker() )
        throw MAXException( _T("Generator source is invalid"), 0 );

    INode* pNode = static_cast<INode*>( pAnim->GetInterface( INODE_INTERFACE ) );
    if( !pNode )
        throw MAXException( _T("Generator source is not a scene node"), 0 );

    particle_istream_ptr prtStream =
        frantic::max3d::particles::max_particle_istream_factory( pNode, requestedChannels, m_currentTime, 10, true );
    // Check for NULL streams, which may be returned by GetPhoenixParticleIstream().
    // TODO: Should the factory ever return a NULL stream?  Should this be fixed somewhere else?
    if( !prtStream )
        throw std::runtime_error( "max_particle_istream_factory: Got a NULL particle stream from node \"" +
                                  frantic::strings::to_string( pNode->GetName() ) + "\"." );

    boost::int64_t rate = this->get_generator_rate();
    if( rate == 0 )
        return particle_istream_ptr( new frantic::particles::streams::empty_particle_istream(
            prtStream->get_channel_map(), prtStream->get_native_channel_map() ) );

    if( requestedChannels.has_channel( _T("ID") ) && prtStream->get_native_channel_map().has_channel( _T("ID") ) ) {
        if( rate < 0 ) {
            if( !m_pStaleIDs ) {
                m_pStaleIDs.reset( new boost::dynamic_bitset<> );
                m_pStaleIDs->resize( 1024u );
            }

            prtStream.reset(
                new id_bitset_culled_particle_istream( prtStream, boost::make_tuple( m_pStaleIDs, true ) ) );
        } else {
            if( !m_pCounts )
                m_pCounts.reset( new std::vector<unsigned>() );

            prtStream = create_N_particles_from( prtStream, static_cast<std::size_t>( rate ), m_pCounts );
        }
    }

    if( requestedChannels.has_channel( _T( "ObjectID" ) ) ) {
        const UWORD objectID = pNode->GetRenderID();
        prtStream = boost::make_shared<frantic::particles::streams::set_channel_particle_istream<boost::uint16_t>>(
            prtStream, _T( "ObjectID" ), static_cast<boost::uint16_t>( objectID ) );
    }

    if( requestedChannels.has_channel( _T( "NodeHandle" ) ) ) {
        const ULONG handle = pNode->GetHandle();
        prtStream = boost::make_shared<frantic::particles::streams::set_channel_particle_istream<boost::uint64_t>>(
            prtStream, _T( "NodeHandle" ), static_cast<boost::uint64_t>( handle ) );
    }

    return prtStream;
}

void krakatoa_generator_impl::mark_particle_id( boost::int64_t id ) {
    if( m_pStaleIDs ) {
        assert( id > 0 );
        assert( m_pStaleIDs->size() > 0 );

        while( id >= static_cast<boost::int64_t>( m_pStaleIDs->size() ) )
            m_pStaleIDs->resize( 2 * m_pStaleIDs->size() );

        m_pStaleIDs->set( static_cast<std::size_t>( id ) );
    }
}

} // namespace max3d
} // namespace stoke

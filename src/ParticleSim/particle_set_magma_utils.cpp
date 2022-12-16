// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <boost\assign.hpp>
#include <boost\unordered_set.hpp>

#include <tbb\blocked_range.h>
#include <tbb\parallel_for.h>

#include <frantic\magma\max3d\MagmaHolder.hpp>

#include <stoke\dummy_particle_set.hpp>

#include <stoke\max3d\ParticleSim\particle_set_magma_utils.hpp>

namespace detail {

struct magma_parallel_eval {
  private:
    frantic::particles::particle_array& m_particles;
    const frantic::magma::simple_compiler::simple_particle_compiler& m_compiler;

  public:
    magma_parallel_eval( frantic::particles::particle_array& particles,
                         const frantic::magma::simple_compiler::simple_particle_compiler& compiler )
        : m_particles( particles )
        , m_compiler( compiler ) {}

    magma_parallel_eval( const magma_parallel_eval& other )
        : m_particles( other.m_particles )
        , m_compiler( other.m_compiler ) {}

    ~magma_parallel_eval() {}

    void operator()( const tbb::blocked_range<std::size_t>& range ) const {
        for( std::size_t i = range.begin(); i < range.end(); ++i ) {
            m_compiler.eval( m_particles.at( i ), i );
        }
    }
};

} // namespace detail

stoke::particle_set_interface_ptr stoke::max3d::apply_magma_to_particle_set(
    frantic::magma::max3d::IMagmaHolder* pMagmaHolder, stoke::particle_set_interface_ptr pParticleSet, TimeValue t,
    frantic::magma::max3d::IErrorReporter2* pErrorReporter, stoke::reflow_expression_id expressionId ) {
    using frantic::magma::max3d::DebugInformation;
    using frantic::magma::max3d::IMagmaHolder;
    using frantic::magma::max3d::MagmaHolder;
    using stoke::max3d::ParticleReflowContext;

    static const boost::unordered_set<frantic::tstring> specialChannels =
        boost::assign::list_of( _T( "Position" ) )( _T( "ID" ) )( _T( "Velocity" ) )( _T( "Age" ) )( _T( "LifeSpan" ) )(
            _T( "AdvectionOffset" ) )( _T( "FieldVelocity" ) );

    boost::shared_ptr<frantic::magma::magma_interface> magma = pMagmaHolder->get_magma_interface();
    if( magma ) {
        magma->set_expression_id( expressionId );

        boost::shared_ptr<ParticleReflowContext> context( new ParticleReflowContext( t, NULL ) );

        frantic::magma::simple_compiler::simple_particle_compiler compiler;

        frantic::particles::streams::particle_istream_ptr pin =
            boost::make_shared<stoke::particle_set_istream>( pParticleSet );

        try {
            compiler.reset( *pMagmaHolder->get_magma_interface(), pin->get_channel_map(), pin->get_native_channel_map(),
                            context );
        } catch( frantic::magma::magma_exception& e ) {
            FF_LOG( error ) << e.what() << ": " << e.get_message() << std::endl;
            pErrorReporter->SetError( e );
            return boost::make_shared<stoke::dummy_particle_set>();
        }

        pErrorReporter->ClearError();

        pin->set_channel_map( compiler.get_channel_map() );

        boost::int64_t particleCount = pin->particle_count();

        const std::size_t count = static_cast<std::size_t>( particleCount );

        frantic::particles::particle_array tempParticleArray( pin->get_channel_map() );
        tempParticleArray.insert_particles( pin );

        detail::magma_parallel_eval evaluator( tempParticleArray, compiler );
        tbb::parallel_for( tbb::blocked_range<std::size_t>( 0, count ), evaluator );

        frantic::channels::channel_map extraChannels;

        const frantic::channels::channel_map& magmaChannelMap = pin->get_channel_map();
        for( frantic::channels::channel_map_const_iterator it = frantic::channels::begin( magmaChannelMap );
             it != frantic::channels::end( magmaChannelMap ); ++it ) {
            if( specialChannels.find( it->name() ) == specialChannels.end() ) {
                extraChannels.define_channel( it->name(), it->arity(), it->data_type() );
            }
        }

        extraChannels.end_channel_definition();
        boost::shared_ptr<stoke::particle_set> resultParticleSet =
            boost::make_shared<stoke::particle_set>( extraChannels );

        frantic::particles::streams::particle_array_particle_istream resultStream( tempParticleArray );

        resultParticleSet->insert_particles( resultStream );

        return resultParticleSet;
    }

    return pParticleSet;
}
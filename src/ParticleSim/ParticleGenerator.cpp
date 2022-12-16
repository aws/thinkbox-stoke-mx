// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/ParticleSim/IDAllocator.hpp>
#include <stoke/max3d/ParticleSim/IReflowField.hpp>
#include <stoke/max3d/ParticleSim/MaxTime.hpp>
#include <stoke/max3d/ParticleSim/ParticleGenerator.hpp>
#include <stoke/max3d/ParticleSim/ParticleReflowContext.hpp>
#include <stoke/max3d/ParticleSim/ParticleSet.hpp>
#include <stoke/max3d/ParticleSim/particle_set_magma_utils.hpp>

#include <frantic/max3d/exception.hpp>

#include <frantic/channels/channel_map.hpp>

#include <frantic/magma/magma_interface.hpp>

#include <frantic/magma/max3d/DebugInformation.hpp>
#include <frantic/magma/max3d/IMagmaHolder.hpp>

#include <stoke/particle_set.hpp>

#include <boost/exception/diagnostic_information.hpp>

namespace stoke {
namespace max3d {

namespace {
FPInterfaceDesc theIParticleGeneratorDesc(
    ParticleGenerator_INTERFACE, _M( "ParticleGenerator" ), 0, NULL, FP_MIXIN,

    // IParticleGenerator::kFnResetSimulation, _T("ResetSimulation"), 0, TYPE_VOID, 0, 0,
    IParticleGenerator::kFnUpdate, _T("UpdateToTime"), 0, TYPE_VOID, 0, 1, _T("Time"), 0, TYPE_TIMEVALUE,
    IParticleGenerator::kFnGenerateNextParticles, _T("GenerateNextParticles"), 0, TYPE_VOID, 0, 2, _T("ParticleSet"), 0,
    TYPE_INTERFACE, _T("TimeStepSeconds"), 0, TYPE_FLOAT, IParticleGenerator::kFnGenerateNextParticlesWithMagma,
    _T( "GenerateNextParticlesWithMagma" ), 0, TYPE_VOID, 0, 5, _T( "ParticleSet" ), 0, TYPE_INTERFACE,
    _T( "TimeStepSeconds" ), 0, TYPE_FLOAT, _T( "MagmaHolder" ), 0, TYPE_REFTARG, _T( "ErrorReporter" ), 0,
    TYPE_INTERFACE, _T( "Time" ), 0, TYPE_TIMEVALUE,

    properties, IParticleGenerator::kFnGetRandomSeed, IParticleGenerator::kFnSetRandomSeed, _T("RandomSeed"), 0,
    TYPE_INT, IParticleGenerator::kFnGetGeneratorRate, IParticleGenerator::kFnSetGeneratorRate, _T("GeneratorRate"), 0,
    TYPE_INT64, IParticleGenerator::kFnGetDiffusionConstant, IParticleGenerator::kFnSetDiffusionConstant,
    _T("DiffusionConstant"), 0, TYPE_FLOAT, IParticleGenerator::kFnGetInitialVelocityField,
    IParticleGenerator::kFnSetInitialVelocityField, _T("InitialVelocityField"), 0, TYPE_INTERFACE,
    IParticleGenerator::kFnGetIDAllocator, IParticleGenerator::kFnSetIDAllocator, _T("IDAllocator"), 0, TYPE_INTERFACE,
    IParticleGenerator::kFnGetInitialLifespan, IParticleGenerator::kFnSetInitialLifespan, _T("InitialLifespan"), 0,
    TYPE_POINT2_BV, IParticleGenerator::kFnGetExtraChannels, FP_NO_FUNCTION, _T("AvailableChannels"), 0,
    TYPE_TSTR_TAB_BV,

    p_end );
}

IParticleGenerator::~IParticleGenerator() {}

FPInterfaceDesc* IParticleGenerator::GetDesc() { return &theIParticleGeneratorDesc; }

FPInterface* IParticleGenerator::GetInitialVelocityFieldMXS() {
    return static_cast<FPInterface*>( this->GetInitialVelocityField() );
}

void IParticleGenerator::SetInitialVelocityFieldMXS( FPInterface* pVelocityField ) {
    if( !pVelocityField )
        throw MAXException( _T( "InitialVelocityField is undefined" ), 0 );

    IReflowField* pField = static_cast<IReflowField*>( pVelocityField->GetInterface( ReflowField_INTERFACE ) );

    if( !pField )
        throw MAXException( _T("Parameter must be a ReflowField"), 0 );

    this->SetInitialVelocityField( pField );
}

FPInterface* IParticleGenerator::GetIDAllocatorMXS() { return static_cast<FPInterface*>( this->GetIDAllocator() ); }

void IParticleGenerator::SetIDAllocatorMXS( FPInterface* pAllocatorInterface ) {
    if( !pAllocatorInterface )
        throw MAXException( _T( "IDAllocator is undefined" ), 0 );

    IDAllocator* pAllocator = static_cast<IDAllocator*>( pAllocatorInterface->GetInterface( IDAllocator_INTERFACE ) );

    if( !pAllocator )
        throw MAXException( _T("Parameter must be an IDAllocator"), 0 );

    this->SetIDAllocator( pAllocator );
}

void IParticleGenerator::GenerateNextParticlesMXS( FPInterface* pParticleSetInterface, float timeStepSeconds ) {
    if( !pParticleSetInterface )
        throw MAXException( _T( "ParticleSet is undefined" ), 0 );

    IParticleSet* pParticleSet =
        static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

    if( !pParticleSet )
        throw MAXException( _T("Parameter must be a ParticleSet"), 0 );

    this->GenerateNextParticles( pParticleSet, timeStepSeconds );
}

void IParticleGenerator::GenerateNextParticlesWithMagmaMXS( FPInterface* pParticleSetInterface, float timeStepSeconds,
                                                            ReferenceTarget* pMagmaHolderInterface,
                                                            FPInterface* pErrorReporterInterface, TimeValue t ) {
    using frantic::magma::max3d::IErrorReporter2;
    using frantic::magma::max3d::IMagmaHolder;

    if( !pParticleSetInterface )
        throw MAXException( _T( "ParticleSet parameter is undefined" ), 0 );

    if( !pMagmaHolderInterface )
        throw MAXException( _T( "MagmaHolder parameter is undefined" ), 0 );

    if( !pErrorReporterInterface )
        throw MAXException( _T( "ErrorReporter parameter is undefined" ), 0 );

    IParticleSet* pParticleSet =
        static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

    if( !pParticleSet )
        throw MAXException( _T( "Parameter must be a ParticleSet" ), 0 );

    IMagmaHolder* pMagmaHolder =
        static_cast<IMagmaHolder*>( pMagmaHolderInterface->GetInterface( MagmaHolder_INTERFACE ) );

    if( !pMagmaHolder ) {
        throw MAXException( _T( "Parameter must be a MagmaHolder" ), 0 );
    }

    IErrorReporter2* pErrorReporter =
        static_cast<IErrorReporter2*>( pErrorReporterInterface->GetInterface( ErrorReport2_INTERFACE ) );

    if( !pErrorReporter ) {
        throw MAXException( _T( "Parameter must be an ErrorReporter" ), 0 );
    }

    this->GenerateNextParticlesWithMagma( pParticleSet, timeStepSeconds, pMagmaHolder, pErrorReporter, t );
}

ParticleGenerator::ParticleGenerator( particle_generator_interface_ptr pImpl )
    : m_impl( pImpl ) {
    m_idAllocator = new IDAllocator( pImpl->get_id_allocator() );
}

ParticleGenerator::~ParticleGenerator() {}

particle_generator_interface_ptr ParticleGenerator::GetImpl() { return m_impl; }

int ParticleGenerator::GetRandomSeed() { return m_impl->get_random_seed(); }

void ParticleGenerator::SetRandomSeed( int seed ) { m_impl->set_random_seed( seed ); }

__int64 ParticleGenerator::GetGeneratorRate() { return m_impl->get_generator_rate(); }

void ParticleGenerator::SetGeneratorRate( __int64 numPerFrame ) {
    if( numPerFrame >= 0 )
        m_impl->set_generator_rate( numPerFrame );
    else
        m_impl->set_generator_rate_enabled( false );
}

Point2 ParticleGenerator::GetInitialLifespan() {
    return Point2( m_impl->get_initial_lifespan_min(), m_impl->get_initial_lifespan_max() );
}

void ParticleGenerator::SetInitialLifespan( const Point2& lifespan ) {
    m_impl->set_initial_lifespan_min( lifespan.x );
    m_impl->set_initial_lifespan_max( lifespan.y );
}

float ParticleGenerator::GetDiffusionConstant() const { return m_impl->get_diffusion_constant(); }

void ParticleGenerator::SetDiffusionConstant( float kDiffusion ) { m_impl->set_diffusion_constant( kDiffusion ); }

IReflowField* ParticleGenerator::GetInitialVelocityField() { return m_initialVelocityField.get(); }

void ParticleGenerator::SetInitialVelocityField( IReflowField* pVelocityField ) {
    m_initialVelocityField = pVelocityField; // TODO: Is this sketchy?
    m_impl->set_initial_velocity_field( pVelocityField->GetImpl() );
}

IDAllocator* ParticleGenerator::GetIDAllocator() { return m_idAllocator.get(); }

void ParticleGenerator::SetIDAllocator( IDAllocator* pAllocator ) {
    m_idAllocator = pAllocator;
    m_impl->set_id_allocator( pAllocator->GetImpl() );
}

Tab<MSTR*> ParticleGenerator::GetExtraChannels( TimeValue /*t*/ ) {
    m_cachedStrings.clear();

    frantic::channels::channel_map sourceMap;

    m_impl->get_generator_channels( sourceMap );

    for( std::size_t i = 0, iEnd = sourceMap.channel_count(); i < iEnd; ++i ) {
        const frantic::channels::channel& ch = sourceMap[i];

        frantic::tstring channelType = frantic::channels::channel_data_type_str( ch.arity(), ch.data_type() );

        MSTR channelTypeString;
        channelTypeString.printf( _T("%s %s"), ch.name().c_str(), channelType.c_str() );

        m_cachedStrings.push_back( channelTypeString );
    }

    Tab<MSTR*> result;
    result.SetCount( static_cast<int>( m_cachedStrings.size() ) );

    int i = 0;
    for( std::vector<MSTR>::iterator it = m_cachedStrings.begin(), itEnd = m_cachedStrings.end(); it != itEnd;
         ++it, ++i )
        result[i] = &( *it );

    return result;
}

void ParticleGenerator::Update( TimeValue updateTime ) {
    MaxTime timeWrapper( updateTime );

    try {
        m_impl->update( timeWrapper );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void ParticleGenerator::GenerateNextParticles( IParticleSet* pParticleSetInterface, float timeStepSeconds ) {
    try {
        using frantic::magma::max3d::DebugInformation;
        using frantic::magma::max3d::IMagmaHolder;
        using stoke::max3d::ParticleReflowContext;

        if( !pParticleSetInterface ) {
            throw MAXException( _T( "Invalid parameter" ), 0 );
        }

        IParticleSet* pParticleSet =
            static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

        if( !pParticleSet ) {
            throw MAXException( _T( "Parameter must be a ParticleSet" ), 0 );
        }

        // Actually generate the particles
        m_impl->generate_next_particles( pParticleSet->GetImpl(), timeStepSeconds );

    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void ParticleGenerator::GenerateNextParticlesWithMagma( IParticleSet* pParticleSetInterface, float timeStepSeconds,
                                                        frantic::magma::max3d::IMagmaHolder* pMagmaHolder,
                                                        frantic::magma::max3d::IErrorReporter2* pErrorReporter,
                                                        TimeValue t ) {
    try {
        using frantic::magma::max3d::DebugInformation;
        using frantic::magma::max3d::IMagmaHolder;
        using stoke::max3d::ParticleReflowContext;

        if( !pParticleSetInterface ) {
            throw MAXException( _T( "ParticleSet parameter is undefined" ), 0 );
        }

        if( !pMagmaHolder )
            throw MAXException( _T( "MagmaHolder parameter is undefined" ), 0 );

        if( !pErrorReporter )
            throw MAXException( _T( "ErrorReporter parameter is undefined" ), 0 );

        IParticleSet* pParticleSet =
            static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

        if( !pParticleSet ) {
            throw MAXException( _T( "Parameter must be a ParticleSet" ), 0 );
        }

        // Actually generate the particles

        // We'll create a new particle_set to insert the particles into before we apply the birth Magma.
        stoke::particle_set_interface_ptr resultParticleSet =
            boost::make_shared<stoke::particle_set>( pParticleSet->GetImpl()->get_extra_channels() );
        m_impl->generate_next_particles( resultParticleSet, timeStepSeconds );

        // Now let's apply the birth magma
        stoke::particle_set_interface_ptr magmaParticleSet =
            apply_magma_to_particle_set( pMagmaHolder, resultParticleSet, t, pErrorReporter, stoke::kBirthExpression );

        if( pParticleSet->GetCount() == 0 ) {
            pParticleSet->reset( magmaParticleSet );
        } else {
            boost::shared_ptr<stoke::particle_set> castedSet =
                boost::dynamic_pointer_cast<stoke::particle_set>( pParticleSet->GetImpl() );

            if( castedSet && magmaParticleSet ) {
                castedSet->insert_particles( stoke::particle_set_istream( magmaParticleSet ) );
            }
        }

    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

} // namespace max3d
} // namespace stoke

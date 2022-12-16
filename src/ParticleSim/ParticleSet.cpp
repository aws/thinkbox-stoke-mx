// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/ParticleSim/IAdvector.hpp>
#include <stoke/max3d/ParticleSim/IReflowField.hpp>
#include <stoke/max3d/ParticleSim/ParticleSet.hpp>
#include <stoke/max3d/ParticleSim/particle_set_magma_utils.hpp>

#include <stoke/dummy_particle_set.hpp>
#include <stoke/particle_set.hpp>
#include <stoke/particle_set_istream.hpp>
#include <stoke/particle_set_serializer.hpp>

#include <frantic/max3d/convert.hpp>

#include <frantic/magma/max3d/DebugInformation.hpp>
#include <frantic/magma/max3d/IMagmaHolder.hpp>

#include <frantic/magma/simple_compiler/simple_particle_compiler.hpp>

#include <frantic/channels/channel_map_const_iterator.hpp>
#include <frantic/particles/particle_file_stream_factory.hpp>
#include <frantic/particles/particle_istream_iterator.hpp>
#include <frantic/particles/streams/particle_array_particle_istream.hpp>

#include <boost/make_shared.hpp>
#include <boost/random.hpp>

#include <stoke/max3d/ParticleSim/ParticleReflowContext.hpp>

namespace stoke {
namespace max3d {

namespace {
FPInterfaceDesc theIParticleSetDesc(
    ParticleSet_INTERFACE, _M( "ParticleSet" ), 0, NULL, FP_MIXIN,

    IParticleSet::kFnGetPosition, _T("GetPosition"), 0, TYPE_POINT3_BV, 0, 1, _T("Index"), 0, TYPE_INT64,
    IParticleSet::kFnGetVelocity, _T("GetVelocity"), 0, TYPE_POINT3_BV, 0, 1, _T("Index"), 0, TYPE_INT64,
    IParticleSet::kFnGetAge, _T("GetAge"), 0, TYPE_FLOAT, 0, 1, _T("Index"), 0, TYPE_INT64,
    IParticleSet::kFnGetLifespan, _T("GetLifespan"), 0, TYPE_FLOAT, 0, 1, _T("Index"), 0, TYPE_INT64,
    IParticleSet::kFnGetID, _T("GetID"), 0, TYPE_INT64, 0, 1, _T("Index"), 0, TYPE_INT64, IParticleSet::kFnSetPosition,
    _T("SetPosition"), 0, TYPE_VOID, 0, 2, _T("Index"), 0, TYPE_INT64, _T("Value"), 0, TYPE_POINT3,
    IParticleSet::kFnSetVelocity, _T("SetVelocity"), 0, TYPE_VOID, 0, 2, _T("Index"), 0, TYPE_INT64, _T("Value"), 0,
    TYPE_POINT3, IParticleSet::kFnSetAge, _T("SetAge"), 0, TYPE_VOID, 0, 2, _T("Index"), 0, TYPE_INT64, _T("Value"), 0,
    TYPE_FLOAT, IParticleSet::kFnSetLifespan, _T("SetLifespan"), 0, TYPE_VOID, 0, 2, _T("Index"), 0, TYPE_INT64,
    _T("Value"), 0, TYPE_FLOAT, IParticleSet::kFnDeleteDeadParticles, _T("DeleteDeadParticles"), 0, TYPE_VOID, 0, 0,
    // IParticleSet::kFnClone, _T("Clone"), 0, TYPE_INTERFACE, 0, 0,

    properties, IParticleSet::kFnGetCount, FP_NO_FUNCTION, _T("Count"), 0, TYPE_INT64,

    p_end );
}

IParticleSet::~IParticleSet() {}

FPInterfaceDesc* IParticleSet::GetDesc() { return &theIParticleSetDesc; }

ParticleSet::ParticleSet( particle_set_interface_ptr pImpl )
    : m_impl( pImpl ) {}

void ParticleSet::reset( particle_set_interface_ptr newImpl ) { m_impl = newImpl; }

ParticleSet::~ParticleSet() {}

particle_set_interface_ptr ParticleSet::GetImpl() { return m_impl; }

ParticleSet::index_type ParticleSet::GetCount() { return static_cast<index_type>( m_impl->get_count() ); }

Point3 ParticleSet::GetPosition( index_type i ) {
    return frantic::max3d::to_max_t( m_impl->get_position( static_cast<particle_set_interface::index_type>( i ) ) );
}

Point3 ParticleSet::GetVelocity( index_type i ) {
    return frantic::max3d::to_max_t( m_impl->get_velocity( static_cast<particle_set_interface::index_type>( i ) ) );
}

float ParticleSet::GetAge( index_type i ) {
    return m_impl->get_age( static_cast<particle_set_interface::index_type>( i ) );
}

float ParticleSet::GetLifespan( index_type i ) {
    return m_impl->get_lifespan( static_cast<particle_set_interface::index_type>( i ) );
}

ParticleSet::id_type ParticleSet::GetParticleID( index_type i ) {
    return static_cast<id_type>( m_impl->get_id( static_cast<particle_set_interface::index_type>( i ) ) );
}

Point3 ParticleSet::GetAdvector( index_type i ) {
    return frantic::max3d::to_max_t(
        m_impl->get_advection_offset( static_cast<particle_set_interface::index_type>( i ) ) );
}

void ParticleSet::SetPosition( index_type i, const Point3& p ) {
    return m_impl->set_position( static_cast<particle_set_interface::index_type>( i ),
                                 frantic::max3d::from_max_t( p ) );
}

void ParticleSet::SetVelocity( index_type i, const Point3& v ) {
    return m_impl->set_velocity( static_cast<particle_set_interface::index_type>( i ),
                                 frantic::max3d::from_max_t( v ) );
}

void ParticleSet::SetAge( index_type i, float age ) {
    return m_impl->set_age( static_cast<particle_set_interface::index_type>( i ), age );
}

void ParticleSet::SetLifespan( index_type i, float lifespan ) {
    return m_impl->set_lifespan( static_cast<particle_set_interface::index_type>( i ), std::max( 0.f, lifespan ) );
}

void ParticleSet::SetAdvectionOffset( index_type i, const Point3& advectionOffset ) {
    return m_impl->set_advection_offset( static_cast<particle_set_interface::index_type>( i ),
                                         frantic::max3d::from_max_t( advectionOffset ) );
}

void ParticleSet::DeleteDeadParticles() {
    class dead_predicate : public particle_set_interface::predicate {
      public:
        virtual bool evaluate( particle_set_interface& ps, particle_set_interface::index_type i ) const {
            return ps.get_age( i ) >= ps.get_lifespan( i );
        }
    } thePredicate;

    m_impl->delete_particles_if( thePredicate );
}

// IParticleSet* ParticleSet::Clone(){
//	return new ParticleSet( m_impl->clone() );
// }

#if MAX_VERSION_MAJOR < 15
FPInterface* CreateParticleSet( const Tab<MCHAR*>* extraChannels ) {
#else
FPInterface* CreateParticleSet( const Tab<const MCHAR*>* extraChannels ) {
#endif
    particle_set_interface_ptr pImpl;

    frantic::channels::channel_map extraChannelMap;

    for( int i = 0, iEnd = extraChannels->Count(); i < iEnd; ++i ) {
        const MCHAR* szChannel = ( *extraChannels )[i];
        if( !szChannel || *szChannel == _T( '\0' ) )
            continue;

        frantic::tstring channel = szChannel;

        frantic::tstring::size_type splitPos = channel.find_last_of( _T( ' ' ) );
        if( splitPos == frantic::tstring::npos )
            throw MAXException( _T("Invalid channel specification") );

        frantic::tstring name = channel.substr( 0, splitPos );
        frantic::tstring typeString = channel.substr( splitPos + 1 );

        if( !frantic::channels::is_valid_channel_name( name ) )
            throw MAXException( _T("Invalid channel name") );

        std::pair<frantic::channels::data_type_t, std::size_t> type =
            frantic::channels::channel_data_type_and_arity_from_string( typeString );
        if( type.first == frantic::channels::data_type_invalid )
            throw MAXException( _T("Invalid channel type") );

        extraChannelMap.define_channel( name, type.second, type.first );
    }
    extraChannelMap.end_channel_definition();

    char* defaultValues = NULL;

    if( extraChannelMap.channel_count() > 0 ) {
        defaultValues = (char*)alloca( extraChannelMap.structure_size() );

        extraChannelMap.construct_structure( defaultValues );
    }

    pImpl = boost::make_shared<particle_set>( extraChannelMap, defaultValues );

    return new ParticleSet( pImpl );
}

void AdvectParticleSet( IParticleSet* pParticleSet, IAdvector* pAdvector, IReflowField* pVelocityField,
                        float timeStepSeconds ) {
    pParticleSet->GetImpl()->advect_particles( *pAdvector->GetImpl(), *pVelocityField->GetImpl(), timeStepSeconds );
}

void UpdateAgeChannel( IParticleSet* pParticleSet, float timeStepSeconds ) {
    pParticleSet->GetImpl()->update_age( timeStepSeconds );
}

void UpdateAdvectionOffsetChannel( IParticleSet* pParticleSet, IAdvector* pAdvector, IReflowField* pVelocityField,
                                   float timeStepSeconds ) {
    pParticleSet->GetImpl()->update_advection_offset( *pAdvector->GetImpl(), *pVelocityField->GetImpl(),
                                                      timeStepSeconds );
}

void ApplyAdvectionOffsetChannel( IParticleSet* pParticleSet ) { pParticleSet->GetImpl()->apply_advection_offset(); }

void AdvectParticleSetMXS( FPInterface* pParticleSetInterface, FPInterface* pAdvectorInterface,
                           FPInterface* pVelocityFieldInterface, float timeStepSeconds ) {
    if( !pParticleSetInterface || !pAdvectorInterface || !pVelocityFieldInterface )
        throw MAXException( _T("Invalid parameter"), 0 );

    IParticleSet* pParticleSet =
        static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

    if( !pParticleSet )
        throw MAXException( _T("Parameter must be a ParticleSet"), 0 );

    IAdvector* pAdvector = static_cast<IAdvector*>( pAdvectorInterface->GetInterface( Advector_INTERFACE ) );

    if( !pAdvector )
        throw MAXException( _T("Parameter must be an Advector"), 0 );

    IReflowField* pField = static_cast<IReflowField*>( pVelocityFieldInterface->GetInterface( ReflowField_INTERFACE ) );

    if( !pField )
        throw MAXException( _T("Parameter must be a ReflowField"), 0 );

    AdvectParticleSet( pParticleSet, pAdvector, pField, timeStepSeconds );
}

void UpdateAgeChannelMXS( FPInterface* pParticleSetInterface, float timeStepSeconds ) {
    if( !pParticleSetInterface )
        throw MAXException( _T( "Invalid parameter" ), 0 );

    IParticleSet* pParticleSet =
        static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

    if( !pParticleSet )
        throw MAXException( _T( "Parameter must be a ParticleSet" ), 0 );

    UpdateAgeChannel( pParticleSet, timeStepSeconds );
}

void UpdateAdvectionOffsetChannelMXS( FPInterface* pParticleSetInterface, FPInterface* pAdvectorInterface,
                                      FPInterface* pVelocityFieldInterface, float timeStepSeconds ) {
    if( !pParticleSetInterface || !pAdvectorInterface || !pVelocityFieldInterface )
        throw MAXException( _T( "Invalid parameter" ), 0 );

    IParticleSet* pParticleSet =
        static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

    if( !pParticleSet )
        throw MAXException( _T( "Parameter must be a ParticleSet" ), 0 );

    IAdvector* pAdvector = static_cast<IAdvector*>( pAdvectorInterface->GetInterface( Advector_INTERFACE ) );

    if( !pAdvector )
        throw MAXException( _T( "Parameter must be an Advector" ), 0 );

    IReflowField* pField = static_cast<IReflowField*>( pVelocityFieldInterface->GetInterface( ReflowField_INTERFACE ) );

    if( !pField )
        throw MAXException( _T( "Parameter must be a ReflowField" ), 0 );

    UpdateAdvectionOffsetChannel( pParticleSet, pAdvector, pField, timeStepSeconds );
}

void ApplyAdvectionOffsetChannelMXS( FPInterface* pParticleSetInterface ) {
    if( !pParticleSetInterface )
        throw MAXException( _T( "Invalid parameter" ), 0 );

    IParticleSet* pParticleSet =
        static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

    if( !pParticleSet )
        throw MAXException( _T( "Parameter must be a ParticleSet" ), 0 );

    ApplyAdvectionOffsetChannel( pParticleSet );
}

void MagmaEvaluateParticleSet( FPInterface* pParticleSetInterface, ReferenceTarget* pMagmaHolderInterface, TimeValue t,
                               FPInterface* pErrorReporterInterface ) {
    using frantic::magma::max3d::IErrorReporter2;
    using frantic::magma::max3d::IMagmaHolder;

    if( !pParticleSetInterface || !pMagmaHolderInterface ) {
        throw MAXException( _T( "Invalid parameter" ), 0 );
    }

    IParticleSet* pParticleSet =
        static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

    if( !pParticleSet ) {
        throw MAXException( _T( "Parameter must be a ParticleSet" ), 0 );
    }

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

    stoke::particle_set_interface_ptr magmaResult = apply_magma_to_particle_set(
        pMagmaHolder, pParticleSet->GetImpl(), t, pErrorReporter, stoke::kPerStepExpression );
    if( magmaResult ) {
        pParticleSet->reset( magmaResult );
    }
}

void WriteParticleSet( IParticleSet* particles, const MCHAR* szFilePath ) {
    if( !szFilePath )
        szFilePath = _T("");

    frantic::channels::channel_map pcm;
    pcm.define_channel( _T("Position"), 3, frantic::channels::data_type_float32 );
    pcm.define_channel( _T("Velocity"), 3, frantic::channels::data_type_float32 );
    pcm.define_channel( _T("Age"), 1, frantic::channels::data_type_float32 );
    pcm.define_channel( _T("LifeSpan"), 1, frantic::channels::data_type_float32 );
    pcm.define_channel( _T("ID"), 1, frantic::channels::data_type_int64 );
    pcm.define_channel( _T( "AdvectionOffset" ), 3, frantic::channels::data_type_float32 );
    pcm.define_channel( _T( "FieldVelocity" ), 3, frantic::channels::data_type_float32 );

    particle_set_interface_ptr pImpl = particles->GetImpl();

    for( std::size_t i = 0, iEnd = pImpl->get_num_channels(); i < iEnd; ++i ) {
        std::pair<frantic::channels::data_type_t, std::size_t> type;
        frantic::tstring channelName;

        pImpl->get_particle_channel( i, &channelName, &type );

        pcm.define_channel( channelName, type.second, type.first );
    }

    pcm.end_channel_definition( 1u );

    boost::shared_ptr<frantic::particles::streams::particle_ostream>
        pOutStream; // = frantic::particles::particle_file_ostream_factory( szFilePath, pcm, pcm, particles->GetCount()
                    // );

    pOutStream = stoke::particle_file_ostream_factory( szFilePath, pcm, pcm, particles->GetCount() );

    write_particle_set( *pImpl, *pOutStream );

    pOutStream->close();
}

void WriteParticleSetMXS( FPInterface* particleSet, const MCHAR* szFilePath ) {
    IParticleSet* ps = static_cast<IParticleSet*>( particleSet->GetInterface( ParticleSet_INTERFACE ) );
    if( !ps )
        throw MAXException( _T("Parameter was not an IParticleSet"), 0 );

    WriteParticleSet( ps, szFilePath );
}

IParticleSet* ReadParticleSet( const MCHAR* szFilePath ) {
    frantic::particles::particle_istream_ptr pStream;

    try {
        pStream = frantic::particles::particle_file_istream_factory( szFilePath );
    } catch( const std::exception& e ) {
        FF_LOG( error ) << e.what() << std::endl;
    }

    if( !pStream )
        return NULL;

    pStream->set_channel_map( pStream->get_native_channel_map() );

    return new ParticleSet( read_particle_set( *pStream ) );
}

} // namespace max3d
} // namespace stoke

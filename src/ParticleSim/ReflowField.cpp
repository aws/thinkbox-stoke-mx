// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/ParticleSim/MaxTime.hpp>
#include <stoke/max3d/ParticleSim/ReflowField.hpp>

#include <stoke/additive_velocity_field.hpp>
#include <stoke/dummy_field.hpp>
#include <stoke/field.hpp>
#include <stoke/particle_splat_field.hpp>

#include <ember/constant_field.hpp>

#include <frantic/max3d/convert.hpp>
#include <frantic/max3d/exception.hpp>
#include <frantic/max3d/particles/particle_stream_factory.hpp>
#include <frantic/max3d/volumetrics/IEmberField.hpp>
#include <frantic/max3d/volumetrics/force_field_adapter.hpp>
#include <frantic/max3d/volumetrics/fumefx_field_factory.hpp>
#include <frantic/max3d/volumetrics/phoenix_field.hpp>

#include <frantic/particles/streams/empty_particle_istream.hpp>

#include <boost/make_shared.hpp>

namespace stoke {
namespace max3d {

namespace {
FPInterfaceDesc theIReflowFieldDesc( ReflowField_INTERFACE, _M( "ReflowField" ), 0, NULL, FP_MIXIN,

                                     // IReflowField::kFnResetSimulation, _T("ResetSimulation"), 0, TYPE_VOID, 0, 0,
                                     // IReflowField::kFnAdvanceSimulation, _T("AdvanceSimulation"), 0, TYPE_VOID, 0, 0,
                                     IReflowField::kFnUpdate, _T("UpdateToTime"), 0, TYPE_VOID, 0, 1, _T("Time"), 0,
                                     TYPE_TIMEVALUE, IReflowField::kFnEvaluateVelocity, _T("EvaluateVelocity"), 0,
                                     TYPE_POINT3_BV, 0, 1, _T("Position"), 0, TYPE_POINT3,

                                     properties, IReflowField::kFnGetVelocityScale, IReflowField::kFnSetVelocityScale,
                                     _T("VelocityScale"), 0, TYPE_FLOAT,

                                     p_end );
}

IReflowField::~IReflowField() {}

FPInterfaceDesc* IReflowField::GetDesc() { return &theIReflowFieldDesc; }

class max_field_impl : public field {
  public:
    max_field_impl( INode* pNode );

    virtual ~max_field_impl();

    virtual void update( const time_interface& updateTime );

  private:
    void update_impl();

  private:
    TimeValue m_simTime;

    AnimHandle m_sourceHandle;
};

FPInterface* CreateReflowField( INode* pNode ) {
    field_interface_ptr pImpl;

    pImpl = boost::make_shared<max_field_impl>( pNode );

    return new ReflowField( pImpl );
}

FPInterface* CreateDummyReflowField() { return new ReflowField( boost::make_shared<dummy_field>() ); }

ReflowField::ReflowField( field_interface_ptr pImpl ) { m_impl = pImpl; }

ReflowField::~ReflowField() {}

field_interface_ptr ReflowField::GetImpl() { return m_impl; }

float ReflowField::GetVelocityScale() { return m_impl->get_velocity_scale(); }

void ReflowField::SetVelocityScale( float newScale ) { m_impl->set_velocity_scale( newScale ); }

void ReflowField::Update( TimeValue t ) {
    MaxTime timeWrapper( t );

    try {
        m_impl->update( timeWrapper );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

Point3 ReflowField::EvaluateVelocity( const Point3& p ) {
    return frantic::max3d::to_max_t( m_impl->evaluate_velocity( frantic::max3d::from_max_t( p ) ) );
}

max_field_impl::max_field_impl( INode* pNode ) {
    m_sourceHandle = Animatable::GetHandleByAnim( pNode );
    m_simTime =
        GetCOREInterface()->GetAnimRange().Start(); // Reasonable value for initializing the current time. Probably also
                                                    // reasonable to use GetCOREInterface()->GetTime()

    this->update_impl();
}

max_field_impl::~max_field_impl() {}

void max_field_impl::update( const time_interface& updateTime ) {
    m_simTime = to_ticks( updateTime );

    this->update_impl();
}

void max_field_impl::update_impl() {
    Animatable* pAnim = Animatable::GetAnimByHandle( m_sourceHandle );
    if( !pAnim || !pAnim->IsRefMaker() )
        throw MAXException( _T("Field source is invalid"), 0 );

    INode* pNode = static_cast<INode*>( pAnim->GetInterface( INODE_INTERFACE ) );
    if( !pNode )
        throw MAXException( _T("Field source is not a scene node"), 0 );

    boost::shared_ptr<frantic::volumetrics::field_interface> pNewField;

    Interval valid = FOREVER; // TODO: Use this.

#if defined( FUMEFX_SDK_AVAILABLE )
    if( frantic::max3d::volumetrics::is_fumefx_node( pNode, m_simTime ) ) {
        pNewField = frantic::max3d::volumetrics::get_fumefx_field(
            pNode, m_simTime, frantic::max3d::volumetrics::fumefx_channels::velocity );
        // std::unique_ptr<frantic::max3d::volumetrics::fumefx_field_interface> fumeField =
        // frantic::max3d::volumetrics::get_fumefx_field( pNode, m_simTime );

        // m_cache.first.reset( fumeField.release() );
        // m_cache.second.SetInstant( t );
    } else
#endif
#if defined( PHOENIX_SDK_AVAILABLE )
        if( frantic::max3d::volumetrics::is_phoenix_node( pNode, m_simTime ) ) {
        pNewField = frantic::max3d::volumetrics::get_phoenix_field( pNode, m_simTime );
        // m_cache.first = frantic::max3d::volumetrics::get_phoenix_field( pNode, m_simTime );
        // m_cache.second.SetInstant( t );
        //}else if( ember::max3d::IEmberField* ember = frantic::max3d::volumetrics::GetEmberFieldInterface(
        //pNode->GetObjectRef()->FindBaseObject() ) ){ 	pNewField = ember->create_field( pNode, m_simTime );
        //	//m_cache.first = ember->create_field( pNode, m_simTime );
        //	//m_cache.second.SetInstant( t );
    } else
#endif
        if( frantic::max3d::volumetrics::is_forcefield_node( pNode, m_simTime ) ) {
        pNewField = frantic::max3d::volumetrics::get_force_field_adapter( pNode, m_simTime );
        // m_cache.first = frantic::max3d::volumetrics::get_force_field_adapter( pNode, m_simTime );
        // m_cache.second.SetInstant( m_simTime );
    } else if( boost::shared_ptr<frantic::volumetrics::field_interface> pField =
                   frantic::max3d::volumetrics::create_field( pNode, m_simTime, valid ) ) {
        // If the resulting field does not have a (valid) Velocity channel,
        const frantic::channels::channel_map& map = pField->get_channel_map();

        if( !map.has_channel( _T("Velocity") ) ) {
            pField = boost::make_shared<ember::constant_field<ember::vec3>>( ember::vec3( 0 ), _T("Velocity") );
        } else {
            const frantic::channels::channel& ch = map[_T("Velocity")];

            if( ch.arity() != 3 || !frantic::channels::is_channel_data_type_float( ch.data_type() ) ) {
                FF_LOG( warning ) << _T("Node: \"") << ( pNode ? pNode->GetName() : _T("<null>") )
                                  << _T("\" does not have a valid Velocity channel") << std::endl;

                pField = boost::make_shared<ember::constant_field<ember::vec3>>( ember::vec3( 0 ), _T("Velocity") );
            }
        }

        pNewField = pField;
    }

    if( !pNewField )
        throw MAXException( _T("Field source is not a known type"), 0 );

    this->set_field( pNewField );
}

class max_particle_field_impl : public particle_splat_field {
  public:
    max_particle_field_impl( INode* pNode, float spacing, int ( *pBounds )[6], int boundsPadding,
                             bool removeDivergence );

    virtual ~max_particle_field_impl();

    virtual void update( const time_interface& updateTime );

    // virtual frantic::particles::particle_istream_ptr reset_simulation_impl();

    // virtual frantic::particles::particle_istream_ptr advance_simulation_impl();

  private:
    frantic::particles::particle_istream_ptr create_stream();

  private:
    TimeValue m_simTime;

    AnimHandle m_sourceHandle;
};

max_particle_field_impl::max_particle_field_impl( INode* pNode, float spacing, int ( *pBounds )[6], int boundsPadding,
                                                  bool removeDivergence )
    : particle_splat_field( spacing, pBounds, boundsPadding, removeDivergence ) {
    m_sourceHandle = Animatable::GetHandleByAnim( pNode );
    m_simTime =
        GetCOREInterface()->GetAnimRange().Start(); // Reasonable value for initializing the current time. Probably also
                                                    // reasonable to use GetCOREInterface()->GetTime()

    // NOTE: This call here prevents this class from being further subclassed because this constructor must be the
    // most-derived in order to call a virtual function.
    this->set_particles( this->create_stream() );
}

max_particle_field_impl::~max_particle_field_impl() {}

void max_particle_field_impl::update( const time_interface& updateTime ) {
    m_simTime = to_ticks( updateTime );

    this->set_particles( this->create_stream() );
}

frantic::particles::particle_istream_ptr max_particle_field_impl::create_stream() {
    Animatable* pAnim = Animatable::GetAnimByHandle( m_sourceHandle );
    if( !pAnim || !pAnim->IsRefMaker() )
        throw MAXException( _T("Field source is invalid"), 0 );

    INode* pNode = static_cast<INode*>( pAnim->GetInterface( INODE_INTERFACE ) );
    if( !pNode )
        throw MAXException( _T("Field source is not a scene node"), 0 );

    frantic::max3d::particles::IMaxKrakatoaPRTEvalContextPtr pEvalContext =
        frantic::max3d::particles::CreateMaxKrakatoaPRTEvalContext( m_simTime, Class_ID( 0, 0 ) );

    frantic::particles::particle_istream_ptr pStream;

    try {
        pStream = frantic::max3d::particles::max_particle_istream_factory( pNode, pEvalContext );
    } catch( const std::exception& e ) {
        FF_LOG( error ) << e.what() << std::endl;

        // TODO: This should be a more specific exception when the node isn't a particle node!
        throw MAXException( _T("Field source is not a valid particle source"), 0 );
    }

    // This stream is useless if we don't have a valid velocity channel.
    if( !pStream->get_native_channel_map().has_channel( _T("Velocity") ) )
        pStream.reset( new frantic::particles::streams::empty_particle_istream( pStream->get_native_channel_map() ) );

    return pStream;
}

FPInterface* CreateParticleVelocityField( INode* pNode, float spacing, int ( *pBounds )[6], int boundsPadding,
                                          bool removeDivergence ) {
    boost::shared_ptr<max_particle_field_impl> pImpl =
        boost::make_shared<max_particle_field_impl>( pNode, spacing, pBounds, boundsPadding, removeDivergence );

    return new ReflowField( boost::static_pointer_cast<field_interface>( pImpl ) );
}

FPInterface* CreateAdditiveVelocityField( const Tab<IReflowField*>* pFields ) {
    boost::shared_ptr<field_interface> pImpl;

    if( pFields->Count() > 0 ) {
        std::vector<field_interface_ptr> implFields;

        for( IReflowField **it = pFields->Addr( 0 ), **itEnd = pFields->Addr( 0 ) + pFields->Count(); it != itEnd;
             ++it )
            implFields.push_back( ( *it )->GetImpl() );

        pImpl = boost::make_shared<additive_velocity_field>( implFields.begin(), implFields.end() );
    } else {
        pImpl = boost::make_shared<dummy_field>();
    }

    return new ReflowField( pImpl );
}

FPInterface* CreateAdditiveVelocityFieldMXS( const Tab<FPInterface*>* pFields ) {
    boost::shared_ptr<field_interface> pImpl;

    if( pFields->Count() > 0 ) {
        std::vector<field_interface_ptr> implFields;

        for( FPInterface **it = pFields->Addr( 0 ), **itEnd = pFields->Addr( 0 ) + pFields->Count(); it != itEnd;
             ++it ) {
            IReflowField* pField = static_cast<IReflowField*>( ( *it )->GetInterface( ReflowField_INTERFACE ) );

            if( !pField )
                throw MAXException( _T("Parameter must be a ReflowField"), 0 );

            implFields.push_back( pField->GetImpl() );
        }

        pImpl = boost::make_shared<additive_velocity_field>( implFields.begin(), implFields.end() );
    } else {
        pImpl = boost::make_shared<dummy_field>();
    }

    return new ReflowField( pImpl );
}

} // namespace max3d
} // namespace stoke

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/EmberPipeObject.hpp>

#include <ember/advection.hpp>
#include <ember/concatenated_field.hpp>
#include <ember/staggered_grid.hpp>

#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>

#define EmberFluidMotionMod_CLASSID Class_ID( 0x5b4f36e3, 0x3afe2819 )
#define EmberFluidMotionMod_NAME "StokeFieldFluidMod"
#define EmberFluidMotionMod_DISPLAYNAME "Stoke Field Fluid Motion"
#define EmberFluidMotionMod_VERSION 1

namespace ember {
namespace max3d {

enum { kTimestep, kOverrideSpacing, kDefaultSpacing, kInputChannel };

class EmberFluidMotionMod : public frantic::max3d::GenericReferenceTarget<OSModifier, EmberFluidMotionMod> {
  public:
    EmberFluidMotionMod( BOOL loading );

    // Reference Maker
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

    // BaseObject
    virtual CreateMouseCallBack* GetCreateMouseCallBack( void );

#if MAX_VERSION_MAJOR < 24
    virtual TYPE_STRING_TYPE GetObjectName();
#else
    virtual TYPE_STRING_TYPE GetObjectName( bool localized ) const;
#endif

    // Modifier
    virtual Interval LocalValidity( TimeValue t );
    virtual ChannelMask ChannelsUsed();
    virtual ChannelMask ChannelsChanged();
    virtual Class_ID InputType();
    virtual void ModifyObject( TimeValue t, ModContext& mc, ObjectState* os, INode* node );

  protected:
    virtual ClassDesc2* GetClassDesc();

  private:
    float get_timestep() const { return frantic::max3d::get<float>( m_pblock, kTimestep ); }
    bool get_override_spacing() const { return frantic::max3d::get<bool>( m_pblock, kOverrideSpacing ); }
    float get_default_spacing( TimeValue t ) const {
        return frantic::max3d::get<float>( m_pblock, kDefaultSpacing, t );
    }
    const MCHAR* get_input_channel() const { return frantic::max3d::get<const MCHAR*>( m_pblock, kInputChannel ); }
};

class EmberFluidMotionModDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_paramDesc;

    friend class EmberFluidMotionMod;

  public:
    EmberFluidMotionModDesc();

    int IsPublic() { return FALSE; }
    void* Create( BOOL loading ) { return new EmberFluidMotionMod( loading ); }
    const TCHAR* ClassName() { return _T( EmberFluidMotionMod_DISPLAYNAME ); }
    SClass_ID SuperClassID() { return OSM_CLASS_ID; }
    Class_ID ClassID() { return EmberFluidMotionMod_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() {
        return _T( EmberFluidMotionMod_NAME );
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( EmberFluidMotionMod_DISPLAYNAME ); }
#endif
};

enum { kMainPBlock };

EmberFluidMotionModDesc::EmberFluidMotionModDesc()
    : m_paramDesc( kMainPBlock, _T("Parameters"), 0, EmberFluidMotionMod_VERSION ) {
    m_paramDesc.OwnerRefNum( 0 );
    m_paramDesc.OwnerClassDesc( this );

    m_paramDesc.Parameter<float>( kTimestep, _T("Timestep"), 0 ).DefaultValue( 1.f / 30.f ).Range( 0.f, FLT_MAX );

    m_paramDesc.Parameter<bool>( kOverrideSpacing, _T("OverrideSpacing"), 0 ).DefaultValue( false );

    m_paramDesc.Parameter<float>( kDefaultSpacing, _T("DefaultSpacing"), 0, true )
        .DefaultValue( 1.f )
        .Range( 1e-4f, FLT_MAX );

    m_paramDesc.Parameter<const MCHAR*>( kInputChannel, _T("InputChannel"), 0 ).DefaultValue( _M( "Velocity" ) );
}

ClassDesc2* GetEmberFluidMotionModDesc() {
    static EmberFluidMotionModDesc theEmberFluidMotionModDesc;
    return &theEmberFluidMotionModDesc;
}

EmberFluidMotionMod::EmberFluidMotionMod( BOOL /*loading*/ ) {
    GetEmberFluidMotionModDesc()->MakeAutoParamBlocks( this );
}

RefResult EmberFluidMotionMod::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& partID,
                                                 RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        if( message == REFMSG_CHANGE )
            return REF_SUCCEED;
    }
    return REF_DONTCARE;
}

CreateMouseCallBack* EmberFluidMotionMod::GetCreateMouseCallBack( void ) { return NULL; }

#if MAX_VERSION_MAJOR < 24
TYPE_STRING_TYPE EmberFluidMotionMod::GetObjectName() { return _T( EmberFluidMotionMod_DISPLAYNAME ); }
#else
TYPE_STRING_TYPE EmberFluidMotionMod::GetObjectName( bool localized ) const {
    return _T( EmberFluidMotionMod_DISPLAYNAME );
}
#endif

Interval EmberFluidMotionMod::LocalValidity( TimeValue /*t*/ ) { return FOREVER; }

ChannelMask EmberFluidMotionMod::ChannelsUsed() { return GEOM_CHANNEL; }

ChannelMask EmberFluidMotionMod::ChannelsChanged() { return GEOM_CHANNEL; }

Class_ID EmberFluidMotionMod::InputType() { return EmberPipeObject_CLASSID; }

class do_fluid_motion_functor {
    frantic::tstring m_channelName;
    boost::array<int, 6> m_voxelBounds;
    float m_spacing;
    float m_timestep;

  public:
    do_fluid_motion_functor( const frantic::tstring& channelName, const boost::array<int, 6>& voxelBounds,
                             float spacing, float timestep )
        : m_channelName( channelName )
        , m_voxelBounds( voxelBounds )
        , m_spacing( spacing )
        , m_timestep( timestep ) {}

    do_fluid_motion_functor( BOOST_RV_REF( do_fluid_motion_functor ) rhs )
        : m_channelName( boost::move( rhs.m_channelName ) )
        , m_voxelBounds( boost::move( rhs.m_voxelBounds ) )
        , m_spacing( rhs.m_spacing )
        , m_timestep( rhs.m_timestep ) {}

    do_fluid_motion_functor& operator=( BOOST_RV_REF( do_fluid_motion_functor ) rhs ) {
        m_channelName = boost::move( rhs.m_channelName );
        m_voxelBounds = boost::move( rhs.m_voxelBounds );
        m_spacing = rhs.m_spacing;
        m_timestep = rhs.m_timestep;
        return *this;
    }

    boost::shared_ptr<frantic::volumetrics::field_interface>
    operator()( const boost::shared_ptr<frantic::volumetrics::field_interface>& pInput,
                frantic::logging::progress_logger& progress ) const {
        assert( pInput->get_channel_map().has_channel(
            m_channelName ) ); // We messed up if the result doesn't have the channel we picked.

        boost::shared_ptr<frantic::volumetrics::field_interface> result;

        if( boost::shared_ptr<ember::concatenated_field> pFields =
                boost::dynamic_pointer_cast<ember::concatenated_field>( pInput ) ) {
            boost::shared_ptr<ember::concatenated_field> pNewFields = boost::make_shared<ember::concatenated_field>();

            for( std::size_t i = 0, iEnd = pFields->get_num_fields(); i < iEnd; ++i ) {
                boost::shared_ptr<frantic::volumetrics::field_interface> pField = pFields->get_field( i );
                if( pField->get_channel_map().has_channel( m_channelName ) ) {
                    boost::shared_ptr<ember::staggered_discretized_field> grid( new ember::staggered_discretized_field(
                        *reinterpret_cast<const int( * )[6]>( m_voxelBounds.data() ), m_spacing, m_channelName ) );

                    progress.check_for_abort();

                    grid->assign( *pInput, m_channelName, &progress );

                    progress.check_for_abort();

                    ember::do_poisson_solve( grid->get_grid(), m_timestep, "DDDDDD", &progress );

                    pNewFields->add_field( grid );
                } else {
                    pNewFields->add_field( pField );
                }
            }

            result = pNewFields;
        } else {
            // TODO: We are dropping other channels here. Fix that.
            boost::shared_ptr<ember::staggered_discretized_field> grid( new ember::staggered_discretized_field(
                *reinterpret_cast<const int( * )[6]>( m_voxelBounds.data() ), m_spacing, m_channelName ) );

            progress.check_for_abort();

            grid->assign( *pInput, m_channelName, &progress );

            progress.check_for_abort();

            ember::do_poisson_solve( grid->get_grid(), m_timestep, "DDDDDD", &progress );

            result = grid;
        }

        return result;
    }

  private:
    BOOST_MOVABLE_BUT_NOT_COPYABLE( do_fluid_motion_functor )
};

namespace {
class MyLocalModData : public LocalModData {
    boost::weak_ptr<future_field_base> m_trackedResult;

  public:
    virtual ~MyLocalModData() {}

    virtual LocalModData* Clone() { return new MyLocalModData( *this ); }

    void SetResult( const boost::weak_ptr<future_field_base>& pResult ) { m_trackedResult = pResult; }

    boost::shared_ptr<future_field_base> GetResult() { return m_trackedResult.lock(); }
};
} // namespace

void EmberFluidMotionMod::ModifyObject( TimeValue t, ModContext& mc, ObjectState* os, INode* /*node*/ ) {
    if( !os || !os->obj || os->obj->ClassID() != EmberPipeObject_CLASSID )
        return;

    if( !mc.localData ) {
        mc.localData = new MyLocalModData;
    } else {
        if( boost::shared_ptr<future_field_base> pPrevResult =
                static_cast<MyLocalModData*>( mc.localData )->GetResult() ) {
            pPrevResult->cancel();
            FF_LOG( debug ) << _T("Cancelled previous EmberFluidMotionMod evaluation due to re-eval") << std::endl;
        }
    }

    EmberPipeObject* pEmberObj = static_cast<EmberPipeObject*>( os->obj );

    Interval valid = FOREVER;

    Box3 bounds = pEmberObj->GetBounds();
    float spacing = pEmberObj->GetDefaultSpacing();
    float timeStep = 0.f;

    m_pblock->GetValue( kTimestep, t, timeStep, valid );

    if( spacing != spacing || this->get_override_spacing() )
        spacing = this->get_default_spacing( t );

    // int boundInts[] = {
    boost::array<int, 6> boundInts = {
        static_cast<int>( std::floor( bounds.pmin.x / spacing - 0.5f ) ),
        static_cast<int>( std::ceil( bounds.pmax.x / spacing - 0.5f ) ) + 1,
        static_cast<int>( std::floor( bounds.pmin.y / spacing - 0.5f ) ),
        static_cast<int>( std::ceil( bounds.pmax.y / spacing - 0.5f ) ) + 1,
        static_cast<int>( std::floor( bounds.pmin.z / spacing - 0.5f ) ),
        static_cast<int>( std::ceil( bounds.pmax.z / spacing - 0.5f ) ) + 1,
    };

    frantic::tstring channelName = this->get_input_channel();
    if( channelName.empty() )
        channelName = pEmberObj->GetChannels()[0].name();
    else if( !pEmberObj->GetChannels().has_channel( channelName ) )
        return;

    do_fluid_motion_functor op( channelName, boundInts, spacing, timeStep );

    future_field_base::ptr_type pResult = pEmberObj->GetFuture()->then( boost::move( op ) );

    // Record the previous result so if this modcontext is re-evaluated we can delete the old one.
    static_cast<MyLocalModData*>( mc.localData )->SetResult( boost::weak_ptr<future_field_base>( pResult ) );

    pEmberObj->Set( pResult, pEmberObj->GetChannels(), bounds, spacing );
    pEmberObj->UpdateValidity( GEOM_CHAN_NUM, valid );
}

ClassDesc2* EmberFluidMotionMod::GetClassDesc() { return GetEmberFluidMotionModDesc(); }

} // namespace max3d
} // namespace ember

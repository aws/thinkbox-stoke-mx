// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource.h"

#include <stoke/max3d/EmberPipeObject.hpp>

#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>

#define EmberViewportMod_CLASSID Class_ID( 0x45786d70, 0x11b168a3 )
#define EmberViewportMod_NAME "StokeFieldDisplayMod"
#define EmberViewportMod_DISPLAYNAME "Stoke Field Display"
#define EmberViewportMod_VERSION 1

namespace ember {
namespace max3d {

enum {
    kViewportDisplayMode,
    kViewportScalarChannel,
    kViewportVectorChannel,
    kViewportColorChannel,
    kViewportVectorScale,
    kViewportVectorNormalize,
    kResolution,
    kSpacing,
    kSpacingMode,
    kViewportScalarMin,
    kViewportScalarMax,
    kUseViewportScalarMin,
    kUseViewportScalarMax,
    kViewportReduce,
    kViewportPointSize,
    kUseViewportPointSize
};

class EmberViewportMod : public frantic::max3d::GenericReferenceTarget<OSModifier, EmberViewportMod> {
  public:
    EmberViewportMod( BOOL loading );

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
    int get_length_resolution( TimeValue t ) { return frantic::max3d::get<int>( m_pblock, kResolution, t, 0 ); }
    int get_width_resolution( TimeValue t ) { return frantic::max3d::get<int>( m_pblock, kResolution, t, 1 ); }
    int get_height_resolution( TimeValue t ) { return frantic::max3d::get<int>( m_pblock, kResolution, t, 2 ); }
    EmberPipeObject::display_mode::option get_viewport_display_mode() {
        return static_cast<EmberPipeObject::display_mode::option>(
            frantic::max3d::get<int>( m_pblock, kViewportDisplayMode ) );
    }
    const MCHAR* get_viewport_scalar_channel() {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kViewportScalarChannel );
    }
    const MCHAR* get_viewport_vector_channel() {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kViewportVectorChannel );
    }
    const MCHAR* get_viewport_color_channel() {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kViewportColorChannel );
    }
    bool get_viewport_vector_normalize() { return frantic::max3d::get<bool>( m_pblock, kViewportVectorNormalize ); }
    float get_viewport_vector_scale() { return frantic::max3d::get<float>( m_pblock, kViewportVectorScale ); }
    EmberPipeObject::spacing_mode::option get_viewport_spacing_mode() {
        return static_cast<EmberPipeObject::spacing_mode::option>( frantic::max3d::get<int>( m_pblock, kSpacingMode ) );
    }
    float get_length_spacing( TimeValue t ) { return frantic::max3d::get<float>( m_pblock, kSpacing, t, 0 ); }
    float get_width_spacing( TimeValue t ) { return frantic::max3d::get<float>( m_pblock, kSpacing, t, 1 ); }
    float get_height_spacing( TimeValue t ) { return frantic::max3d::get<float>( m_pblock, kSpacing, t, 2 ); }
    int get_viewport_reduce() { return std::max( 0, frantic::max3d::get<int>( m_pblock, kViewportReduce ) ); }
    bool get_viewport_point_size_enabled() { return frantic::max3d::get<bool>( m_pblock, kUseViewportPointSize ); }
    float get_viewport_point_size() { return frantic::max3d::get<float>( m_pblock, kViewportPointSize ); }
};

class EmberViewportModDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_paramDesc;

  public:
    EmberViewportModDesc();

    int IsPublic() { return FALSE; }
    void* Create( BOOL loading ) { return new EmberViewportMod( loading ); }
    const TCHAR* ClassName() { return _T( EmberViewportMod_DISPLAYNAME ); }
    SClass_ID SuperClassID() { return OSM_CLASS_ID; }
    Class_ID ClassID() { return EmberViewportMod_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() {
        return _T( EmberViewportMod_NAME );
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( EmberViewportMod_DISPLAYNAME ); }
#endif
};

enum { kMainPBlock };

EmberViewportModDesc::EmberViewportModDesc()
    : m_paramDesc( kMainPBlock, _T("Parameters"), 0, EmberViewportMod_VERSION ) {
    m_paramDesc.OwnerRefNum( 0 );
    m_paramDesc.OwnerClassDesc( this );

    m_paramDesc.Parameter<int[3]>( kResolution, _T("Resolution") )
        .EnableAnimation( true, IDS_RESOLUTION )
        .DefaultValue( 10 )
        .Range( 1, INT_MAX );

    m_paramDesc.Parameter<int>( kViewportDisplayMode, _T("ViewportDisplayMode") )
        .DefaultValue( EmberPipeObject::display_mode::kDisplayDots );

    m_paramDesc.Parameter<const MCHAR*>( kViewportScalarChannel, _T("ViewportScalarChannel") ).DefaultValue( _M( "" ) );

    m_paramDesc.Parameter<const MCHAR*>( kViewportVectorChannel, _T("ViewportVectorChannel") ).DefaultValue( _M( "" ) );

    m_paramDesc.Parameter<const MCHAR*>( kViewportColorChannel, _T("ViewportColorChannel") ).DefaultValue( _M( "" ) );

    m_paramDesc.Parameter<bool>( kViewportVectorNormalize, _T("ViewportVectorNormalize") ).DefaultValue( false );

    m_paramDesc.Parameter<float>( kViewportVectorScale, _T("ViewportVectorScale") )
        .EnableAnimation( true, IDS_VIEWPORTVECTORSCALE )
        .DefaultValue( 1.f );

    m_paramDesc.Parameter<int>( kSpacingMode, _T("SpacingMode") )
        .DefaultValue( EmberPipeObject::spacing_mode::kSpacingConstant );

    m_paramDesc.Parameter<float[3]>( kSpacing, _T("Spacing") )
        .EnableAnimation( true, IDS_SPACING )
        .DefaultValue( 1.f )
        .Range( 1e-4f, FLT_MAX );

    m_paramDesc.Parameter<bool>( kUseViewportScalarMin, _T("UseViewportScalarMin") ).DefaultValue( false );
    m_paramDesc.Parameter<bool>( kUseViewportScalarMax, _T("UseViewportScalarMax") ).DefaultValue( false );
    m_paramDesc.Parameter<float>( kViewportScalarMin, _T("ViewportScalarMin") )
        .DefaultValue( 0.f )
        .Range( -FLT_MAX, FLT_MAX );
    m_paramDesc.Parameter<float>( kViewportScalarMax, _T("ViewportScalarMax") )
        .DefaultValue( 100.f )
        .Range( -FLT_MAX, FLT_MAX );

    m_paramDesc.Parameter<int>( kViewportReduce, _T("ViewportReduce") ).DefaultValue( 1 ).Range( 0, INT_MAX );

    m_paramDesc.Parameter<bool>( kUseViewportPointSize, _T("UseViewportPointSize") ).DefaultValue( false );
    m_paramDesc.Parameter<float>( kViewportPointSize, _T("ViewportPointSize") )
        .DefaultValue( 5.f )
        .Range( 0.f, FLT_MAX );
}

ClassDesc2* GetEmberViewportModDesc() {
    static EmberViewportModDesc theEmberViewportModDesc;
    return &theEmberViewportModDesc;
}

EmberViewportMod::EmberViewportMod( BOOL /*loading*/ ) { GetEmberViewportModDesc()->MakeAutoParamBlocks( this ); }

RefResult EmberViewportMod::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& partID,
                                              RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        if( message == REFMSG_CHANGE )
            return REF_SUCCEED;
    }
    return REF_DONTCARE;
}

CreateMouseCallBack* EmberViewportMod::GetCreateMouseCallBack( void ) { return NULL; }

#if MAX_VERSION_MAJOR < 24
TYPE_STRING_TYPE EmberViewportMod::GetObjectName() { return _T( EmberViewportMod_DISPLAYNAME ); }
#else
TYPE_STRING_TYPE EmberViewportMod::GetObjectName( bool localized ) const { return _T( EmberViewportMod_DISPLAYNAME ); }
#endif

Interval EmberViewportMod::LocalValidity( TimeValue /*t*/ ) { return FOREVER; }

ChannelMask EmberViewportMod::ChannelsUsed() { return DISP_ATTRIB_CHANNEL; }

ChannelMask EmberViewportMod::ChannelsChanged() { return DISP_ATTRIB_CHANNEL; }

Class_ID EmberViewportMod::InputType() { return EmberPipeObject_CLASSID; }

void EmberViewportMod::ModifyObject( TimeValue t, ModContext& /*mc*/, ObjectState* os, INode* /*node*/ ) {
    if( !os || !os->obj || os->obj->ClassID() != EmberPipeObject_CLASSID )
        return;

    EmberPipeObject* pEmberObj = static_cast<EmberPipeObject*>( os->obj );

    switch( this->get_viewport_spacing_mode() ) {
    case EmberPipeObject::spacing_mode::kSpacingConstant:
        pEmberObj->SetViewportResolution( this->get_length_resolution( t ), this->get_width_resolution( t ),
                                          this->get_height_resolution( t ) );
        break;
    case EmberPipeObject::spacing_mode::kSpacingDynamic:
        pEmberObj->SetViewportSpacing( this->get_length_spacing( t ), this->get_width_spacing( t ),
                                       this->get_height_spacing( t ) );
        break;
    case EmberPipeObject::spacing_mode::kSpacingNatural:
        pEmberObj->SetViewportSpacingNatural( this->get_viewport_reduce() );
        break;
    }

    const MCHAR* maskChannel = this->get_viewport_scalar_channel();
    const MCHAR* vectorChannel = this->get_viewport_vector_channel();
    const MCHAR* colorChannel = this->get_viewport_color_channel();

    pEmberObj->SetViewportScalarChannel( maskChannel ? maskChannel : _T("") );
    pEmberObj->SetViewportVectorChannel( vectorChannel ? vectorChannel : _T("") );
    pEmberObj->SetViewportColorChannel( colorChannel ? colorChannel : _T("") );

    float scalarMin = m_pblock->GetFloat( kViewportScalarMin );
    float scalarMax = m_pblock->GetFloat( kViewportScalarMax );
    float pointSize = this->get_viewport_point_size();

    bool useMin = m_pblock->GetInt( kUseViewportScalarMin ) != FALSE;
    if( !useMin )
        scalarMin = std::numeric_limits<float>::quiet_NaN();

    bool useMax = m_pblock->GetInt( kUseViewportScalarMax ) != FALSE;
    if( !useMax )
        scalarMax = std::numeric_limits<float>::quiet_NaN();

    bool usePointSize = get_viewport_point_size_enabled();
    if( !usePointSize )
        pointSize = std::numeric_limits<float>::quiet_NaN();

    switch( this->get_viewport_display_mode() ) {
    case EmberPipeObject::display_mode::kDisplayDots:
        pEmberObj->SetViewportDisplayDots( scalarMin, scalarMax, pointSize );
        break;
    case EmberPipeObject::display_mode::kDisplayLines: {
        bool normalize = this->get_viewport_vector_normalize();
        float scale = this->get_viewport_vector_scale();

        pEmberObj->SetViewportDisplayDots( scalarMin, scalarMax, pointSize );
        pEmberObj->SetViewportDisplayLines( normalize, scale );
    } break;
    default:
        pEmberObj->SetViewportDisplayNone();
        break;
    }

    pEmberObj->UpdateValidity( DISP_ATTRIB_CHAN_NUM, FOREVER );
}

ClassDesc2* EmberViewportMod::GetClassDesc() { return GetEmberViewportModDesc(); }

} // namespace max3d
} // namespace ember

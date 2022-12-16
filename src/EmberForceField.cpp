// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource_ember.h"

#include <frantic/max3d/GenericReferenceTarget.hpp>
#include <frantic/max3d/convert.hpp>
#include <frantic/max3d/max_utility.hpp>
#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>
#include <frantic/max3d/volumetrics/IEmberField.hpp>

#include <frantic/volumetrics/field_interface.hpp>

#pragma warning( push, 3 )
#include <object.h>
#include <simpmod.h>
#pragma warning( pop )
//#include <ParticleFlow/OneClickCreateCallBack.h> //No particular reason this should be in ParticleFlow.

#if MAX_VERSION_MAJOR >= 17
#include <frantic/max3d/viewport/DecoratedMeshEdgeRenderItem.hpp>

#pragma warning( push, 3 )
#include <Graphics\CustomRenderItemHandle.h>
#include <Graphics\IDisplayManager.h>
#pragma warning( pop )
#endif

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#if MAX_VERSION_MAJOR < 15
#define p_end end
#endif

#define EmberForceObject_CLASSID Class_ID( 0x379d0024, 0x5fbc6760 )
#define EmberForceObject_CLASSNAME "Field Force"
#define EmberForceObject_OBJECTNAME "FieldForce"
#define EmberForceObject_VERSION 1

#define EmberForceMod_CLASSID Class_ID( 0x535451b2, 0x6c2b3136 )
#define EmberForceMod_CLASSNAME "Field Force Mod"
#define EmberForceMod_OBJECTNAME "FieldForceMod"
#define EmberForceMod_VERSION 1

extern HINSTANCE hInstance;

using frantic::volumetrics::field_interface;
typedef boost::shared_ptr<field_interface> field_ptr;

namespace ember {
namespace max3d {

extern const Mesh* GetEmberForceFieldMesh();
#if MAX_VERSION_MAJOR >= 25
extern MaxSDK::SharedMeshPtr GetEmberForceFieldMeshShared();
#endif

class EmberForceObjectDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_pbDesc;

    EmberForceObjectDesc();

  public:
    static EmberForceObjectDesc& GetInstance();

    int IsPublic() { return TRUE; }
    void* Create( BOOL loading );
    const TCHAR* ClassName() { return _T( EmberForceObject_CLASSNAME ); }
    SClass_ID SuperClassID() { return WSM_OBJECT_CLASS_ID; }
    Class_ID ClassID() { return EmberForceObject_CLASSID; }
    const TCHAR* Category() { return _T("Stoke"); }
    const TCHAR* InternalName() {
        return _T( EmberForceObject_OBJECTNAME );
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( EmberForceObject_CLASSNAME ); }
#endif
};

class EmberForceModDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_pbDesc;

  public:
    EmberForceModDesc();

    int IsPublic() { return FALSE; }
    void* Create( BOOL loading );
    const TCHAR* ClassName() { return _T( EmberForceMod_CLASSNAME ); }
    SClass_ID SuperClassID() { return WSM_CLASS_ID; }
    Class_ID ClassID() { return EmberForceMod_CLASSID; }
    const TCHAR* Category() { return _T("Stoke"); }
    const TCHAR* InternalName() {
        return _T( EmberForceMod_OBJECTNAME );
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( EmberForceMod_CLASSNAME ); }
#endif
};

EmberForceModDesc::EmberForceModDesc()
    : m_pbDesc( 0, _T("Parameters"), 0, EmberForceMod_VERSION ) {
    m_pbDesc.OwnerClassDesc( this );
    m_pbDesc.OwnerRefNum( 0 );
}

enum { kEmberNode, kTimeType, kIconSize, kChannelName, kForceScale };

enum time_type { time_ticks, time_seconds };

class EmberForceField;

class EmberForceObject : public frantic::max3d::GenericReferenceTarget<WSMObject, EmberForceObject> {
  protected:
    ClassDesc2* GetClassDesc();

  public:
    EmberForceObject( BOOL loading );
    virtual ~EmberForceObject();

    void UpdateForceField( EmberForceField& theForceField, TimeValue t );

    // From ReferenceMaker
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

    // From BaseObject
#if MAX_VERSION_MAJOR >= 17
    // Max 2015+ uses a new viewport system.
    virtual unsigned long GetObjectDisplayRequirement() const;

    virtual bool PrepareDisplay( const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext );

    virtual bool UpdatePerNodeItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                     MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                     MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer );

    virtual bool UpdatePerViewItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                     MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                     MaxSDK::Graphics::UpdateViewContext& viewContext,
                                     MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer );

#else
    virtual bool RequiresSupportForLegacyDisplayMode() const;
#endif

    virtual CreateMouseCallBack* GetCreateMouseCallBack();
#if MAX_VERSION_MAJOR < 24
    virtual TYPE_STRING_TYPE GetObjectName();
#else
    virtual TYPE_STRING_TYPE GetObjectName( bool localized ) const;
#endif
    virtual int Display( TimeValue t, INode* inode, ViewExp* vpt, int flags );
    virtual int HitTest( TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt );
    virtual void GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* vp, Box3& box );
    virtual void GetLocalBoundBox( TimeValue t, INode* inode, ViewExp* vp, Box3& box );

    // From Object
    virtual int IsRenderable();
    virtual void InitNodeName( MSTR& s );
    virtual ObjectState Eval( TimeValue t );
    virtual Interval ObjectValidity( TimeValue t );

    // From WSMObject
    virtual Modifier* CreateWSMMod( INode* node );

    virtual BOOL SupportsDynamics();
    virtual ForceField* GetForceField( INode* node );
    virtual CollisionObject* GetCollisionObject( INode* node );

  private:
    Interval m_cacheValidity;
    std::vector<frantic::tstring> m_channels;
    boost::shared_ptr<frantic::volumetrics::field_interface> m_field;

#if MAX_VERSION_MAJOR >= 17
    MaxSDK::Graphics::CustomRenderItemHandle m_iconRenderItem;
#endif

  private:
    time_type get_time_type() const { return m_pblock->GetInt( kTimeType ) == time_ticks ? time_ticks : time_seconds; }
    Matrix3 get_icon_transform( TimeValue t ) {
        Matrix3 result;
        float f = m_pblock->GetFloat( kIconSize, t );
        result.SetScale( Point3( f, f, f ) );
        return result;
    }

    friend class EmberForceObjectDlgProc;
    friend class EmberForceMod;

    void get_available_channels( TimeValue t, std::vector<frantic::tstring>& outChannels );
    void cache_field( TimeValue t );
};

EmberForceObjectDesc& EmberForceObjectDesc::GetInstance() {
    static EmberForceObjectDesc theEmberForceObjectDesc;
    return theEmberForceObjectDesc;
}

namespace {
class EmberNodeValidator : public PBValidator {
  public:
    static EmberNodeValidator& GetInstance();

    virtual BOOL Validate( PB2Value& v ) {
        if( !v.r )
            return TRUE;

        if( INode* pNode = static_cast<INode*>( v.r->GetInterface( INODE_INTERFACE ) ) )
            return frantic::max3d::volumetrics::is_field( pNode, GetCOREInterface()->GetTime() ) ? TRUE : FALSE;

        return FALSE;
    }
    // virtual BOOL Validate (PB2Value &v, ReferenceMaker *owner, ParamID id, int tabIndex)
    virtual void DeleteThis() {}
};

EmberNodeValidator& EmberNodeValidator::GetInstance() {
    static EmberNodeValidator theEmberNodeValidator;
    return theEmberNodeValidator;
}
} // namespace

class EmberForceObjectDlgProc : public ParamMap2UserDlgProc {
  public:
    static EmberForceObjectDlgProc& GetInstance();

    virtual INT_PTR DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

    virtual void DeleteThis() {}
};

EmberForceObjectDlgProc& EmberForceObjectDlgProc::GetInstance() {
    static EmberForceObjectDlgProc theFieldTexmapDlgProc;
    return theFieldTexmapDlgProc;
}

#define WM_UPDATE_LIST ( WM_APP + 0xd3d )

INT_PTR EmberForceObjectDlgProc::DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam,
                                          LPARAM lParam ) {
    switch( msg ) {
    case WM_INITDIALOG:
        if( IParamBlock2* pb = map->GetParamBlock() ) {
            if( HWND hWndCombo = GetDlgItem( hWnd, IDC_EMBERFORCE_CHANELNAME_COMBO ) ) {
                ComboBox_SetCueBannerText( hWndCombo, _T("Select a channel ...") );

                if( EmberForceObject* esw = static_cast<EmberForceObject*>( pb->GetOwner() ) ) {
                    std::vector<frantic::tstring> options;

                    esw->get_available_channels( t, options );

                    for( std::vector<frantic::tstring>::const_iterator it = options.begin(), itEnd = options.end();
                         it != itEnd; ++it )
                        ComboBox_AddString( hWndCombo, it->c_str() );
                }

                const MCHAR* curText = pb->GetStr( kChannelName );

                if( curText != NULL && curText[0] != _T( '\0' ) ) {
                    int result = ComboBox_FindStringExact( hWndCombo, -1, curText );
                    if( result != CB_ERR )
                        ComboBox_SetCurSel( hWndCombo, result );

                    ComboBox_SetText( hWndCombo, curText );
                }
            }
        }

        break;
    case WM_COMMAND:
        if( LOWORD( wParam ) == IDC_EMBERFORCE_CHANELNAME_COMBO ) {
            if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                TCHAR curText[64];

                ComboBox_GetText( reinterpret_cast<HWND>( lParam ), curText, 64 );

                if( IParamBlock2* pb = map->GetParamBlock() )
                    pb->SetValue( kChannelName, 0, curText );

                return TRUE;
            }
        } else if( LOWORD( wParam ) == IDC_EMBERFORCE_NODEOPTIONS_BUTTON ) {
            if( HIWORD( wParam ) == BN_CLICKED ) {
                ReferenceTarget* rtarg = NULL;
                if( IParamBlock2* pb = map->GetParamBlock() ) {
                    ReferenceMaker* rmaker = pb->GetOwner();
                    if( rmaker && rmaker->IsRefTarget() )
                        rtarg = static_cast<ReferenceTarget*>( rmaker );
                }

                frantic::max3d::mxs::expression( _T("try(::StokeContextMenuStruct.OpenFieldForceMenu owner)catch()") )
                    .bind( _T("owner"), rtarg )
                    .evaluate<void>();

                return TRUE;
            }
        }
        break;
    case WM_UPDATE_LIST:
        if( HWND hWndCombo = GetDlgItem( hWnd, IDC_EMBERFORCE_CHANELNAME_COMBO ) ) {
            ComboBox_ResetContent( hWndCombo );

            if( IParamBlock2* pb = map->GetParamBlock() ) {
                if( EmberForceObject* esw = static_cast<EmberForceObject*>( pb->GetOwner() ) ) {
                    std::vector<frantic::tstring> options;

                    esw->get_available_channels( t, options );

                    for( std::vector<frantic::tstring>::const_iterator it = options.begin(), itEnd = options.end();
                         it != itEnd; ++it )
                        ComboBox_AddString( hWndCombo, it->c_str() );
                }

                const MCHAR* curText = pb->GetStr( kChannelName );

                if( curText != NULL && curText[0] != _T( '\0' ) ) {
                    int result = ComboBox_FindStringExact( hWndCombo, -1, curText );
                    if( result != CB_ERR )
                        ComboBox_SetCurSel( hWndCombo, result );

                    ComboBox_SetText( hWndCombo, curText );
                }
            }
        }

        return TRUE;
    }

    return FALSE;
}

EmberForceObjectDesc::EmberForceObjectDesc()
    : m_pbDesc( 0, _T("Parameters"), 0, EmberForceObject_VERSION ) {
    m_pbDesc.OwnerClassDesc( this );
    m_pbDesc.OwnerRefNum( 0 );

    m_pbDesc.RolloutTemplate( 0, IDD_EMBERFORCE, IDS_EMBERFORCE_TITLE, &EmberForceObjectDlgProc::GetInstance() );

    m_pbDesc.Parameter<INode*>( kEmberNode, _T("FieldNode"), 0 )
        .Validator( &EmberNodeValidator::GetInstance() )
        .PickNodeButtonUI( 0, IDC_EMBERFORCE_NODEPICKER_BUTTON, IDS_PRTEMBER_SOURCE_CAPTION );

    m_pbDesc.Parameter<int>( kTimeType, _T("TimeType"), 0 ).DefaultValue( static_cast<int>( time_ticks ) );

    m_pbDesc.Parameter<float>( kIconSize, _T("IconSize"), 0 )
        .DefaultValue( 1.f )
        .Range( 0.f, FLT_MAX )
        .SpinnerUI( 0, EDITTYPE_POS_FLOAT, IDC_EMBERFORCE_ICONSIZE_EDIT, IDC_EMBERFORCE_ICONSIZE_SPIN,
                    boost::optional<float>() );

    m_pbDesc.Parameter<const MCHAR*>( kChannelName, _T("ChannelName"), 0 ).DefaultValue( _T("Force") );

    m_pbDesc.Parameter<float>( kForceScale, _T("ForceScale"), 0 )
        .DefaultValue( 1.f )
        .Range( -FLT_MAX, FLT_MAX )
        .SpinnerUI( 0, EDITTYPE_FLOAT, IDC_EMBERFORCE_SCALE_EDIT, IDC_EMBERFORCE_SCALE_SPIN, boost::optional<float>() );
}

void* EmberForceObjectDesc::Create( BOOL loading ) { return new EmberForceObject( loading ); }

ClassDesc2* EmberForceObject::GetClassDesc() { return &EmberForceObjectDesc::GetInstance(); }

EmberForceObject::EmberForceObject( BOOL /*loading*/ )
    : m_cacheValidity( NEVER ) {
    GetClassDesc()->MakeAutoParamBlocks( this );
}

EmberForceObject::~EmberForceObject() {}

void EmberForceObject::get_available_channels( TimeValue t, std::vector<frantic::tstring>& outChannels ) {
    cache_field( t );

    outChannels = m_channels;
}

void EmberForceObject::cache_field( TimeValue t ) {
    if( m_cacheValidity.InInterval( t ) )
        return;

    m_field.reset();
    m_channels.clear();
    m_cacheValidity.SetInfinite();

    INode* pNode = m_pblock->GetINode( kEmberNode );
    if( pNode ) {
        boost::shared_ptr<frantic::volumetrics::field_interface> field =
            frantic::max3d::volumetrics::create_field( pNode, t, m_cacheValidity );
        if( field ) {
            const frantic::channels::channel_map& fieldMap = field->get_channel_map();

            for( std::size_t i = 0, iEnd = fieldMap.channel_count(); i < iEnd; ++i ) {
                if( fieldMap[i].arity() == 3 &&
                    frantic::channels::is_channel_data_type_float( fieldMap[i].data_type() ) )
                    m_channels.push_back( fieldMap[i].name() );
            }
        }

        m_field = field;
    }
}

RefResult EmberForceObject::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& partID,
                                              RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        int tabIndex;
        ParamID lastParam = m_pblock->LastNotifyParamID( tabIndex );

        if( message == REFMSG_CHANGE ) {
            if( lastParam == kEmberNode ) {
                if( partID & PART_GEOM ) {
                    m_cacheValidity.SetEmpty();

                    if( IParamMap2* pMap = m_pblock->GetMap() )
                        PostMessage( pMap->GetHWnd(), WM_UPDATE_LIST, 0, 0 );
                }
                return REF_STOP;
            }
        }

        // return REF_STOP;
    }
    return REF_SUCCEED;
}

unsigned long EmberForceObject::GetObjectDisplayRequirement() const { return 0; }

bool EmberForceObject::PrepareDisplay( const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext ) {
#if MAX_VERSION_MAJOR >= 25
    MaxSDK::SharedMeshPtr iconMesh = GetEmberForceFieldMeshShared();
#else
    const Mesh* iconMesh = GetEmberForceFieldMesh();
#endif

    MaxSDK::Graphics::Matrix44 iconTM;
    MaxSDK::Graphics::MaxWorldMatrixToMatrix44( iconTM,
                                                this->get_icon_transform( prepareDisplayContext.GetDisplayTime() ) );

#if MAX_VERSION_MAJOR >= 25
    std::unique_ptr<MaxSDK::Graphics::Utilities::MeshEdgeRenderItem> impl(
        new frantic::max3d::viewport::DecoratedMeshEdgeRenderItem( iconMesh, false, iconTM ) );
#else
    std::unique_ptr<MaxSDK::Graphics::Utilities::MeshEdgeRenderItem> impl(
        new frantic::max3d::viewport::DecoratedMeshEdgeRenderItem( const_cast<Mesh*>( iconMesh ), false, false,
                                                                   iconTM ) );
#endif

    m_iconRenderItem.Release();
    m_iconRenderItem.Initialize();
    m_iconRenderItem.SetCustomImplementation( impl.release() );
    m_iconRenderItem.SetVisibilityGroup( MaxSDK::Graphics::RenderItemVisible_Gizmo );

    return true;
}

bool EmberForceObject::UpdatePerNodeItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                           MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                           MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer ) {
    targetRenderItemContainer.ClearAllRenderItems();
    targetRenderItemContainer.AddRenderItem( m_iconRenderItem );

    return true;
}

bool EmberForceObject::UpdatePerViewItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                           MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                           MaxSDK::Graphics::UpdateViewContext& viewContext,
                                           MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer ) {
    return WSMObject::UpdatePerViewItems( updateDisplayContext, nodeContext, viewContext, targetRenderItemContainer );
}

class EmberForceObjectCreateCallBack : public CreateMouseCallBack {
    IParamBlock2* m_pblock;
    ParamID m_paramID;
    IPoint2 sp1;

    EmberForceObjectCreateCallBack() {}

    virtual ~EmberForceObjectCreateCallBack() {}

  public:
    static EmberForceObjectCreateCallBack& GetInstance();

    void SetObject( IParamBlock2* pblock, ParamID paramID );

    virtual int proc( ViewExp* vpt, int msg, int point, int flags, IPoint2 m, Matrix3& mat );
};

EmberForceObjectCreateCallBack& EmberForceObjectCreateCallBack::GetInstance() {
    static EmberForceObjectCreateCallBack theEmberForceObjectCreateCallBack;
    return theEmberForceObjectCreateCallBack;
}

void EmberForceObjectCreateCallBack::SetObject( IParamBlock2* pblock, ParamID paramID ) {
    m_pblock = pblock;
    m_paramID = paramID;
}

int EmberForceObjectCreateCallBack::proc( ViewExp* vpt, int msg, int point, int /*flags*/, IPoint2 m, Matrix3& mat ) {
    if( msg == MOUSE_POINT ) {
        switch( point ) {
        case 0: {
            sp1 = m;

            Point3 p = vpt->SnapPoint( m, m, NULL, SNAP_IN_3D );
            mat.IdentityMatrix();
            mat.SetTrans( p );
        } break;
        case 1:
            return CREATE_STOP;
        }
    } else if( msg == MOUSE_MOVE ) {
        switch( point ) {
        case 1: {
            float dist = vpt->GetCPDisp( Point3( 0, 0, 0 ), Point3( (float)M_SQRT2, (float)M_SQRT2, 0.f ), sp1, m );
            dist = vpt->SnapLength( dist );
            dist = 2.f * fabsf( dist );

            if( m_pblock )
                m_pblock->SetValue( m_paramID, 0, dist );
        } break;
        }
    } else if( msg == MOUSE_ABORT ) {
        return CREATE_ABORT;
    }

    return CREATE_CONTINUE;
}

CreateMouseCallBack* EmberForceObject::GetCreateMouseCallBack() {
    // return NULL;
    // return OneClickCreateCallBack::Instance();
    EmberForceObjectCreateCallBack::GetInstance().SetObject( m_pblock, static_cast<ParamID>( kIconSize ) );
    return &EmberForceObjectCreateCallBack::GetInstance();
}

#if MAX_VERSION_MAJOR < 24
TYPE_STRING_TYPE EmberForceObject::GetObjectName() { return _T( EmberForceObject_OBJECTNAME ); }
#else
TYPE_STRING_TYPE EmberForceObject::GetObjectName( bool localized ) const { return _T( EmberForceObject_OBJECTNAME ); }
#endif

int EmberForceObject::Display( TimeValue t, INode* inode, ViewExp* vpt, int /*flags*/ ) {
#if MAX_VERSION_MAJOR >= 17
    if( MaxSDK::Graphics::IsRetainedModeEnabled() )
        return TRUE;
#endif

    if( inode->IsNodeHidden() || inode->GetBoxMode() )
        return TRUE;

    GraphicsWindow* gw = vpt->getGW();
    DWORD rndLimits = gw->getRndLimits();

    if( gw == NULL || inode == NULL )
        return FALSE;

    const Mesh* iconMesh = GetEmberForceFieldMesh();

    Matrix3 tm;
    tm = get_icon_transform( t );
    tm = tm * inode->GetNodeTM( t );

    gw->setRndLimits( GW_WIREFRAME );
    gw->setTransform( tm );

    frantic::max3d::draw_mesh_wireframe( gw, const_cast<Mesh*>( iconMesh ),
                                         inode->Selected()
                                             ? frantic::graphics::color3f( 1 )
                                             : frantic::graphics::color3f::from_RGBA( inode->GetWireColor() ) );

    gw->setRndLimits( rndLimits );

    return TRUE;
}

int EmberForceObject::HitTest( TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p,
                               ViewExp* vpt ) {
#if MAX_VERSION_MAJOR >= 17
    if( MaxSDK::Graphics::IsHardwareHitTesting( vpt ) )
        return FALSE;
#endif

    GraphicsWindow* gw = vpt->getGW();
    DWORD rndLimits = gw->getRndLimits();

    if( ( ( flags & HIT_SELONLY ) && !inode->Selected() ) || gw == NULL || inode == NULL )
        return FALSE;

    HitRegion hitRegion;
    MakeHitRegion( hitRegion, type, crossing, 4, p );

    gw->setRndLimits( GW_PICK | GW_WIREFRAME );
    gw->setHitRegion( &hitRegion );
    gw->clearHitCode();

    const Mesh* iconMesh = GetEmberForceFieldMesh();

    Matrix3 tm;
    tm = get_icon_transform( t );
    tm = tm * inode->GetNodeTM( t );

    gw->setTransform( tm );
    if( const_cast<Mesh*>( iconMesh )->select( gw, NULL, &hitRegion ) )
        return TRUE;

    gw->setRndLimits( rndLimits );

    return FALSE;
}

void EmberForceObject::GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* /*vp*/, Box3& box ) {
    const Mesh* iconMesh = GetEmberForceFieldMesh();

    box = const_cast<Mesh*>( iconMesh )->getBoundingBox();
    box = box * get_icon_transform( t );
    box = box * inode->GetNodeTM( t );
}

void EmberForceObject::GetLocalBoundBox( TimeValue t, INode* /*inode*/, ViewExp* /*vp*/, Box3& box ) {
    const Mesh* iconMesh = GetEmberForceFieldMesh();

    box = const_cast<Mesh*>( iconMesh )->getBoundingBox();
    box = box * get_icon_transform( t );
}

int EmberForceObject::IsRenderable() { return FALSE; }

void EmberForceObject::InitNodeName( MSTR& s ) { s = _T( EmberForceObject_OBJECTNAME ); }

ObjectState EmberForceObject::Eval( TimeValue t ) { return ObjectState( this ); }

Interval EmberForceObject::ObjectValidity( TimeValue t ) { return Interval( t, t ); }

BOOL EmberForceObject::SupportsDynamics() { return TRUE; }

class EmberForceField : public ForceField {
    AnimHandle m_owner; // Must be a EmberForceObject
    Interval m_validity;

    field_ptr m_field;
    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> m_forceAccessor;
    float m_scale;

  public:
    EmberForceField();
    EmberForceField( EmberForceObject* owner );

    void Reset( EmberForceObject* owner );

    void Update( const field_ptr& field, const frantic::tstring& channelName, float scale, const Interval& validity );

    virtual ~EmberForceField();

    virtual Point3 Force( TimeValue t, const Point3& pos, const Point3& vel, int index );

    // virtual void SetRandSeed(int seed);

    virtual void DeleteThis();

    friend class EmberForceMod;
};

EmberForceField::EmberForceField()
    : m_owner( 0 )
    , m_scale( 1.f ) {}

EmberForceField::EmberForceField( EmberForceObject* owner )
    : m_owner( 0 )
    , m_scale( 1.f ) {
    this->Reset( owner );
}

EmberForceField::~EmberForceField() {}

void EmberForceField::Reset( EmberForceObject* owner ) { m_owner = owner ? Animatable::GetHandleByAnim( owner ) : 0; }

void EmberForceField::Update( const field_ptr& field, const frantic::tstring& channelName, float scale,
                              const Interval& validity ) {
    m_field = field;
    m_scale = scale;

    if( m_field ) {
        m_validity = validity;

        const frantic::channels::channel_map& map = m_field->get_channel_map();

        if( map.has_channel( channelName ) ) {
            const frantic::channels::channel& ch = map[channelName];

            if( ch.arity() == 3 && frantic::channels::is_channel_data_type_float( ch.data_type() ) )
                m_forceAccessor = map.get_cvt_accessor<frantic::graphics::vector3f>( channelName );
        }
    } else {
        m_validity.SetInfinite();
    }
}

Point3 EmberForceField::Force( TimeValue t, const Point3& pos, const Point3& /*vel*/, int /*index*/ ) {
    if( !m_validity.InInterval( t ) ) {
        m_validity.SetEmpty();

        Animatable* ownerAnim = Animatable::GetAnimByHandle( m_owner );

        if( ownerAnim && ownerAnim->ClassID() == EmberForceObject_CLASSID ) {
            static_cast<EmberForceObject*>( ownerAnim )->UpdateForceField( *this, t );
        }

        // The owner should have updated this ... but it might not have.
        if( !m_validity.InInterval( t ) ) {
            m_field.reset();
            m_validity.SetInfinite();
            m_forceAccessor.reset( frantic::graphics::vector3f( 0.f, 0.f, 0.f ) );
            m_scale = 1.f;
        }
    }

    if( !m_field )
        return Point3( 0, 0, 0 );

    char* buffer = (char*)alloca( m_field->get_channel_map().structure_size() );

    Point3 result;
    if( m_field->evaluate_field( buffer, frantic::max3d::from_max_t( pos ) ) ) {
        result = frantic::max3d::to_max_t( m_forceAccessor.get( buffer ) * m_scale );
    } else {
        result.Set( 0, 0, 0 );
    }

    return result;
}

void EmberForceField::DeleteThis() { delete this; }

ForceField* EmberForceObject::GetForceField( INode* /*node*/ ) { return new EmberForceField( this ); }

void EmberForceObject::UpdateForceField( EmberForceField& theForceField, TimeValue t ) {
    this->cache_field( t );

    // Interval validity = FOREVER;

    // INode* pNode = m_pblock->GetINode( kEmberNode );
    // if( pNode != NULL ){
    //	boost::shared_ptr<frantic::volumetrics::field_interface> field = frantic::max3d::volumetrics::create_field(
    //pNode, t, validity ); 	if( field ){
    if( m_field ) {
        Interval validity = m_cacheValidity;
        float scale = 1.f;
        m_pblock->GetValue( kForceScale, t, scale, validity );

        double timeDivisor = 1.0;
        if( get_time_type() == time_ticks )
            timeDivisor = static_cast<double>( TIME_TICKSPERSEC * TIME_TICKSPERSEC );

        // TODO: This is pretty bad since timeDivisor is usually very small. Perhaps we need to store doubles?
        float finalScale = static_cast<float>( static_cast<double>( scale ) / timeDivisor );

        const MCHAR* channelName = m_pblock->GetStr( kChannelName );

        theForceField.Update( m_field, channelName ? channelName : _T(""), finalScale, validity );
        //}
    }
}

CollisionObject* EmberForceObject::GetCollisionObject( INode* /*node*/ ) { return NULL; }

ClassDesc2* GetEmberForceModDesc() {
    static EmberForceModDesc theEmberForceModDesc;
    return &theEmberForceModDesc;
}

class EmberForceMod : public SimpleWSMMod {
  public:
    EmberForceMod();
    EmberForceMod( INode* node, EmberForceObject* obj );

// From Animatable
#if MAX_VERSION_MAJOR < 24
    virtual void GetClassName( TSTR& s ) { s = GetEmberForceModDesc()->ClassName(); }
#else
    virtual void GetClassName( TSTR& s, bool localized ) const { s = GetEmberForceModDesc()->ClassName(); }
#endif

    virtual SClass_ID SuperClassID() { return GetEmberForceModDesc()->SuperClassID(); }
    virtual void DeleteThis() { delete this; }
    virtual Class_ID ClassID() { return GetEmberForceModDesc()->ClassID(); }
    virtual RefTargetHandle Clone( RemapDir& remap );

#if MAX_VERSION_MAJOR < 24
    virtual TYPE_STRING_TYPE GetObjectName() {
        return const_cast<TYPE_STRING_TYPE>( GetEmberForceModDesc()->ClassName() );
    }
#else
    virtual TYPE_STRING_TYPE GetObjectName( bool localized ) const {
        return const_cast<TYPE_STRING_TYPE>( GetEmberForceModDesc()->ClassName() );
    }
#endif

    virtual void ModifyObject( TimeValue t, ModContext& mc, ObjectState* os, INode* node );

    // From SimpleWSMMod
    virtual Interval GetValidity( TimeValue t );
    virtual Deformer& GetDeformer( TimeValue t, ModContext& mc, Matrix3& mat, Matrix3& invmat );

  private:
    EmberForceField m_field;
};

Modifier* EmberForceObject::CreateWSMMod( INode* node ) { return new EmberForceMod( node, this ); }

void* EmberForceModDesc::Create( BOOL /*loading*/ ) { return new EmberForceMod; }

EmberForceMod::EmberForceMod() {}

EmberForceMod::EmberForceMod( INode* node, EmberForceObject* /*obj*/ ) { ReplaceReference( SIMPWSMMOD_NODEREF, node ); }

RefTargetHandle EmberForceMod::Clone( RemapDir& remap ) {
    EmberForceMod* newob = new EmberForceMod( nodeRef, static_cast<EmberForceObject*>( obRef ) );
    newob->SimpleWSMModClone( this, remap );
    BaseClone( this, newob, remap );
    return newob;
}

void EmberForceMod::ModifyObject( TimeValue t, ModContext& mc, ObjectState* os, INode* node ) {
    ParticleObject* obj = GetParticleInterface( os->obj );
    if( obj ) {
        m_field.Reset( static_cast<EmberForceObject*>( GetWSMObject( t ) ) );
        if( EmberForceObject* obj = static_cast<EmberForceObject*>( GetWSMObject( t ) ) )
            obj->UpdateForceField( m_field, t );

        obj->ApplyForceField( &m_field );
    }
}

class EmberDeformer : public Deformer {
  public:
    Point3 Map( int /*i*/, Point3 p ) { return p; }
};

Interval EmberForceMod::GetValidity( TimeValue t ) {
    if( !m_field.m_validity.InInterval( t ) )
        static_cast<EmberForceObject*>( GetWSMObject( t ) )->UpdateForceField( m_field, t );
    return m_field.m_validity;
}

Deformer& EmberForceMod::GetDeformer( TimeValue /*t*/, ModContext& /*mc*/, Matrix3& /*mat*/, Matrix3& /*invmat*/ ) {
    static EmberDeformer theEmberDeformer;
    return theEmberDeformer;
}

ClassDesc2* GetEmberForceObjectDesc() { return &ember::max3d::EmberForceObjectDesc::GetInstance(); }

} // namespace max3d
} // namespace ember

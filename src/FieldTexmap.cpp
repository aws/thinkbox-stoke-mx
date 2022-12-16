// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource_ember.h"

#pragma warning( push, 3 )
#include <iparamb2.h>
#include <iparamm2.h>
#include <stdmat.h>
#pragma warning( pop )

#include <stoke/max3d/FieldTexmap.hpp>
#include <stoke/max3d/ISimEmber.hpp>

#include <frantic/max3d/convert.hpp>
#include <frantic/max3d/exception.hpp>
#include <frantic/max3d/max_utility.hpp>
#include <frantic/max3d/paramblock_builder.hpp>
#include <frantic/max3d/volumetrics/IEmberField.hpp>

#include <frantic/graphics/vector3f.hpp>
#include <frantic/logging/logging_level.hpp>

#include <custcont.h>

#undef NDEBUG
#include <cassert>

namespace ember {
namespace max3d {

class FieldTexmapDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_pbDesc;

  public:
    FieldTexmapDesc();

    int IsPublic() { return TRUE; }
    void* Create( BOOL loading ) { return new FieldTexmap( loading ); }
    const TCHAR* ClassName() { return _T( FieldTexmap_DISPLAYNAME ); }
    SClass_ID SuperClassID() { return TEXMAP_CLASS_ID; }
    Class_ID ClassID() { return FieldTexmap_CLASSID; }
    const TCHAR* Category() { return TEXMAP_CAT_3D; }
    const TCHAR* InternalName() { return _T( FieldTexmap_NAME ); }
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( FieldTexmap_DISPLAYNAME ); }
#endif
};
ClassDesc2* GetFieldTexmapDesc() {
    static FieldTexmapDesc theFieldTexmapClassDesc;
    return &theFieldTexmapClassDesc;
}

// m_pblock paramters id
enum { kMainBlock };

// rollout id
enum { kMainRollout };

// parameter ids
enum { kEmberNode, kFieldChannnel };

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

class FieldTexmapAccessor : public PBAccessor {
  public:
    static FieldTexmapAccessor& GetInstance();

    virtual void Get( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t, Interval& valid );
    virtual void Set( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t );
};

FieldTexmapAccessor& FieldTexmapAccessor::GetInstance() {
    static FieldTexmapAccessor theFieldTexmapAccessor;
    return theFieldTexmapAccessor;
}

void FieldTexmapAccessor::Get( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t,
                               Interval& valid ) {}

void FieldTexmapAccessor::Set( PB2Value& v, ReferenceMaker* owner, ParamID id, int /*tabIndex*/, TimeValue t ) {
    if( owner == NULL )
        return;

    if( id == kEmberNode ) {
        INode* pNode = NULL;

        if( v.r != NULL )
            pNode = static_cast<INode*>( v.r->GetInterface( INODE_INTERFACE ) );

        if( pNode != NULL )
            FF_LOG( debug ) << _T("FieldTexmapAccessor::Set() with node: \"") << pNode->GetName() << _T("\"")
                            << std::endl;

        static_cast<FieldTexmap*>( owner )->on_node_picked( t );
    }
}

class FieldTexmapDlgProc : public ParamMap2UserDlgProc {
  public:
    static FieldTexmapDlgProc& GetInstance();

    virtual INT_PTR DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

    virtual void DeleteThis() {}
};

FieldTexmapDlgProc& FieldTexmapDlgProc::GetInstance() {
    static FieldTexmapDlgProc theFieldTexmapDlgProc;
    return theFieldTexmapDlgProc;
}

#define WM_UPDATE_LIST ( WM_APP + 0xd3d )

INT_PTR FieldTexmapDlgProc::DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
    switch( msg ) {
    case WM_INITDIALOG:
        if( IParamBlock2* pb = map->GetParamBlock() ) {
            if( HWND hWndCombo = GetDlgItem( hWnd, IDC_EMBERTEX_CHANNEL_COMBO ) ) {
                ComboBox_SetCueBannerText( hWndCombo, _T("Select a channel ...") );

                if( FieldTexmap* ft = static_cast<FieldTexmap*>( pb->GetOwner() ) ) {
                    std::vector<frantic::tstring> options;

                    ft->get_available_channels( t, options );

                    for( std::vector<frantic::tstring>::const_iterator it = options.begin(), itEnd = options.end();
                         it != itEnd; ++it )
                        ComboBox_AddString( hWndCombo, it->c_str() );
                }

                const MCHAR* curText = pb->GetStr( kFieldChannnel );

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
        if( LOWORD( wParam ) == IDC_EMBERTEX_CHANNEL_COMBO ) {
            // We need to subclass the edit control and intercept the enter key to allow custom typing in the combo box
            // (in addition to using lost-focus).

            /*if( HIWORD(wParam) == WM_CHAR ){

            }else if( HIWORD(wParam) == CBN_EDITCHANGE ){
                    TCHAR curText[64];

                    ComboBox_GetText( reinterpret_cast<HWND>( lParam ), curText, 64 );

                    if( IParamBlock2* pb = map->GetParamBlock() )
                            pb->SetValue( kFieldChannnel, 0, curText );

                    return TRUE;
            }else if( HIWORD(wParam) == CBN_SELCHANGE ){
            }else*/
            if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                TCHAR curText[64];

                ComboBox_GetText( reinterpret_cast<HWND>( lParam ), curText, 64 );

                if( IParamBlock2* pb = map->GetParamBlock() )
                    pb->SetValue( kFieldChannnel, 0, curText );

                return TRUE;
            } /*else if( HIWORD(wParam) == CBN_SETFOCUS ){
                     DisableAccelerators();

                     return TRUE;
             }else if( HIWORD(wParam) == CBN_KILLFOCUS ){
                     EnableAccelerators();

                     return TRUE;
             }*/
        } else if( LOWORD( wParam ) == IDC_EMBERTEX_NODEMENU_BUTTON ) {
            if( HIWORD( wParam ) == BN_CLICKED ) {
                ReferenceTarget* rtarg = NULL;
                if( IParamBlock2* pb = map->GetParamBlock() ) {
                    ReferenceMaker* rmaker = pb->GetOwner();
                    if( rmaker && rmaker->IsRefTarget() )
                        rtarg = static_cast<ReferenceTarget*>( rmaker );
                }

                frantic::max3d::mxs::expression( _T("try(::StokeContextMenuStruct.OpenFieldTexmapMenu owner)catch()") )
                    .bind( _T("owner"), rtarg )
                    .evaluate<void>();

                return TRUE;
            }
        }
        break;
    case WM_UPDATE_LIST:
        if( HWND hWndCombo = GetDlgItem( hWnd, IDC_EMBERTEX_CHANNEL_COMBO ) ) {
            ComboBox_ResetContent( hWndCombo );

            if( IParamBlock2* pb = map->GetParamBlock() ) {
                if( FieldTexmap* ft = static_cast<FieldTexmap*>( pb->GetOwner() ) ) {
                    std::vector<frantic::tstring> options;

                    ft->get_available_channels( t, options );

                    for( std::vector<frantic::tstring>::const_iterator it = options.begin(), itEnd = options.end();
                         it != itEnd; ++it )
                        ComboBox_AddString( hWndCombo, it->c_str() );
                }

                const MCHAR* curText = pb->GetStr( kFieldChannnel );

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

FieldTexmapDesc::FieldTexmapDesc()
    : m_pbDesc( kMainBlock, _T("Parameters"), 0, FieldTexmap_VERSION ) {
    m_pbDesc.OwnerClassDesc( this );
    m_pbDesc.OwnerRefNum( 0 );

    m_pbDesc.RolloutTemplate( kMainRollout, IDD_EMBERTEX, IDS_EMBERTEX_TITLE, &FieldTexmapDlgProc::GetInstance() );

    m_pbDesc.Parameter<INode*>( kEmberNode, _T("FieldNode"), 0 )
        .Validator( &EmberNodeValidator::GetInstance() )
        .Accessor( &FieldTexmapAccessor::GetInstance() )
        .PickNodeButtonUI( kMainRollout, IDC_EMBERTEX_NODEPICKER_BUTTON, IDS_PRTEMBER_SOURCE_CAPTION );

    m_pbDesc.Parameter<const MCHAR*>( kFieldChannnel, _T("ChannelName"), 0 ).DefaultValue( _T("Color") );
}

FieldTexmap::FieldTexmap( BOOL loading ) {
    m_pblock = NULL;
    m_xyzGen = NULL;
    m_inRenderMode = false;

    if( !loading ) {
        Reset();
    }
}

FieldTexmap::~FieldTexmap() {}

ClassDesc2* FieldTexmap::GetClassDesc() { return GetFieldTexmapDesc(); }

void FieldTexmap::on_node_picked( TimeValue /*t*/ ) {}

void FieldTexmap::on_node_changed( TimeValue /*t*/ ) {}

void FieldTexmap::get_available_channels( TimeValue t, std::vector<frantic::tstring>& outChannels ) {
    cache_field( t );

    if( m_field ) {
        const frantic::channels::channel_map& fieldMap = m_field->get_channel_map();

        for( std::size_t i = 0, iEnd = fieldMap.channel_count(); i < iEnd; ++i ) {
            if( ( fieldMap[i].arity() == 1 || fieldMap[i].arity() == 3 ) &&
                frantic::channels::is_channel_data_type_float( fieldMap[i].data_type() ) )
                outChannels.push_back( fieldMap[i].name() );
        }
    }
}

// From MtlBase
void FieldTexmap::Reset() {
    this->GetClassDesc()->MakeAutoParamBlocks( this );

    ReplaceReference( 1, GetNewDefaultXYZGen() );

    if( m_xyzGen->IsStdXYZGen() )
        static_cast<StdXYZGen*>( m_xyzGen )->SetCoordSystem( XYZ_WORLD_COORDS );
}

void FieldTexmap::Update( TimeValue t, Interval& valid ) {
    if( !m_updateInterval.InInterval( t ) ) {
        m_updateInterval = FOREVER;

        try {
            cache_field( t );

            m_updateInterval &= m_fieldInterval;

            const MCHAR* szChannelName = m_pblock->GetStr( kFieldChannnel );
            if( szChannelName == NULL || szChannelName[0] == _T( '\0' ) )
                szChannelName = _T("Color");

            m_monoAccessor.reset();
            m_colorAccessor.reset( frantic::graphics::color3f::black() );

            if( m_field && m_field->get_channel_map().has_channel( szChannelName ) &&
                frantic::channels::is_channel_data_type_float(
                    m_field->get_channel_map()[szChannelName].data_type() ) ) {
                if( m_field->get_channel_map()[szChannelName].arity() == 3 ) {
                    m_colorAccessor =
                        m_field->get_channel_map().get_cvt_accessor<frantic::graphics::color3f>( szChannelName );
                } else if( m_field->get_channel_map()[szChannelName].arity() == 1 ) {
                    m_monoAccessor = m_field->get_channel_map().get_cvt_accessor<float>( szChannelName );
                }
            }

        } catch( const std::exception& e ) {
            m_field.reset();

            FF_LOG( error ) << _T("Stoke FieldTexmap initialization failure:\n")
                            << frantic::strings::to_tstring( e.what() ) << std::endl;

            if( frantic::max3d::is_network_render_server() )
                GetCOREInterface()->Log()->LogEntry(
                    SYSLOG_ERROR, NO_DIALOG, _T("Stoke FieldTexmap initialization failure"),
                    const_cast<TCHAR*>( frantic::strings::to_tstring( e.what() ).c_str() ) );
        }

        if( !m_xyzGen ) {
            ReplaceReference( 1, GetNewDefaultXYZGen() );

            if( m_xyzGen->IsStdXYZGen() )
                static_cast<StdXYZGen*>( m_xyzGen )->SetCoordSystem( XYZ_WORLD_COORDS );
        }

        m_xyzGen->Update( t, m_updateInterval );
    }

    valid &= m_updateInterval;
}

Interval FieldTexmap::Validity( TimeValue t ) {
    Interval result = FOREVER;
    this->Update( t, result );
    return result;
}

ParamDlg* FieldTexmap::CreateParamDlg( HWND hwMtlEdit, IMtlParams* imp ) {
    // TODO: This function needs to be more RAII-like.
    IAutoMParamDlg* primaryDlg = this->GetClassDesc()->CreateParamDlgs( hwMtlEdit, imp, this );

    if( m_xyzGen )
        primaryDlg->AddDlg( m_xyzGen->CreateParamDlg( hwMtlEdit, imp ) );

    return primaryDlg;
}

RefTargetHandle FieldTexmap::GetReference( int i ) {
    if( i < 1 )
        return GenericReferenceTarget<Texmap, FieldTexmap>::GetReference( i );

    return i == 1 ? m_xyzGen : NULL;
}

void FieldTexmap::SetReference( int i, RefTargetHandle rtarg ) {
    if( i < 1 )
        GenericReferenceTarget<Texmap, FieldTexmap>::SetReference( i, rtarg );
    else if( i == 1 )
        m_xyzGen = (XYZGen*)rtarg;
}

Animatable* FieldTexmap::SubAnim( int i ) {
    if( i < 1 )
        return GenericReferenceTarget<Texmap, FieldTexmap>::SubAnim( i );

    return i == 1 ? m_xyzGen : NULL;
}

#if MAX_VERSION_MAJOR < 24
TSTR FieldTexmap::SubAnimName( int i ) {
    if( i < 1 )
        return GenericReferenceTarget<Texmap, FieldTexmap>::SubAnimName( i );

    return i == 1 ? _T("XYZGen") : _T("");
}
#else
TSTR FieldTexmap::SubAnimName( int i, bool localized ) {
    if( i < 1 )
        return GenericReferenceTarget<Texmap, FieldTexmap>::SubAnimName( i, localized );

    return i == 1 ? _T("XYZGen") : _T("");
}
#endif

RefResult FieldTexmap::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& /*partID*/,
                                         RefMessage message, BOOL /*propagate*/ ) {
    switch( message ) {
    case REFMSG_CHANGE:
        if( hTarget == m_pblock ) {
            // 3ds max will not properly update the UIs for texture map buttons on its own
            ParamID changing_param = m_pblock->LastNotifyParamID();
            m_pblock->GetDesc()->InvalidateUI( changing_param );

            if( changing_param == kEmberNode ) {
                m_field.reset();
                m_fieldInterval.SetEmpty();
                m_updateInterval.SetEmpty();

                if( IParamMap2* pmap = m_pblock->GetMap( kMainRollout ) )
                    PostMessage( pmap->GetHWnd(), WM_UPDATE_LIST, 0, 0 );
            }
        } else if( hTarget == m_xyzGen ) {
            m_updateInterval.SetEmpty();
        }
        break;
    }
    return REF_SUCCEED;
}

// load and save are from the sample code. this how all materials seem to implement these functions.
#define MTL_HDR_CHUNK 0x4000

IOResult FieldTexmap::Save( ISave* isave ) {
    // from sample code
    IOResult res;
    isave->BeginChunk( MTL_HDR_CHUNK );
    res = MtlBase::Save( isave );
    if( res != IO_OK )
        return res;
    isave->EndChunk();
    return IO_OK;
}

IOResult FieldTexmap::Load( ILoad* iload ) {
    // from sample code
    IOResult res;
    int id;
    while( IO_OK == ( res = iload->OpenChunk() ) ) {
        switch( id = iload->CurChunkID() ) {
        case MTL_HDR_CHUNK:
            res = MtlBase::Load( iload );
            break;
        }
        iload->CloseChunk();
        if( res != IO_OK )
            return res;
    }

    return IO_OK;
}

Texmap* FieldTexmap::GetSubTexmap( int /*i*/ ) { return NULL; }

void FieldTexmap::SetSubTexmap( int /*i*/, Texmap* /*m*/ ) {}

#if MAX_VERSION_MAJOR < 24
TSTR FieldTexmap::GetSubTexmapSlotName( int /*i*/ ) { return _T(""); }
#else
TSTR FieldTexmap::GetSubTexmapSlotName( int /*i*/, bool localized ) { return _T(""); }
#endif

AColor FieldTexmap::EvalColor( ShadeContext& sc ) {
    if( gbufID )
        sc.SetGBufferID( gbufID );
    if( !sc.doMaps )
        return AColor( 0, 0, 0 );
    if( sc.InMtlEditor() )
        return AColor( 0, 0, 0 );
    if( !m_field )
        return AColor( 0, 0, 0 );

    // see if we've cached this color
    AColor c;
    if( sc.GetCache( this, c ) )
        return c;

    Point3 p, dp;

    m_xyzGen->GetXYZ( sc, p, dp );

    // TODO: Filter using max_abs_component of dp.

    // Transform p in the field's internal space.

    frantic::graphics::vector3f realP = /*m_toFieldSpace **/ frantic::max3d::from_max_t( p );

    char* buffer = (char*)alloca( m_field->get_channel_map().structure_size() );

    if( m_field->evaluate_field( buffer, realP ) ) {
        if( !m_monoAccessor.is_valid() ) {
            c = frantic::max3d::to_max_t( m_colorAccessor.get( buffer ) );
        } else {
            c.a = 1.f;
            c.r = c.g = c.b = m_monoAccessor.get( buffer );
        }
    } else {
        c.Black();
    }

    sc.PutCache( this, c );

    return c;
}

float FieldTexmap::EvalMono( ShadeContext& sc ) {
    /*return Intens( EvalColor(sc) );*/
    if( gbufID )
        sc.SetGBufferID( gbufID );
    if( !sc.doMaps )
        return 0.f;
    if( sc.InMtlEditor() )
        return 0.f;
    if( !m_field )
        return 0.f;

    // see if we've cached this color
    float f;
    if( sc.GetCache( this, f ) )
        return f;

    Point3 p, dp;

    m_xyzGen->GetXYZ( sc, p, dp );

    // TODO: Filter using max_abs_component of dp.

    // Transform p in the field's internal space.

    frantic::graphics::vector3f realP = /*m_toFieldSpace **/ frantic::max3d::from_max_t( p );

    char* buffer = (char*)alloca( m_field->get_channel_map().structure_size() );

    if( m_field->evaluate_field( buffer, realP ) ) {
        if( !m_monoAccessor.is_valid() ) {
            f = Intens( frantic::max3d::to_max_t( m_colorAccessor.get( buffer ) ) );
        } else {
            f = m_monoAccessor.get( buffer );
        }
    } else {
        f = 0.f;
    }

    sc.PutCache( this, f );

    return f;
}

Point3 FieldTexmap::EvalNormalPerturb( ShadeContext& sc ) {
    if( gbufID )
        sc.SetGBufferID( gbufID );
    if( !sc.doMaps )
        return Point3( 0.0f, 0.0f, 0.0f );
    if( sc.InMtlEditor() )
        return Point3( 0.0f, 0.0f, 0.0f );
    if( !m_field )
        return Point3( 0.0f, 0.0f, 0.0f );

    Point3 np;
    if( sc.GetCache( this, np ) )
        return np;

    Point3 p, dp;
    m_xyzGen->GetXYZ( sc, p, dp );

    Point3 M[3];
    m_xyzGen->GetBumpDP( sc, M );

    char* buffer = (char*)alloca( m_field->get_channel_map().structure_size() );

    frantic::graphics::vector3f realP = /*m_toFieldSpace **/ frantic::max3d::from_max_t( p );

    if( !m_monoAccessor.is_valid() ) {
        // Use the vector output of the field to calculate the normal perturb. This is pseudo-normal mapping, but not
        // really. Essentially we just use the color of the field to offset the normal along the bump basis vectors.
        if( m_field->evaluate_field( buffer, realP ) ) {
            np = static_cast<Point3>( frantic::max3d::to_max_t( m_colorAccessor.get( buffer ) ) );

            np = np.x * M[0] + np.y * M[1] * np.z * M[2];
        } else {
            np.Set( 0, 0, 0 );
        }
    } else {
        float del = 0.125f; // Arbitrary. Should we use dp for this?

        // Use finite differences to calculate the heightfield gradient to maniuplate the normal.
        // This calculation is consistent with how 3DS Max procedural maps operate.
        float x1 = 0, x0 = 0;
        if( m_field->evaluate_field( buffer, realP + del * frantic::max3d::from_max_t( M[0] ) ) )
            x1 = m_monoAccessor.get( buffer );
        if( m_field->evaluate_field( buffer, realP - del * frantic::max3d::from_max_t( M[0] ) ) )
            x0 = m_monoAccessor.get( buffer );

        np.x = ( x1 - x0 ) / ( 2.f * del );

        float y1 = 0, y0 = 0;
        if( m_field->evaluate_field( buffer, realP + del * frantic::max3d::from_max_t( M[1] ) ) )
            y1 = m_monoAccessor.get( buffer );
        if( m_field->evaluate_field( buffer, realP - del * frantic::max3d::from_max_t( M[1] ) ) )
            y0 = m_monoAccessor.get( buffer );

        np.y = ( y1 - y0 ) / ( 2.f * del );

        float z1 = 0, z0 = 0;
        if( m_field->evaluate_field( buffer, realP + del * frantic::max3d::from_max_t( M[2] ) ) )
            z1 = m_monoAccessor.get( buffer );
        if( m_field->evaluate_field( buffer, realP - del * frantic::max3d::from_max_t( M[2] ) ) )
            z0 = m_monoAccessor.get( buffer );

        np.z = ( z1 - z0 ) / ( 2.f * del );
    }

    np = sc.VectorFromNoScale( np, REF_OBJECT );

    return np;
}

int FieldTexmap::RenderBegin( TimeValue t, ULONG flags ) {
    if( flags != RENDERBEGIN_IN_MEDIT ) {
        // switching into render-mode calls initialize. this will initialize the first frame in the render sequence.
        // subsequent frames in the sequence will be initialized by the "Update" call.
        m_fieldInterval.SetEmpty();
        m_updateInterval.SetEmpty();

        m_inRenderMode = true;

        // We need to notify that we changed when switching to/from render mode.
        this->NotifyDependents( FOREVER, PART_ALL, REFMSG_CHANGE );
    }
    return 1;
}

int FieldTexmap::RenderEnd( TimeValue /*t*/ ) {
    // Check this so we don't invalidate due to use in the material editor.
    if( m_inRenderMode ) {
        m_inRenderMode = false;

        m_fieldInterval.SetEmpty();
        m_updateInterval.SetEmpty();

        // We need to notify that we changed when switching to/from render mode.
        this->NotifyDependents( FOREVER, PART_ALL, REFMSG_CHANGE );
    }

    return 1;
}

//
//
// private member functions
//
//

void FieldTexmap::cache_field( TimeValue t ) {
    if( m_fieldInterval.InInterval( t ) )
        return;

    m_field.reset();
    m_fieldInterval = FOREVER;

    INode* node = m_pblock->GetINode( kEmberNode );
    if( node )
        m_field = frantic::max3d::volumetrics::create_field( node, t, m_fieldInterval );

    assert( m_fieldInterval.InInterval( t ) );
}

void FieldTexmap::initialize( TimeValue t ) {}

} // namespace max3d
} // namespace ember

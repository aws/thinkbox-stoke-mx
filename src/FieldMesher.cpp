// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "StokeMXVersion.h"
#include "stoke/max3d/EmberPipeObject.hpp"
#include "stoke/max3d/FieldMesher.hpp"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>
#include <ember/openvdb.hpp>
#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>
#include <frantic/max3d/volumetrics/IEmberField.hpp>
#include <frantic/volumetrics/field_interface.hpp>
#include <krakatoa/max3d/PRTObject.hpp>

#include <frantic/graphics/color3f.hpp>
#include <frantic/logging/progress_logger.hpp>
#include <frantic/max3d/geometry/mesh.hpp>
#include <frantic/max3d/geometry/polymesh.hpp>
#include <frantic/max3d/geopipe/named_selection_sets.hpp>
#include <frantic/max3d/logging/status_panel_progress_logger.hpp>
#include <frantic/max3d/max_utility.hpp>

#include <frantic/math/utils.hpp>

#include <boost/assign/list_of.hpp>
#include <boost/numeric/conversion/bounds.hpp>
#include <boost/thread.hpp>
#include <frantic/channels/named_channel_data.hpp>
#include <frantic/geometry/polymesh3.hpp>
#include <frantic/geometry/polymesh3_builder.hpp>
#include <frantic/volumetrics/levelset/rle_trilerp.hpp>

#include <Shellapi.h>

#include <frantic/geometry/fractional_mesh_interface.hpp>
#include <frantic/geometry/mesh_interface_utils.hpp>
#include <frantic/geometry/polymesh3_builder.hpp>
#include <frantic/geometry/polymesh3_interface.hpp>
#include <iostream>

#include <algorithm>

#if MAX_VERSION_MAJOR >= 14
#include <Graphics/IDisplayManager.h>
#endif

#include <notify.h>

#include "resource_ember.h"
#include "stoke/max3d/fixed_combo_box.hpp"

#ifdef X_AXIS
#undef X_AXIS
#endif

#ifdef Y_AXIS
#undef Y_AXIS
#endif

#ifdef Z_AXIS
#undef Z_AXIS
#endif

#include <openvdb/openvdb.h>
#include <openvdb/tools/Filter.h>
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/tools/LevelSetFilter.h>
#include <openvdb/tools/VolumeToMesh.h>

namespace DISPLAY_MODE {
enum display_mode_enum {
    MESH,
    BOX,
    VERTICES,
    FACES,
    //
    COUNT
};
};

namespace SAMPLER_MODE {
enum sampler_mode_enum {
    LINEAR,
    QUADRATIC,

    COUNT
};
};
#define DISPLAY_MODE_MESH _T("Mesh")
#define DISPLAY_MODE_BOX _T("Box")
#define DISPLAY_MODE_VERTICES _T("Verts")
#define DISPLAY_MODE_FACES _T("Faces")

#define DEFAULT_DISPLAY_MODE ( DISPLAY_MODE::MESH )

#define DEFAULT_SAMPLER ( SAMPLER_MODE::LINEAR )
#define SAMPLER_MODE_QUADRATIC _T("Quadratic")
#define SAMPLER_MODE_LINEAR _T("Linear")

/* namespaces */
using namespace frantic;
using namespace frantic::geometry;
using namespace frantic::files;
using namespace frantic::graphics;
using namespace frantic::max3d;
using namespace frantic::max3d::geometry;
using namespace frantic::channels;
using namespace frantic::geometry;
using namespace boost::assign;

extern HINSTANCE hInstance;

extern void BuildMesh_Text_EMBER_Mesher( Mesh& outMesh );

boost::shared_ptr<Mesh> BuildIconMesh() {

    Mesh* theIcon = CreateNewMesh();

    BuildMesh_Text_EMBER_Mesher( *theIcon );
    return boost::shared_ptr<Mesh>( theIcon );
}
TCHAR* GetString( UINT id ) {
    static TCHAR buf[256];

    if( hInstance )
        return LoadString( hInstance, id, buf, sizeof( buf ) / sizeof( TCHAR ) ) ? buf : NULL;
    return NULL;
}

boost::shared_ptr<Mesh> FieldMesher::m_pIconMesh = BuildIconMesh();

FieldMesher* FieldMesher::editObj = 0;

class FieldMesherValidator : public PBValidator {
  public:
    static FieldMesherValidator* GetInstance() {
        static FieldMesherValidator theFieldMesherValidator;
        return &theFieldMesherValidator;
    }

    virtual BOOL Validate( PB2Value& /*v*/ ) { return FALSE; }

    virtual BOOL Validate( PB2Value& v, ReferenceMaker* /*owner*/, ParamID id, int /*tabIndex*/ ) {

        if( !v.r )
            return TRUE;

        if( INode* pNode = static_cast<INode*>( v.r->GetInterface( INODE_INTERFACE ) ) )
            return frantic::max3d::volumetrics::is_field( pNode, GetCOREInterface()->GetTime() ) ? TRUE : FALSE;

        return FALSE;
    }

    virtual void DeleteThis() {}
};

// parameter list
enum {
    pb_targetNode,
    pb_surfaceLevel,
    pb_adaptivity,
    pb_surfaceChannel,
    pb_showIcon,
    pb_iconSize,
    pb_flipNormals,
    pb_keepMeshInMemory,
    pb_writeVelocityMapChannel,
    pb_velocityMapChannel,
    pb_enableViewportMesh,
    pb_displayMode,
    pb_viewportResolution,
    pb_renderSampler,
    pb_renderUsingViewportSettings,
    pb_renderResolution,
    pb_viewportSampler,
    pb_applyObjectTransforms,
    pb_displayPercent,
    pb_displayLimit,
    pb_useDisplayLimit
};

enum {
    main_param_map,
    src_param_map,
    meshing_param_map,
    //
    param_map_count // keep last
};

class MainDlgProc : public ParamMap2UserDlgProc {
    FieldMesher* m_obj;
    std::vector<int> m_displayModeIndexToCode;

  public:
    MainDlgProc()
        : m_obj( 0 ) {}
    virtual ~MainDlgProc() {}
    virtual INT_PTR DlgProc( TimeValue t, IParamMap2* /*map*/, HWND hWnd, UINT msg, WPARAM wParam, LPARAM /*lParam*/ ) {
        switch( msg ) {
        case WM_INITDIALOG:
            if( m_obj ) {

                fixed_combo_box displayModeControl( hWnd, IDC_DISPLAY_MODE, FieldMesher::get_display_mode_codes,
                                                    FieldMesher::get_display_mode_name, m_displayModeIndexToCode );
                displayModeControl.reset_strings();
                displayModeControl.set_cur_sel_code( m_obj->get_display_mode( t ) );

                UpdateEnables();
            }
            return TRUE;
        case WM_COMMAND: {
            const int id = LOWORD( wParam );
            const int notifyCode = HIWORD( wParam );
            switch( id ) {

            case IDC_DISPLAY_MODE:
                if( CBN_SELCHANGE == notifyCode ) {
                    fixed_combo_box displayModeControl( hWnd, IDC_DISPLAY_MODE, FieldMesher::get_display_mode_codes,
                                                        FieldMesher::get_display_mode_name, m_displayModeIndexToCode );
                    int sel = displayModeControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_display_mode( sel, t );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
                break;
            }
        }
        }
        return FALSE;
    }

    void InvalidateUI( HWND hwnd, int element ) {
        if( !hwnd ) {
            return;
        }
        switch( element ) {

        case pb_displayMode: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int displayMode = m_obj->get_display_mode( 0 );
                fixed_combo_box displayModeControl( hwnd, IDC_DISPLAY_MODE, FieldMesher::get_display_mode_codes,
                                                    FieldMesher::get_display_mode_name, m_displayModeIndexToCode );
                displayModeControl.set_cur_sel_code( displayMode );
            }
        } break;
        }
    }
    virtual void DeleteThis() {}
    void Update( TimeValue /*t*/ ) {
        try {
            UpdateEnables();
        } catch( const std::exception& e ) {
            frantic::tstring errmsg =
                _T( "MainDlgProc::Update: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
            FF_LOG( error ) << errmsg << std::endl;
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Error" ), _T( "%s" ), e.what() );
            if( frantic::max3d::is_network_render_server() )
                throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
        }
    }
    void UpdateEnables() {
        if( !m_obj ) {
            return;
        }

        IParamBlock2* pb = m_obj->GetParamBlockByID( field_mesher_param_block );
        if( !pb ) {
            return;
        }
        IParamMap2* pm = pb->GetMap( main_param_map );
        if( !pm ) {
            return;
        }
        const HWND hwnd = pm->GetHWnd();
        if( !hwnd ) {
            return;
        }

        const int displayMode = pb->GetInt( pb_displayMode );
        const bool enableMapChannel = pb->GetInt( pb_writeVelocityMapChannel ) != 0;
        const bool enableDisplayLimit = pb->GetInt( pb_useDisplayLimit ) != 0;
        const bool fractionalDisplay = displayMode == DISPLAY_MODE::FACES || displayMode == DISPLAY_MODE::VERTICES;

        EnableWindow( GetDlgItem( hwnd, IDC_VELOCITY_MAP_CHANNEL_STATIC ), enableMapChannel );
        pm->Enable( pb_velocityMapChannel, enableMapChannel );

        EnableWindow( GetDlgItem( hwnd, IDC_DISPLAY_PERCENT_STATIC ), fractionalDisplay );
        pm->Enable( pb_displayPercent, fractionalDisplay );
        pm->Enable( pb_useDisplayLimit, fractionalDisplay );
        pm->Enable( pb_displayLimit, fractionalDisplay && enableDisplayLimit );
    }
    void set_obj( FieldMesher* obj ) { m_obj = obj; }
};

MainDlgProc* GetMainDlgProc() {
    static MainDlgProc mainDlgProc;
    return &mainDlgProc;
}

class MeshinghDlgProc : public ParamMap2UserDlgProc {
    FieldMesher* m_obj;
    std::vector<int> m_renderSamplerIndexToCode;
    std::vector<int> m_viewportSamplerIndexToCode;

  public:
    MeshinghDlgProc()
        : m_obj( 0 ) {}
    virtual ~MeshinghDlgProc() {}
    virtual INT_PTR DlgProc( TimeValue t, IParamMap2* /*map*/, HWND hWnd, UINT msg, WPARAM wParam, LPARAM /*lParam*/ ) {
        switch( msg ) {
        case WM_INITDIALOG:
            if( m_obj ) {

                fixed_combo_box renderModeControl( hWnd, IDC_FIELD_MESHER_RENDER_SAMPLER,
                                                   FieldMesher::get_sampler_codes, FieldMesher::get_sampler_name,
                                                   m_renderSamplerIndexToCode );
                renderModeControl.reset_strings();
                renderModeControl.set_cur_sel_code( m_obj->get_render_sampler( t ) );

                fixed_combo_box viewportModeControl( hWnd, IDC_FIELD_MESHER_VIEWPORT_SAMPLER,
                                                     FieldMesher::get_sampler_codes, FieldMesher::get_sampler_name,
                                                     m_viewportSamplerIndexToCode );
                viewportModeControl.reset_strings();
                viewportModeControl.set_cur_sel_code( m_obj->get_viewport_sampler( t ) );

                UpdateEnables();
            }
            return TRUE;
        case WM_COMMAND: {
            const int id = LOWORD( wParam );
            const int notifyCode = HIWORD( wParam );
            switch( id ) {

            case IDC_FIELD_MESHER_RENDER_SAMPLER:
                if( CBN_SELCHANGE == notifyCode ) {

                    fixed_combo_box renderModeControl( hWnd, IDC_FIELD_MESHER_RENDER_SAMPLER,
                                                       FieldMesher::get_sampler_codes, FieldMesher::get_sampler_name,
                                                       m_renderSamplerIndexToCode );
                    int sel = renderModeControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_render_sampler( sel, t );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
                break;
            case IDC_FIELD_MESHER_VIEWPORT_SAMPLER:
                if( CBN_SELCHANGE == notifyCode ) {

                    fixed_combo_box viewportModeControl( hWnd, IDC_FIELD_MESHER_VIEWPORT_SAMPLER,
                                                         FieldMesher::get_sampler_codes, FieldMesher::get_sampler_name,
                                                         m_viewportSamplerIndexToCode );
                    int sel = viewportModeControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_viewport_sampler( sel, t );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
                break;
            }
        }
        }
        return FALSE;
    }

    void InvalidateUI( HWND hwnd, int element ) {
        if( !hwnd ) {
            return;
        }
        switch( element ) {

        case pb_renderSampler: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int renderMode = m_obj->get_render_sampler( 0 );
                fixed_combo_box renderModeControl( hwnd, IDC_FIELD_MESHER_RENDER_SAMPLER,
                                                   FieldMesher::get_sampler_codes, FieldMesher::get_sampler_name,
                                                   m_renderSamplerIndexToCode );
                renderModeControl.set_cur_sel_code( renderMode );
            }
        } break;
        case pb_viewportSampler: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int viewportMode = m_obj->get_viewport_sampler( 0 );
                fixed_combo_box viewportModeControl( hwnd, IDC_FIELD_MESHER_VIEWPORT_SAMPLER,
                                                     FieldMesher::get_sampler_codes, FieldMesher::get_sampler_name,
                                                     m_viewportSamplerIndexToCode );
                viewportModeControl.set_cur_sel_code( viewportMode );
            }
        } break;
        }
    }
    virtual void DeleteThis() {}
    void Update( TimeValue /*t*/ ) {
        try {
            UpdateEnables();
        } catch( const std::exception& e ) {
            frantic::tstring errmsg =
                _T( "MeshinghDlgProc::Update: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
            FF_LOG( error ) << errmsg << std::endl;
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Error" ), _T( "%s" ), e.what() );
            if( frantic::max3d::is_network_render_server() )
                throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
        }
    }
    void UpdateEnables() {
        if( !m_obj ) {
            return;
        }

        IParamBlock2* pb = m_obj->GetParamBlockByID( field_mesher_param_block );
        if( !pb ) {
            return;
        }
        IParamMap2* pm = pb->GetMap( meshing_param_map );
        if( !pm ) {
            return;
        }
        const HWND hwnd = pm->GetHWnd();
        if( !hwnd ) {
            return;
        }

        pm->Enable( pb_renderResolution, !m_obj->render_using_viewport_settings( 0 ) );
        EnableWindow( GetDlgItem( hwnd, IDC_FIELD_MESHER_RENDER_SAMPLER ),
                      !m_obj->render_using_viewport_settings( 0 ) );
    }
    void set_obj( FieldMesher* obj ) { m_obj = obj; }
};

MeshinghDlgProc* GetMeshingDlgProc() {
    static MeshinghDlgProc meshingDlgProc;
    return &meshingDlgProc;
}

#define WM_UPDATE_LIST ( WM_APP + 0xd3d )
class SrcDlgProc : public ParamMap2UserDlgProc {
    FieldMesher* m_obj;

  public:
    SrcDlgProc()
        : m_obj( 0 ) {}
    virtual ~SrcDlgProc() {}
    virtual INT_PTR DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
        switch( msg ) {
        case WM_INITDIALOG:
            if( IParamBlock2* pb = map->GetParamBlock() ) {
                if( HWND hWndCombo = GetDlgItem( hWnd, IDC_PRTEMBER_SURFACECHANNEL_COMBO ) ) {
                    ComboBox_SetCueBannerText( hWndCombo, _T( "Select a channel ..." ) );

                    std::vector<frantic::tstring> options;

                    if( m_obj )
                        m_obj->get_available_channels( t, options );

                    for( std::vector<frantic::tstring>::const_iterator it = options.begin(), itEnd = options.end();
                         it != itEnd; ++it )
                        ComboBox_AddString( hWndCombo, it->c_str() );

                    const MCHAR* curText = pb->GetStr( pb_surfaceChannel );

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
            if( LOWORD( wParam ) == IDC_PRTEMBER_SURFACECHANNEL_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    TCHAR curText[64];

                    ComboBox_GetText( reinterpret_cast<HWND>( lParam ), curText, 64 );

                    if( IParamBlock2* pb = map->GetParamBlock() )
                        pb->SetValue( pb_surfaceChannel, 0, curText );

                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_PRTEMBER_MENU_BUTTON ) {
                if( HIWORD( wParam ) == BN_CLICKED ) {
                    ReferenceTarget* rtarg = NULL;
                    if( IParamBlock2* pb = map->GetParamBlock() ) {
                        ReferenceMaker* rmaker = pb->GetOwner();
                        if( rmaker && rmaker->IsRefTarget() )
                            rtarg = static_cast<ReferenceTarget*>( rmaker );
                    }

                    frantic::max3d::mxs::expression(
                        _T( "try(::StokeContextMenuStruct.OpenFieldMesherMenu owner)catch()" ) )
                        .bind( _T( "owner" ), rtarg )
                        .evaluate<void>();

                    return TRUE;
                }
            }
            break;
        case WM_UPDATE_LIST: // this should only get called when we change nodes
            if( HWND hWndCombo = GetDlgItem( hWnd, IDC_PRTEMBER_SURFACECHANNEL_COMBO ) ) {
                ComboBox_ResetContent( hWndCombo );

                if( IParamBlock2* pb = map->GetParamBlock() ) {
                    std::vector<frantic::tstring> options;
                    options.clear();

                    if( m_obj ) {
                        m_obj->get_available_channels( t, options );
                    }

                    BOOST_FOREACH( const frantic::tstring& option, options )
                        ComboBox_AddString( hWndCombo, option.c_str() );

                    frantic::tstring curText = pb->GetStr( pb_surfaceChannel );
                    openvdb::GridCPtrVec grids;
                    bool hasGrids = m_obj->get_grids( grids, t );
                    // check if the current density channel works in case we've re-selected the same node somehow
                    if( curText == _T( "" ) || ComboBox_FindStringExact( hWndCombo, -1, curText.c_str() ) == CB_ERR ) {
                        if( !options.empty() ) {
                            pb->SetValue( pb_surfaceChannel, t, options.front().c_str() ); // take first available
                                                                                           // option
                            curText = options.front();

                            // override with level set if available
                            if( hasGrids ) {
                                BOOST_FOREACH( const frantic::tstring& option, options ) {
                                    if( is_level_set( grids, option ) ) {
                                        pb->SetValue( pb_surfaceChannel, t, option.c_str() );
                                        curText = option;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if( curText != _T( "" ) ) {
                        int result = ComboBox_FindStringExact( hWndCombo, -1, curText.c_str() );
                        if( result != CB_ERR ) {
                            ComboBox_SetCurSel( hWndCombo, result );
                            if( is_level_set( grids, curText ) ) {
                                pb->SetValue( pb_flipNormals, t, true );
                                HWND check = GetDlgItem( hWnd, IDC_FIELD_MESHER_FLIP_NORMALS );
                                Button_SetCheck( check, true );
                            }
                        }

                        ComboBox_SetText( hWndCombo, curText.c_str() );
                    }
                }
            }

            return TRUE;
        }
        return FALSE;
    }
    virtual void DeleteThis() {}
    void Update( TimeValue /*t*/ ) {
        try {
            UpdateEnables();
        } catch( const std::exception& e ) {
            frantic::tstring errmsg =
                _T( "SrcDlgProc::Update: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
            FF_LOG( error ) << errmsg << std::endl;
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Error" ), _T( "%s" ), e.what() );
            if( frantic::max3d::is_network_render_server() )
                throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
        }
    }
    void UpdateEnables() {
        if( !m_obj ) {
            return;
        }

        IParamBlock2* pb = m_obj->GetParamBlockByID( field_mesher_param_block );
        if( !pb ) {
            return;
        }
        IParamMap2* pm = pb->GetMap( src_param_map );
        if( !pm ) {
            return;
        }
        const HWND hwnd = pm->GetHWnd();
        if( !hwnd ) {
            return;
        }
    }
    void set_obj( FieldMesher* obj ) { m_obj = obj; }

  private:
    bool is_level_set( const openvdb::GridCPtrVec& grids, const frantic::tstring& name ) {
        openvdb::GridBase::ConstPtr currGrid = openvdb::findGridByName( grids, frantic::strings::to_string( name ) );
        return currGrid && ( currGrid->getGridClass() == openvdb::GRID_LEVEL_SET || name == _T( "SignedDistance" ) ||
                             boost::algorithm::starts_with<tstring>( name, _T( "ls_" ) ) );
    }
};

SrcDlgProc* GetSrcDlgProc() {
    static SrcDlgProc srcDlgProc;
    return &srcDlgProc;
}

// The class descriptor for FieldMesher
class FieldMesherDesc : public ClassDesc2 {
    // ParamBlockDesc2 * m_pParamDesc;
    frantic::max3d::ParamBlockBuilder m_pbDesc;

  public:
    FieldMesherDesc()
        : m_pbDesc( field_mesher_param_block, _T( "Parameters" ), 0, 1 ) {

        m_pbDesc.OwnerClassDesc( this );
        m_pbDesc.OwnerRefNum( 0 );
        m_pbDesc.RolloutTemplate( main_param_map, IDD_FIELD_MESHER_MAIN, IDS_FIELD_MESHER_MAIN, GetMainDlgProc() );
        m_pbDesc.RolloutTemplate( src_param_map, IDD_FIELD_MESHER_SOURCE, IDS_PRTEMBER_SOURCE_TITLE, GetSrcDlgProc() );
        m_pbDesc.RolloutTemplate( meshing_param_map, IDD_FIELD_MESHER_MESHING, IDS_FIELD_MESHER_MESHING,
                                  GetMeshingDlgProc() );
        // src
        m_pbDesc.Parameter<INode*>( pb_targetNode, _T( "TargetNode" ), 0 )
            .Validator( FieldMesherValidator::GetInstance() )
            .PickNodeButtonUI( src_param_map, IDC_PRTEMBER_SOURCE_BUTTON, IDS_PRTEMBER_SOURCE_CAPTION );

        m_pbDesc.Parameter<float>( pb_surfaceLevel, _T( "SurfaceLevel" ), IDS_SURFACELEVEL, true )
            .Range( -FLT_MAX, FLT_MAX )
            .DefaultValue( 0.f )
            .SpinnerUI( src_param_map, EDITTYPE_FLOAT, IDC_PRTEMBER_THRESHOLD_EDIT, IDC_PRTEMBER_THRESHOLD_SPIN,
                        boost::optional<float>() );

        m_pbDesc.Parameter<float>( pb_adaptivity, _T( "Adaptivity" ), IDS_FIELD_MESHER_ADAPTIVITY, true )
            .Range( 0, 1 )
            .DefaultValue( 0.f )
            .SpinnerUI( meshing_param_map, EDITTYPE_FLOAT, IDC_FIELD_MESHER_ADAPTIVITY_EDIT,
                        IDC_FIELD_MESHER_ADAPTIVITY_SPIN, boost::optional<float>() );

        m_pbDesc.Parameter<bool>( pb_flipNormals, _T( "FlipNormals" ), IDS_FIELD_MESHER_FLIP_NORMALS )
            .DefaultValue( false )
            .CheckBoxUI( meshing_param_map, IDC_FIELD_MESHER_FLIP_NORMALS );

        m_pbDesc.Parameter<const MCHAR*>( pb_surfaceChannel, _T( "SurfaceChannel" ), 0 ).DefaultValue( _T( "" ) );

        // main
        m_pbDesc.Parameter<bool>( pb_showIcon, _T( "ShowIcon" ), IDS_SHOWICON )
            .DefaultValue( true )
            .CheckBoxUI( main_param_map, IDC_ICON_SHOW );

        m_pbDesc.Parameter<float>( pb_iconSize, _T( "IconSize" ), IDS_ICONSIZE )
            .Units( frantic::max3d::parameter_units::world )
            .DefaultValue( 10.f )
            .Range( 0.f, FLT_MAX )
            .SpinnerUI( main_param_map, EDITTYPE_UNIVERSE, IDC_ICON_SIZE, IDC_ICON_SIZE_SPIN,
                        boost::optional<float>() );

        m_pbDesc
            .Parameter<bool>( pb_writeVelocityMapChannel, _T( "WriteVelocityMapChannel" ), IDS_WRITEVELOCITYMAPCHANNEL )
            .DefaultValue( false )
            .CheckBoxUI( main_param_map, IDC_WRITE_VELOCITY_MAP_CHANNEL );

        m_pbDesc.Parameter<int>( pb_velocityMapChannel, _T( "VelocityMapChannel" ), IDS_VELOCITYMAPCHANNEL )
            .DefaultValue( 2 )
            .Range( 0, 99 )
            .SpinnerUI( main_param_map, EDITTYPE_INT, IDC_VELOCITY_MAP_CHANNEL, IDC_VELOCITY_MAP_CHANNEL_SPIN, 1 );

        m_pbDesc.Parameter<bool>( pb_enableViewportMesh, _T( "EnableViewportMesh" ), IDS_ENABLEVIEWPORTMESH )
            .DefaultValue( true )
            .CheckBoxUI( main_param_map, IDC_ENABLE_IN_VIEWPORT );

        m_pbDesc.Parameter<int>( pb_displayMode, _T( "DisplayMode" ), IDS_DISPLAYMODE )
            .DefaultValue( DEFAULT_DISPLAY_MODE )
            .Range( 0, ( DISPLAY_MODE::COUNT - 1 ) );

        m_pbDesc.Parameter<bool>( pb_applyObjectTransforms, _T( "ApplyObjectTransforms" ), IDS_APPLYOBJECTTRANSFORMS )
            .DefaultValue( false )
            .CheckBoxUI( main_param_map, IDC_APPLY_OBJECT_TRANSFORMS );

        m_pbDesc
            .Parameter<bool>( pb_renderUsingViewportSettings, _T( "RenderUsingViewportSettings" ),
                              IDS_RENDERUSINGVIEWPORTSETTINGS )
            .DefaultValue( true )
            .CheckBoxUI( meshing_param_map, IDC_FIELD_MESHER_RENDER_USING_VIEWPORT_SETTINGS );

        m_pbDesc.Parameter<float>( pb_viewportResolution, _T( "ViewportResolution" ), IDS_FIELDMESHERVIEWPORTRES, true )
            .Range( 0.0001f, 99999 )
            .DefaultValue( 1.f )
            .SpinnerUI( meshing_param_map, EDITTYPE_FLOAT, IDC_FIELD_MESHER_VIEWPORT_RES,
                        IDC_FIELD_MESHER_VIEWPORT_RES_SPIN, boost::optional<float>() );

        m_pbDesc.Parameter<int>( pb_renderSampler, _T( "RenderSampler" ), IDS_FIELDMESHERRENDERSAMPLER )
            .DefaultValue( DEFAULT_SAMPLER )
            .Range( 0, ( SAMPLER_MODE::COUNT - 1 ) );

        m_pbDesc.Parameter<int>( pb_viewportSampler, _T( "ViewportSampler" ), IDS_FIELDMESHERVIEWPORTSAMPLER )
            .DefaultValue( DEFAULT_SAMPLER )
            .Range( 0, ( SAMPLER_MODE::COUNT - 1 ) );

        m_pbDesc.Parameter<float>( pb_renderResolution, _T( "RenderResolution" ), IDS_FIELDMESHERRENDERRES, true )
            .Range( 0.0001f, 99999 )
            .DefaultValue( 1.f )
            .SpinnerUI( meshing_param_map, EDITTYPE_FLOAT, IDC_FIELD_MESHER_RENDER_RES,
                        IDC_FIELD_MESHER_RENDER_RES_SPIN, boost::optional<float>() );

        m_pbDesc.Parameter<float>( pb_displayPercent, _T( "DisplayPercent" ), IDS_DISPLAYPERCENT )
            .DefaultValue( 100 )
            .Range( 0, 100 )
            .SpinnerUI( main_param_map, EDITTYPE_FLOAT, IDC_DISPLAY_PERCENT, IDC_DISPLAY_PERCENT_SPIN,
                        boost::optional<float>() );

        m_pbDesc.Parameter<int>( pb_displayLimit, _T( "DisplayLimit" ), IDS_DISPLAYLIMIT )
            .DefaultValue( 1000 )
            .Range( 0, std::numeric_limits<int>::max() )
            .SpinnerUI( main_param_map, EDITTYPE_INT, IDC_DISPLAY_LIMIT, IDC_DISPLAY_LIMIT_SPIN, 1 );

        m_pbDesc.Parameter<bool>( pb_useDisplayLimit, _T( "UseDisplayLimit" ), IDS_USEDISPLAYLIMIT )
            .DefaultValue( false )
            .CheckBoxUI( main_param_map, IDC_USE_DISPLAY_LIMIT );
    }
    ~FieldMesherDesc();

    int IsPublic() { return TRUE; }
    void* Create( BOOL /*loading = FALSE*/ ) { return new FieldMesher(); }
    const TCHAR* ClassName() { return _T( FieldMesher_DISPLAY_NAME ); }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return FieldMesher_CLASS_ID; }
    const TCHAR* Category() { return _T( "Stoke" ); }

    const TCHAR* InternalName() {
        return _T( FieldMesher_CLASS_NAME );
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( FieldMesher_DISPLAY_NAME ); }
#endif
};

class create_mouse_callback : public CreateMouseCallBack {
  private:
    FieldMesher* m_obj;
    IPoint2 m_sp0;
    Point3 m_p0;

  public:
    int proc( ViewExp* vpt, int msg, int point, int flags, IPoint2 m, Matrix3& mat );
    void SetObj( FieldMesher* obj );
};

int create_mouse_callback::proc( ViewExp* vpt, int msg, int point, int /*flags*/, IPoint2 m, Matrix3& mat ) {
    float r;
    Point3 p1, center;

    if( msg == MOUSE_FREEMOVE ) {
        vpt->SnapPreview( m, m, NULL, SNAP_IN_3D );
    }

    if( !m_obj ) {
        return CREATE_ABORT;
    }

    if( msg == MOUSE_POINT || msg == MOUSE_MOVE ) {
        switch( point ) {
        case 0:
            // m_obj->suspendSnap = TRUE;
            m_sp0 = m;
            m_p0 = vpt->SnapPoint( m, m, NULL, SNAP_IN_3D );
            mat.SetTrans( m_p0 );
            break;
        case 1:
            if( msg == MOUSE_MOVE ) {
                p1 = vpt->SnapPoint( m, m, NULL, SNAP_IN_3D );
                r = ( p1 - m_p0 ).Length();
                IParamBlock2* pb = m_obj->GetParamBlockByID( 0 );
                if( !pb ) {
                    return CREATE_ABORT;
                }
                pb->SetValue( pb_iconSize, 0, 2.f * r );
            } else if( msg == MOUSE_POINT ) {
                // m_obj->suspendSnap=FALSE;
                return CREATE_STOP;
            }
            break;
        }
    }

    if( msg == MOUSE_ABORT ) {
        return CREATE_ABORT;
    }

    return TRUE;
}
void create_mouse_callback::SetObj( FieldMesher* obj ) { m_obj = obj; }
static create_mouse_callback g_createMouseCB;

FieldMesherDesc::~FieldMesherDesc() {}

ClassDesc* GetFieldMesherClassDesc() {
    static FieldMesherDesc theDesc;
    return &theDesc;
}

static void notify_render_preeval( void* param, NotifyInfo* info ) {
    FieldMesher* pFieldMesher = (FieldMesher*)param;
    TimeValue* pTime = (TimeValue*)info->callParam;
    if( pFieldMesher && pTime ) {
        pFieldMesher->SetRenderTime( *pTime );
        pFieldMesher->SetEmptyValidityAndNotifyDependents();
    }
}

static void notify_post_renderframe( void* param, NotifyInfo* info ) {
    FieldMesher* pFieldMesher = (FieldMesher*)param;
    RenderGlobalContext* pContext = (RenderGlobalContext*)info->callParam;
    if( pFieldMesher && pContext ) {
        pFieldMesher->ClearRenderTime();
    }
}

std::vector<int> FieldMesher::m_displayModeDisplayCodes =
    list_of( DISPLAY_MODE::MESH )( DISPLAY_MODE::BOX )( DISPLAY_MODE::VERTICES )( DISPLAY_MODE::FACES );
std::vector<int> FieldMesher::m_samplerModeDisplayCodes = list_of( SAMPLER_MODE::LINEAR )( SAMPLER_MODE::QUADRATIC );
std::map<int, frantic::tstring> FieldMesher::m_displayModeNames;
std::map<int, frantic::tstring> FieldMesher::m_samplerModeNames;

frantic::tstring get_code_name( const std::map<int, frantic::tstring>& codeToNameMap, int code ) {
    std::map<int, frantic::tstring>::const_iterator i = codeToNameMap.find( code );
    if( i == codeToNameMap.end() ) {
        return _T( "<unknown:" ) + boost::lexical_cast<frantic::tstring>( code ) + _T( ">" );
    } else {
        return i->second;
    }
}

frantic::tstring FieldMesher::get_display_mode_name( int code ) { return get_code_name( m_displayModeNames, code ); }
frantic::tstring FieldMesher::get_sampler_name( int code ) { return get_code_name( m_samplerModeNames, code ); }
const std::vector<int>& FieldMesher::get_display_mode_codes() { return m_displayModeDisplayCodes; }
const std::vector<int>& FieldMesher::get_sampler_codes() { return m_samplerModeDisplayCodes; }
bool FieldMesher::render_using_viewport_settings( TimeValue t ) const {
    return frantic::max3d::get<bool>( pblock2, pb_renderUsingViewportSettings, t );
}
bool FieldMesher::get_show_icon( TimeValue t ) const { return frantic::max3d::get<bool>( pblock2, pb_showIcon, t ); }
float FieldMesher::get_icon_size( TimeValue t ) const { return frantic::max3d::get<float>( pblock2, pb_iconSize, t ); }
bool FieldMesher::get_keep_mesh_in_memory( TimeValue t ) const {
    return frantic::max3d::get<bool>( pblock2, pb_keepMeshInMemory, t );
}
int FieldMesher::get_output_velocity_channel( TimeValue t ) const {
    return frantic::max3d::get<int>( pblock2, pb_velocityMapChannel, t );
}
bool FieldMesher::get_vel_to_map_channel( TimeValue t ) const {
    return frantic::max3d::get<bool>( pblock2, pb_writeVelocityMapChannel, t );
}
bool FieldMesher::get_enable_viewport_mesh( TimeValue t ) const {
    return frantic::max3d::get<bool>( pblock2, pb_enableViewportMesh, t );
}
int FieldMesher::get_render_sampler( TimeValue t ) const {
    return frantic::max3d::get<int>( pblock2, pb_renderSampler, t );
}
int FieldMesher::get_viewport_sampler( TimeValue t ) const {
    return frantic::max3d::get<int>( pblock2, pb_viewportSampler, t );
}
void FieldMesher::set_display_mode( int val, TimeValue t ) { frantic::max3d::set( pblock2, pb_displayMode, t, val ); }
void FieldMesher::set_render_sampler( int val, TimeValue t ) {
    frantic::max3d::set( pblock2, pb_renderSampler, t, val );
}
void FieldMesher::set_viewport_sampler( int val, TimeValue t ) {
    frantic::max3d::set( pblock2, pb_viewportSampler, t, val );
}
bool FieldMesher::use_flip_normals( TimeValue t ) const {
    return frantic::max3d::get<bool>( pblock2, pb_flipNormals, t );
}
bool FieldMesher::in_object_space( TimeValue t ) const {
    return frantic::max3d::get<bool>( pblock2, pb_applyObjectTransforms, t );
}

int FieldMesher::get_display_mode( TimeValue t ) const {
    if( m_inRenderingMode )
        return DISPLAY_MODE::MESH;

    return frantic::max3d::get<int>( pblock2, pb_displayMode, t );
}

FieldMesher::StaticInitializer::StaticInitializer() {
    m_displayModeNames.clear();
    m_displayModeNames[DISPLAY_MODE::MESH] = DISPLAY_MODE_MESH;
    m_displayModeNames[DISPLAY_MODE::BOX] = DISPLAY_MODE_BOX;
    m_displayModeNames[DISPLAY_MODE::VERTICES] = DISPLAY_MODE_VERTICES;
    m_displayModeNames[DISPLAY_MODE::FACES] = DISPLAY_MODE_FACES;

    m_samplerModeNames.clear();
    m_samplerModeNames[SAMPLER_MODE::QUADRATIC] = SAMPLER_MODE_QUADRATIC;
    m_samplerModeNames[SAMPLER_MODE::LINEAR] = SAMPLER_MODE_LINEAR;
}

FieldMesher::StaticInitializer FieldMesher::m_staticInitializer;

// --- Required Implementations ------------------------------

// Constructor
#pragma warning( push, 3 )
#pragma warning( disable : 4355 ) // 'this' : used in base member initializer list
FieldMesher::FieldMesher()
    : pblock2( 0 )
    , m_cachedPolymesh3( 0 )
    ,

    m_inRenderingMode( false )
    , m_renderTime( TIME_NegInfinity )
    ,

    m_cachedPolymesh3IsTriangles( true )
    , m_doneBuildNormals( false ) {
    ivalid.SetEmpty();
    GetFieldMesherClassDesc()->MakeAutoParamBlocks( this );
    InitializeFPDescriptor();

    RegisterNotification( notify_render_preeval, (void*)this, NOTIFY_RENDER_PREEVAL );
    RegisterNotification( notify_post_renderframe, (void*)this, NOTIFY_POST_RENDERFRAME );
}
#pragma warning( pop )

FieldMesher::~FieldMesher() {
    UnRegisterNotification( notify_post_renderframe, (void*)this, NOTIFY_POST_RENDERFRAME );
    UnRegisterNotification( notify_render_preeval, (void*)this, NOTIFY_RENDER_PREEVAL );

    DeleteAllRefsFromMe();
};

// Animatable methods
void FieldMesher::DeleteThis() { delete this; }
Class_ID FieldMesher::ClassID() { return FieldMesher_CLASS_ID; }

#if MAX_VERSION_MAJOR < 24
void FieldMesher::GetClassName( MSTR& s ) { s = GetFieldMesherClassDesc()->ClassName(); }
#else
void FieldMesher::GetClassName( MSTR& s, bool localized ) { s = GetFieldMesherClassDesc()->ClassName(); }
#endif

int FieldMesher::NumSubs() { return 1; }
Animatable* FieldMesher::SubAnim( int i ) { return ( i == 0 ) ? pblock2 : 0; }
#if MAX_VERSION_MAJOR < 24
MSTR FieldMesher::SubAnimName( int i ) { return ( i == 0 ) ? pblock2->GetLocalName() : _T( "" ); }
#else
MSTR FieldMesher::SubAnimName( int i, bool localized ) { return ( i == 0 ) ? pblock2->GetLocalName() : _T( "" ); }
#endif
int FieldMesher::NumParamBlocks() { return 1; }
IParamBlock2* FieldMesher::GetParamBlock( int i ) {
    switch( i ) {
    case 0:
        return pblock2;
    default:
        return 0;
    }
}
IParamBlock2* FieldMesher::GetParamBlockByID( BlockID id ) {
    if( id == pblock2->ID() ) {
        return pblock2;
    } else {
        return 0;
    }
}
void FieldMesher::BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev ) {
    editObj = this;
    GetSrcDlgProc()->set_obj( this );
    GetMainDlgProc()->set_obj( this );
    GetMeshingDlgProc()->set_obj( this );
    GetFieldMesherClassDesc()->BeginEditParams( ip, this, flags, prev );
}
void FieldMesher::EndEditParams( IObjParam* ip, ULONG flags, Animatable* next ) {
    GetFieldMesherClassDesc()->EndEditParams( ip, this, flags, next );
    GetMainDlgProc()->set_obj( 0 );
    GetSrcDlgProc()->set_obj( 0 );
    GetMeshingDlgProc()->set_obj( 0 );
    editObj = 0;
}
ReferenceTarget* FieldMesher::Clone( RemapDir& remap ) {
    FieldMesher* oldObj = this;
    FieldMesher* newObj = new FieldMesher();
    for( int i = 0, iEnd = newObj->NumRefs(); i < iEnd; ++i ) {
        newObj->ReplaceReference( i, remap.CloneRef( oldObj->GetReference( i ) ) );
    }
    oldObj->BaseClone( oldObj, newObj, remap );
    return newObj;
}
void FieldMesher::FreeCaches() {
    ivalid.SetEmpty();
    clear_cache();
}

int FieldMesher::RenderBegin( TimeValue /*t*/, ULONG flags ) {
    try {
        // Only switch to rendering mode if it's not in the material editor
        if( ( flags & RENDERBEGIN_IN_MEDIT ) == 0 ) {
            m_inRenderingMode = true;
            ivalid = NEVER;
            invalidate();
        }
    } catch( const std::exception& e ) {
        const frantic::tstring errmsg = _T( "RenderBegin: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
    }
    return 1;
}

int FieldMesher::RenderEnd( TimeValue /*t*/ ) {
    m_inRenderingMode = false;
    invalidate();
    return 1;
}

BaseInterface* FieldMesher::GetInterface( Interface_ID id ) {
    if( id == FieldMesher_INTERFACE_ID ) {
        return static_cast<frantic::max3d::fpwrapper::FFMixinInterface<FieldMesher>*>( this );
    } else {
        return GeomObject::GetInterface( id );
    }
}

// From ReferenceMaker
int FieldMesher::NumRefs() { return 1; }

RefTargetHandle FieldMesher::GetReference( int i ) { // return (RefTargetHandle)pblock;}
    if( i == 0 ) {
        return pblock2;
    } else {
        return NULL;
    }
}

void FieldMesher::SetReference( int i, RefTargetHandle rtarg ) {
    if( i == 0 ) {
        pblock2 = static_cast<IParamBlock2*>( rtarg );
    }
}

IOResult FieldMesher::Load( ILoad* iload ) { return IO_OK; }

#if MAX_VERSION_MAJOR >= 17
RefResult FieldMesher::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& partID,
                                         RefMessage message, BOOL /*propagate*/ ) {
#else
RefResult FieldMesher::NotifyRefChanged( Interval /*changeInt*/, RefTargetHandle hTarget, PartID& partID,
                                         RefMessage message ) {
#endif
    switch( message ) {
    case REFMSG_CHANGE:
        if( hTarget == pblock2 ) {
            int param = pblock2->LastNotifyParamID();
            if( param == pb_targetNode ) {
                if( !( partID & PART_GEOM ) )
                    return REF_STOP;

                if( IParamMap2* map = pblock2->GetMap( src_param_map ) )
                    SendMessage( map->GetHWnd(), WM_UPDATE_LIST, 0, 0 );
            }
        }
        ivalid.SetEmpty();
        if( editObj == this ) {
            reinterpret_cast<ClassDesc2*>( GetFieldMesherClassDesc() )->InvalidateUI();
        }
        return ( REF_SUCCEED );
    case REFMSG_GET_PARAM_DIM: {
        GetParamDim* gpd = (GetParamDim*)partID;
        if( pblock2 && gpd->index < pblock2->NumParams() ) {
            gpd->dim = pblock2->GetParamDef( static_cast<ParamID>( gpd->index ) ).dim;
        } else {
            gpd->dim = defaultDim; // GetParameterDim( gpd->index );
        }
    }
#if MAX_VERSION_MAJOR >= 12
        return REF_HALT;
#else
        return REF_STOP;
#endif
    // TODO : I think we shouldn't need this, but parameters don't appear without it.
    case REFMSG_GET_PARAM_NAME: {
        GetParamName* gpd = (GetParamName*)partID;
        TCHAR* s = 0;
        if( pblock2 && gpd->index < pblock2->NumParams() ) {
            const StringResID stringID = pblock2->GetParamDef( static_cast<ParamID>( gpd->index ) ).local_name;
            if( stringID ) {
                s = GetString( static_cast<UINT>( stringID ) );
            }
        }
        if( s ) {
            gpd->name = TSTR( s );
        } else {
            gpd->name = TSTR( _T( "" ) );
        }
    }
#if MAX_VERSION_MAJOR >= 12
        return REF_HALT;
#else
        return REF_STOP;
#endif
    }
    return ( REF_SUCCEED );
}

CreateMouseCallBack* FieldMesher::GetCreateMouseCallBack() {
    g_createMouseCB.SetObj( this );
    return &g_createMouseCB;
}

// Display, GetWorldBoundBox, GetLocalBoundBox are virtual methods of SimpleObject2 that must be implemented.
// HitTest is optional.
int FieldMesher::Display( TimeValue time, INode* inode, ViewExp* pView, int flags ) {
    try {
        if( !inode || !pView )
            return 0;

        if( inode->IsNodeHidden() ) {
            return TRUE;
        }

        UpdateMesh( time );

        GraphicsWindow* gw = pView->getGW();
        const DWORD rndLimits = gw->getRndLimits();

        // gw->setRndLimits( GW_Z_BUFFER|GW_WIREFRAME );
        Matrix3 tm = inode->GetNodeTM( time );
        tm.PreTranslate( inode->GetObjOffsetPos() );
        PreRotateMatrix( tm, inode->GetObjOffsetRot() );
        ApplyScaling( tm, inode->GetObjOffsetScale() );
        gw->setTransform( tm );
        const int displayMode = get_display_mode( time );

        if( displayMode == DISPLAY_MODE::BOX ) {
            const color3f fillColor = color3f::from_RGBA( inode->GetWireColor() );
            const color3f lineColor = inode->Selected() ? color3f( &from_max_t( GetSelColor() )[0] ) : fillColor;
            gw->setColor( LINE_COLOR, lineColor.r, lineColor.g, lineColor.b );
            frantic::max3d::DrawBox( gw, frantic::max3d::to_max_t( m_meshBoundingBox ) );
        } else {
            // Let the SimpleObject2 display code do its thing
#if MAX_VERSION_MAJOR >= 14
            if( !MaxSDK::Graphics::IsRetainedModeEnabled() ||
                MaxSDK::Graphics::IsRetainedModeEnabled() && displayMode == DISPLAY_MODE::VERTICES ) {
#else
            {
#endif
                if( displayMode == DISPLAY_MODE::VERTICES ) {
                    gw->setRndLimits( GW_Z_BUFFER | GW_VERT_TICKS );
                } else {
                    build_normals();
                }
                Rect damageRect;
                if( flags & USE_DAMAGE_RECT )
                    damageRect = pView->GetDammageRect();
                if( has_triangle_mesh() ) {
                    mesh.render( gw, inode->Mtls(), ( flags & USE_DAMAGE_RECT ) ? &damageRect : NULL, COMP_ALL,
                                 inode->NumMtls() );
                } else {
                    mm.render( gw, inode->Mtls(), ( flags & USE_DAMAGE_RECT ) ? &damageRect : NULL, COMP_ALL,
                               inode->NumMtls() );
                }
            }
        }

        gw->setRndLimits( GW_Z_BUFFER | GW_WIREFRAME );

        // Render the icon if necessary
        if( get_show_icon( time ) && !( gw->getRndMode() & GW_BOX_MODE ) && !inode->GetBoxMode() ) {
            Matrix3 iconMatrix = inode->GetNodeTM( time );
            float f = get_icon_size( time );
            iconMatrix.Scale( Point3( f, f, f ) );

            gw->setTransform( iconMatrix );

            // This wireframe drawing function is easier to use, because we don't have to mess with the material and
            // stuff.
            gw->setRndLimits( GW_WIREFRAME | ( GW_Z_BUFFER & rndLimits ) );
            max3d::draw_mesh_wireframe(
                gw, m_pIconMesh.get(), inode->Selected() ? color3f( 1 ) : color3f::from_RGBA( inode->GetWireColor() ) );
        }

        gw->setRndLimits( rndLimits );

        return TRUE;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T( "Display: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Object Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );

        return 0;
    }
}

bool is_hardware_hit_testing( ViewExp* vpt ) {
#if MAX_VERSION_MAJOR >= 15
    return MaxSDK::Graphics::IsHardwareHitTesting( vpt );
#else
    UNREFERENCED_PARAMETER( vpt );
    return false;
#endif
}

int FieldMesher::HitTest( TimeValue time, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt ) {
    try {
        GraphicsWindow* gw = vpt->getGW();

        if( ( ( flags & HIT_SELONLY ) && !inode->Selected() ) || gw == NULL || inode == NULL )
            return 0;

        const DWORD rndLimits = gw->getRndLimits();

        const int displayMode = get_display_mode( time );

        HitRegion hitRegion;
        MakeHitRegion( hitRegion, type, crossing, 4, p );
        gw->setHitRegion( &hitRegion );
        gw->clearHitCode();

        if( gw->getRndMode() & GW_BOX_MODE ) {
            gw->setRndLimits( rndLimits & ~( DWORD( GW_ILLUM | GW_FLAT | GW_TEXTURE | GW_CONSTANT ) ) | GW_PICK );

            Matrix3 tm = inode->GetNodeTM( time );
            gw->setTransform( tm );

            Box3 box;
            GetLocalBoundBox( time, inode, vpt, box );
            const int hit = frantic::max3d::hit_test_box( gw, box, hitRegion, flags & HIT_ABORTONHIT );
            gw->setRndLimits( rndLimits );
            return hit;
        }

        if( displayMode == DISPLAY_MODE::BOX ) {
            gw->setRndLimits( rndLimits & ~( DWORD( GW_ILLUM | GW_FLAT | GW_TEXTURE | GW_CONSTANT ) ) | GW_PICK );

            Matrix3 tm = inode->GetNodeTM( time );
            gw->setTransform( tm );

            Box3 box = frantic::max3d::to_max_t( m_meshBoundingBox );
            const int hit = frantic::max3d::hit_test_box( gw, box, hitRegion, flags & HIT_ABORTONHIT );
            if( hit ) {
                gw->setRndLimits( rndLimits );
                return hit;
            }
        } else if( displayMode == DISPLAY_MODE::VERTICES || !is_hardware_hit_testing( vpt ) ) {
            if( displayMode == DISPLAY_MODE::VERTICES ) {
                gw->setRndLimits( GW_VERT_TICKS | GW_Z_BUFFER );
            }

            gw->setTransform( inode->GetObjectTM( time ) );

            if( has_triangle_mesh() ) {
                if( mesh.select( gw, inode->Mtls(), &hitRegion, flags & HIT_ABORTONHIT, inode->NumMtls() ) ) {
                    gw->setRndLimits( rndLimits );
                    return true;
                }
            } else {
                if( mm.select( gw, inode->Mtls(), &hitRegion, flags & HIT_ABORTONHIT, inode->NumMtls() ) ) {
                    gw->setRndLimits( rndLimits );
                    return true;
                }
            }
        }

        // Hit test against the icon if necessary
        if( get_show_icon( time ) ) {
            gw->setRndLimits( rndLimits & ~( DWORD( GW_ILLUM | GW_FLAT | GW_TEXTURE | GW_CONSTANT ) ) | GW_WIREFRAME |
                              GW_PICK );
            gw->setHitRegion( &hitRegion );
            gw->clearHitCode();

            Matrix3 iconMatrix = inode->GetNodeTM( time );
            float f = get_icon_size( time );
            iconMatrix.Scale( Point3( f, f, f ) );

            gw->setTransform( iconMatrix );
            if( m_pIconMesh->select( gw, NULL, &hitRegion ) ) {
                gw->setRndLimits( rndLimits );
                return true;
            }
        }

        gw->setRndLimits( rndLimits );
        return false;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T( "HitTest: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Object Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );

        return false;
    }
}

#if MAX_VERSION_MAJOR >= 17

unsigned long FieldMesher::GetObjectDisplayRequirement() const {
    return MaxSDK::Graphics::ObjectDisplayRequireLegacyDisplayMode;
}

#elif MAX_VERSION_MAJOR >= 14

bool FieldMesher::RequiresSupportForLegacyDisplayMode() const { return true; }

#endif

#if MAX_VERSION_MAJOR >= 17

bool FieldMesher::PrepareDisplay( const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext ) {
    try {
        int displayMode = get_display_mode( 0 );

        BuildMesh( prepareDisplayContext.GetDisplayTime() );
        mRenderItemHandles.ClearAllRenderItems();

        mm.InvalidateHardwareMesh( 1 );

        if( displayMode == DISPLAY_MODE::MESH || displayMode == DISPLAY_MODE::FACES ) {
            if( has_triangle_mesh() ) {
                if( mesh.getNumVerts() > 0 ) {
                    MaxSDK::Graphics::GenerateMeshRenderItemsContext generateRenderItemsContext;
                    generateRenderItemsContext.GenerateDefaultContext( prepareDisplayContext );

                    MaxSDK::Graphics::IMeshDisplay2* pMeshDisplay = static_cast<MaxSDK::Graphics::IMeshDisplay2*>(
                        mesh.GetInterface( IMesh_DISPLAY2_INTERFACE_ID ) );
                    if( !pMeshDisplay ) {
                        return false;
                    }
                    pMeshDisplay->PrepareDisplay( generateRenderItemsContext );

                    return true;
                }
            } else {
                return PreparePolyObjectDisplay( mm, prepareDisplayContext );
            }
        }

        return true;

    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T( "PrepareDisplay: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Object Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }

    return false;
}

bool FieldMesher::UpdatePerNodeItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                      MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                      MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer ) {
    try {

        int displayMode = get_display_mode( 0 );

        mm.InvalidateHardwareMesh( 1 );

        if( displayMode == DISPLAY_MODE::MESH || displayMode == DISPLAY_MODE::FACES ) {
            if( has_triangle_mesh() ) {
                if( mesh.getNumVerts() > 0 ) {
                    MaxSDK::Graphics::IMeshDisplay2* pMeshDisplay = static_cast<MaxSDK::Graphics::IMeshDisplay2*>(
                        mesh.GetInterface( IMesh_DISPLAY2_INTERFACE_ID ) );
                    if( !pMeshDisplay ) {
                        return false;
                    }

                    MaxSDK::Graphics::GenerateMeshRenderItemsContext generateRenderItemsContext;
                    generateRenderItemsContext.GenerateDefaultContext( updateDisplayContext );
                    generateRenderItemsContext.RemoveInvisibleMeshElementDescriptions( nodeContext.GetRenderNode() );

                    pMeshDisplay->GetRenderItems( generateRenderItemsContext, nodeContext, targetRenderItemContainer );

                    return true;
                }
            } else {
                return UpdatePolyObjectPerNodeItemsDisplay( mm, updateDisplayContext, nodeContext,
                                                            targetRenderItemContainer );
            }
        }
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T( "UpdatePerNodeItems: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Object Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }

    return false;
}

#elif MAX_VERSION_MAJOR >= 15

bool FieldMesher::UpdateDisplay( const MaxSDK::Graphics::MaxContext& /*maxContext*/,
                                 const MaxSDK::Graphics::UpdateDisplayContext& displayContext ) {
    try {
        int displayMode = get_display_mode( 0 );

        BuildMesh( displayContext.GetDisplayTime() );

        if( displayMode == DISPLAY_MODE::MESH || displayMode == DISPLAY_MODE::FACES ) {
            if( has_triangle_mesh() ) {
                if( mesh.getNumVerts() > 0 ) {
                    MaxSDK::Graphics::GenerateMeshRenderItemsContext generateRenderItemsContext;
                    generateRenderItemsContext.GenerateDefaultContext( displayContext );
                    mesh.GenerateRenderItems( mRenderItemHandles, generateRenderItemsContext );
                    return true;
                }
            } else {
                if( mm.numv > 0 ) {
                    MaxSDK::Graphics::GenerateMeshRenderItemsContext generateRenderItemsContext;
                    generateRenderItemsContext.GenerateDefaultContext( displayContext );
                    mm.GenerateRenderItems( mRenderItemHandles, generateRenderItemsContext );
                    return true;
                }
            }
        }

        // If we did not set a mesh above, then clear the existing mesh
        mRenderItemHandles.ClearAllRenderItems();
        return true;

    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T( "UpdateDisplay: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Object Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
    return false;
}

#elif MAX_VERSION_MAJOR == 14

bool FieldMesher::UpdateDisplay( unsigned long renderItemCategories,
                                 const MaxSDK::Graphics::MaterialRequiredStreams& materialRequiredStreams,
                                 TimeValue t ) {
    try {
        int displayMode = get_display_mode( 0 );
        BuildMesh( t );

        if( displayMode == DISPLAY_MODE::MESH || displayMode == DISPLAY_MODE::FACES ) {
            if( has_triangle_mesh() ) {
                MaxSDK::Graphics::GenerateRenderItems( mRenderItemHandles, &mesh, renderItemCategories,
                                                       materialRequiredStreams );
            } else {
                MaxSDK::Graphics::GenerateRenderItems( mRenderItemHandles, &mm, renderItemCategories,
                                                       materialRequiredStreams );
            }
        } else {
            mRenderItemHandles.removeAll();
        }

        return true;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T( "UpdateDisplay: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Object Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
    return false;
}

#endif

// from Object
ObjectState FieldMesher::Eval( TimeValue t ) {
    UpdateMesh( t );
    return ObjectState( this );
}

Interval FieldMesher::ObjectValidity( TimeValue /*t*/ ) {
    // UpdateMesh( t );
    return ivalid;
}

void FieldMesher::InitNodeName( MSTR& s ) { s = _T( FieldMesher_DISPLAY_NAME ); }

int FieldMesher::CanConvertToType( Class_ID obtype ) {
    if( obtype == defObjectClassID )
        return TRUE;
    if( obtype == mapObjectClassID )
        return TRUE;
    if( obtype == polyObjectClassID )
        return TRUE;
    if( obtype == triObjectClassID )
        return TRUE;
#ifndef NO_PATCHES
    if( obtype == patchObjectClassID )
        return TRUE;
#endif
    if( Object::CanConvertToType( obtype ) )
        return TRUE;
    if( CanConvertTriObject( obtype ) )
        return TRUE;
    return FALSE;
}

Object* FieldMesher::ConvertToType( TimeValue t, Class_ID obtype ) {
    // TODO : do we ever need to build normals?
    // I believe we don't for Mesh, because the sdk docs state that operator= does not copy rVerts
    if( obtype == triObjectClassID ) {
        UpdateMesh( t );
        TriObject* triob;
        triob = CreateNewTriObject();
        if( has_triangle_mesh() ) {
            triob->GetMesh() = mesh;
            triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        } else {
            mm.OutToTri( triob->GetMesh() );
            triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        }
        return triob;
    }
    if( obtype == defObjectClassID || obtype == mapObjectClassID ) {
        if( has_triangle_mesh() ) {
            TriObject* triob;
            UpdateMesh( t );
            triob = CreateNewTriObject();
            triob->GetMesh() = mesh;
            triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
            return triob;
        } else {
            PolyObject* polyob;
            polyob = new PolyObject;
            polyob->GetMesh() = mm;
            polyob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            polyob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
            return polyob;
        }
    }
    if( obtype == polyObjectClassID ) {
        PolyObject* polyob;
        UpdateMesh( t );
        if( has_triangle_mesh() ) {
            TriObject* triob;
            UpdateMesh( t );
            triob = CreateNewTriObject();
            triob->GetMesh() = mesh;
            triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
            polyob = static_cast<PolyObject*>( triob->ConvertToType( t, polyObjectClassID ) );
        } else {
            polyob = new PolyObject;
            polyob->GetMesh() = mm;
            polyob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            polyob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        }
        return polyob;
    }
#ifndef NO_PATCHES
    if( obtype == patchObjectClassID ) {
        UpdateMesh( t );
        PatchObject* patchob = new PatchObject();
        if( has_triangle_mesh() ) {
            patchob->patch = mesh;
        } else {
            Mesh* tempMesh = new class Mesh;
            mm.OutToTri( *tempMesh );
            patchob->patch = *tempMesh;
            delete tempMesh;
        }
        patchob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
        patchob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        return patchob;
    }
#endif
    if( Object::CanConvertToType( obtype ) ) {
        UpdateMesh( t );
        return Object::ConvertToType( t, obtype );
    }
    if( CanConvertTriObject( obtype ) ) {
        UpdateMesh( t );
        TriObject* triob = CreateNewTriObject();
        triob->GetMesh() = mesh;
        triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
        triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        Object* ob = triob->ConvertToType( t, obtype );
        if( ob != triob ) {
            triob->DeleteThis();
        }
        return ob;
    }
    return NULL;
}

BOOL FieldMesher::PolygonCount( TimeValue t, int& numFaces, int& numVerts ) {
    try {
        UpdateMesh( t );
        if( has_triangle_mesh() ) {
            numFaces = mesh.numFaces;
            numVerts = mesh.numVerts;
        } else {
            numFaces = mm.numf;
            numVerts = mm.numv;
        }
        return TRUE;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T( "PolygonCount: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Object Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
    numFaces = 0;
    numVerts = 0;
    return 0;
}

int FieldMesher::IntersectRay( TimeValue t, Ray& ray, float& at, Point3& norm ) {
    UpdateMesh( t );
    build_normals();
    return mesh.IntersectRay( ray, at, norm );
}

void FieldMesher::GetWorldBoundBox( TimeValue time, INode* inode, ViewExp* vpt, Box3& box ) {
    GetLocalBoundBox( time, inode, vpt, box );
    box = box * inode->GetNodeTM( time );
}

void FieldMesher::GetLocalBoundBox( TimeValue t, INode* /*inode*/, ViewExp* /*vpt*/, Box3& box ) {
    try {
        UpdateMesh( t );

        box = frantic::max3d::to_max_t( m_meshBoundingBox );

        if( get_show_icon( t ) ) {
            // Compute the world-space scaled bounding box
            float scale = get_icon_size( t );
            Matrix3 iconMatrix( 1 );
            iconMatrix.Scale( Point3( scale, scale, scale ) );
            box += m_pIconMesh->getBoundingBox( &iconMatrix );
        }

    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T( "GetLocalBoundBox: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Object Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

void FieldMesher::GetDeformBBox( TimeValue t, Box3& box, Matrix3* tm, BOOL /*useSel*/ ) {
    UpdateMesh( t );
    box.Init();
    if( tm ) {
        bool done = false;

        if( has_triangle_mesh() ) {
            if( mesh.numVerts > 0 ) {
                box = mesh.getBoundingBox( tm );
                done = true;
            }
        } else {
            if( mm.numv > 0 ) {
                box = mm.getBoundingBox( tm );
                done = true;
            }
        }

        if( !done && !m_meshBoundingBox.is_empty() ) {
            for( int i = 0; i < 8; ++i ) {
                box += frantic::max3d::to_max_t( m_meshBoundingBox.get_corner( i ) ) * ( *tm );
            }
        }
    } else {
        if( !m_meshBoundingBox.is_empty() ) {
            box = frantic::max3d::to_max_t( m_meshBoundingBox );
        }
    }
}

Mesh* FieldMesher::GetRenderMesh( TimeValue t, INode* /*inode*/, View& /*view*/, BOOL& needDelete ) {
    UpdateMesh( t );
    if( has_triangle_mesh() ) {
        needDelete = FALSE;
        return &mesh;
    } else {
        needDelete = TRUE;
        Mesh* result = new class Mesh;
        mm.OutToTri( *result );
        return result;
    }
}

//--- FieldMesher methods -------------------------------

void FieldMesher::UpdateMesh( TimeValue t ) {
    if( !ivalid.InInterval( t ) ) {
        BuildMesh( t );
    }
}

INode* FieldMesher::get_inode() {
    INode* inode = 0;

    RefTargetHandle target = this; // m_parent.get_target()
    INode* rootNode = GetCOREInterface()->GetRootNode();

    // try finding the node in the scene
    if( !inode ) {
        inode = frantic::max3d::get_inode( rootNode, target );
    }

    // try finding the node in xref'ed scenes
    if( !inode ) {
        const int xrefFileCount = rootNode->GetXRefFileCount();
        for( int i = 0; i < xrefFileCount && !inode; ++i ) {
            INode* xrefRootNode = rootNode->GetXRefTree( i );
            if( xrefRootNode ) {
                inode = frantic::max3d::get_inode( xrefRootNode, target );
            }
        }
    }

    if( !inode ) {
        throw std::runtime_error( "get_inode: Could not find a match for myself in the scene." );
    }

    return inode;
}
/**
 * Gets the existing grids from the target node. Returns false if the node hasn't been selected, the grids have a
 * bounding box with area==0 (this produces an error), or if the node doesn't have any grids.
 *
 * @param t the current time
 * @param outTransformed tells if the grids have been transformed to object space
 * @param outChannels the vector of channel names to populate
 */
bool FieldMesher::get_grids( openvdb::GridCPtrVec& outGrids, bool& outTransformed, TimeValue t ) {
    INode* pFieldNode = NULL;
    outGrids.clear();
    outTransformed = false;

    pFieldNode = pblock2->GetINode( pb_targetNode );
    if( pFieldNode == NULL )
        return false;

    ObjectState os = pFieldNode->EvalWorldState( t );

    if( boost::shared_ptr<ember::max3d::EmberPipeObject> pPipeObj =
            frantic::max3d::safe_convert_to_type<ember::max3d::EmberPipeObject>( os.obj, t,
                                                                                 EmberPipeObject_CLASSID ) ) {
        frantic::graphics::boundbox3f bounds = frantic::max3d::from_max_t( pPipeObj->GetBounds() );
        if( bounds.minimum() == bounds.maximum() || bounds.is_empty() )
            return false;

        float spacing = pPipeObj->GetDefaultSpacing();

        openvdb::CoordBBox coordBBox = ember::get_ijk_inner_bounds( bounds.minimum(), bounds.maximum(), spacing );
        openvdb::math::Transform::Ptr xform = openvdb::math::Transform::createLinearTransform( spacing );

        ember::convert_to_grids( outGrids, pPipeObj->GetFuture()->get(), xform, coordBBox );
        outTransformed = !( pPipeObj->GetInWorldSpace() );
        return true;
    }
    return false;
}

bool FieldMesher::get_grids( openvdb::GridCPtrVec& outGrids, TimeValue t ) {
    bool unused;
    return get_grids( outGrids, unused, t );
}
/**
 * Gets the current channels from the target node where arity==1. (Used to populate density channel dropdown)
 *
 * @param t the current time
 * @param outChannels the vector of channel names to populate
 */
void FieldMesher::get_available_channels( TimeValue t, std::vector<frantic::tstring>& outChannels ) {
    outChannels.clear();
    INode* pFieldNode = NULL;
    pFieldNode = pblock2->GetINode( pb_targetNode );
    if( pFieldNode != NULL ) {
        ObjectState os = pFieldNode->EvalWorldState( t );

        if( boost::shared_ptr<ember::max3d::EmberPipeObject> pPipeObj =
                frantic::max3d::safe_convert_to_type<ember::max3d::EmberPipeObject>( os.obj, t,
                                                                                     EmberPipeObject_CLASSID ) ) {
            const frantic::channels::channel_map& map = pPipeObj->GetChannels();

            for( std::size_t i = 0, iEnd = map.channel_count(); i < iEnd; ++i ) {
                if( map[i].arity() == 1 && frantic::channels::is_channel_data_type_float( map[i].data_type() ) )
                    outChannels.push_back( map[i].name() );
            }
        }
    }
}

/**
 * Checks the tree at ijk and gets the corresponding data. If the value is non-active its weight and data will be zeroed
 * out and 0 will be returned. For use in sampling channels.
 *
 * @param tree the tree to check
 * @param ijk the coordinates to check at
 * @param outWeight the weight at ijk
 * @param outData the data at ijk
 * @param outSumWeights the sum of its original value and the weight at ijk if it is active
 */
template <typename TreeType, typename ValueType>
bool probe_tree( const TreeType& tree, const openvdb::Coord& ijk, float& outWeight, ValueType& outData,
                 float& outSumWeights ) {
    if( tree.probeValue( ijk, outData ) ) {
        outSumWeights += outWeight;
        return 1;
    }

    outData = openvdb::zeroVal<ValueType>();
    outWeight = 0;
    return 0;
}

/**
 * Samples the value at inCoord. If any of the surrounding values are inactive they are disregarded.
 *
 * @param the tree to sample from
 * @param inCoord the coordinate to sample at
 */
template <typename ValueType, typename TreeType>
ValueType sample_value( const TreeType& tree, const openvdb::Vec3R& inCoord ) {
    const openvdb::Vec3i inIdx = openvdb::tools::local_util::floorVec3( inCoord );
    openvdb::Coord ijk( inIdx );
    const openvdb::Vec3R uvw = inCoord - inIdx;
    float deltas[] = { (float)uvw.x(), (float)uvw.y(), (float)uvw.z() };
    float weights[8];
    ValueType data[8];
    ValueType result = openvdb::zeroVal<ValueType>();

    frantic::volumetrics::levelset::get_trilerp_weights( deltas, weights );

    float sumWeights = 0;
    int found = 0;
    found += probe_tree( tree, ijk, weights[0], data[0], sumWeights );

    ijk[2] += 1; // i, j, k + 1
    found += probe_tree( tree, ijk, weights[4], data[4], sumWeights );

    ijk[1] += 1; // i, j+1, k + 1
    found += probe_tree( tree, ijk, weights[6], data[6], sumWeights );

    ijk[2] -= 1; // i, j+1, k
    found += probe_tree( tree, ijk, weights[2], data[2], sumWeights );

    ijk[0] += 1;
    ijk[1] -= 1; // i+1, j, k
    found += probe_tree( tree, ijk, weights[1], data[1], sumWeights );

    ijk[2] += 1; // i+1, j, k + 1
    found += probe_tree( tree, ijk, weights[5], data[5], sumWeights );

    ijk[1] += 1; // i+1, j+1, k + 1
    found += probe_tree( tree, ijk, weights[7], data[7], sumWeights );

    ijk[2] -= 1; // i+1, j+1, k
    found += probe_tree( tree, ijk, weights[3], data[3], sumWeights );

    if( found != 0 ) {
        if( sumWeights > 1e-20f ) {
            for( int i = 0; i < 8; i++ ) {
                result += data[i] * ( weights[i] / sumWeights );
            }
        } else {
            float weight = 1 / found;
            for( int i = 0; i < 8; i++ ) {
                result += data[i] * weight; // probe_tree will have zeroed out any non-active data values
            }
        }
        return result;
    }
    return openvdb::zeroVal<ValueType>();
}

/**
 * Populates a pre-existing channel in cachedPolymesh3 from a grid
 *
 * @param grid the grid the channel is being created from
 * @param outMesh the mesh the channel is being added to
 * @param name the channel name
 */
template <typename GridType>
void add_channel_impl( openvdb::GridBase::ConstPtr grid, frantic::geometry::polymesh3_ptr outMesh,
                       const std::string& name ) {
    polymesh3_const_vertex_accessor<vector3f> verts = outMesh->get_const_vertex_accessor<vector3f>( _T( "verts" ) );
    polymesh3_vertex_accessor<void> channel = outMesh->get_vertex_accessor( frantic::strings::to_tstring( name ) );

    GridType::ConstPtr theGrid = openvdb::gridConstPtrCast<GridType>( grid );

    const GridType::TreeType& tree = theGrid->constTree();

    for( std::size_t i = 0; i < verts.vertex_count(); ++i ) {
        frantic::graphics::vector3f vec = verts.get_vertex( i );
        GridType::ValueType result = sample_value<typename GridType::ValueType>(
            tree, theGrid->transform().worldToIndex( openvdb::Vec3R( vec.x, vec.y, vec.z ) ) );

        memcpy( channel.get_vertex( i ), &result, sizeof( result ) );
    }
}

/**
 * Creates a channel for vector types
 *
 * @param name the channel name
 * @param outMesh the mesh the channel is being added to
 */
template <typename GridType>
void make_channel_vector( const std::string& name, frantic::geometry::polymesh3_ptr outMesh ) {
    outMesh->add_empty_vertex_channel(
        frantic::strings::to_tstring( name ),
        frantic::channels::channel_data_type_traits<GridType::ValueType::ValueType>::data_type(),
        GridType::ValueType::size );
}

/**
 * Creates a channel for scalar types
 *
 * @param name the channel name
 * @param outMesh the mesh the channel is being added to
 */
template <typename GridType>
void make_channel_scalar( const std::string& name, frantic::geometry::polymesh3_ptr outMesh ) {
    outMesh->add_empty_vertex_channel( frantic::strings::to_tstring( name ),
                                       frantic::channels::channel_data_type_traits<GridType::ValueType>::data_type(),
                                       1 );
}

/**
 * Creates an additional channel for a pre-made mesh held in m_cachedPolymesh3
 *
 * @param grid the grid the channel will be created from
 */
void FieldMesher::add_channel( openvdb::GridBase::ConstPtr grid ) {
    std::string name = grid->getName();

    if( ( grid )->isType<openvdb::Vec3SGrid>() ) {
        make_channel_vector<openvdb::Vec3SGrid>( name, m_cachedPolymesh3 );
        add_channel_impl<typename openvdb::Vec3SGrid>( grid, m_cachedPolymesh3, name );
    }
    if( ( grid )->isType<openvdb::Vec3IGrid>() ) {
        make_channel_vector<openvdb::Vec3IGrid>( name, m_cachedPolymesh3 );
        add_channel_impl<typename openvdb::Vec3IGrid>( grid, m_cachedPolymesh3, name );
    }
    if( ( grid )->isType<openvdb::Vec3DGrid>() ) {
        make_channel_vector<openvdb::Vec3DGrid>( name, m_cachedPolymesh3 );
        add_channel_impl<typename openvdb::Vec3DGrid>( grid, m_cachedPolymesh3, name );
    } else if( ( grid )->isType<openvdb::FloatGrid>() ) {
        make_channel_scalar<openvdb::FloatGrid>( name, m_cachedPolymesh3 );
        add_channel_impl<typename openvdb::FloatGrid>( grid, m_cachedPolymesh3, name );
    } else if( ( grid )->isType<openvdb::DoubleGrid>() ) {
        make_channel_scalar<openvdb::DoubleGrid>( name, m_cachedPolymesh3 );
        add_channel_impl<typename openvdb::DoubleGrid>( grid, m_cachedPolymesh3, name );
    } else if( ( grid )->isType<openvdb::Int32Grid>() ) {
        make_channel_scalar<openvdb::Int32Grid>( name, m_cachedPolymesh3 );
        add_channel_impl<typename openvdb::Int32Grid>( grid, m_cachedPolymesh3, name );
    } else if( ( grid )->isType<openvdb::Int64Grid>() ) {
        make_channel_scalar<openvdb::Int64Grid>( name, m_cachedPolymesh3 );
        add_channel_impl<typename openvdb::Int64Grid>( grid, m_cachedPolymesh3, name );
    }
}

class LoggerUpdater {
  private:
    const FieldMesher* m_mesher;
    boost::shared_ptr<frantic::logging::progress_logger> m_logger;
    float m_progress;

  public:
    LoggerUpdater( const FieldMesher* fieldMesher, frantic::tstring title )
        : m_mesher( fieldMesher )
        , m_logger( m_mesher->is_in_viewport()
                        ? boost::shared_ptr<frantic::logging::progress_logger>(
                              new frantic::max3d::logging::status_panel_progress_logger( 0, 100, 0, title, true ) )
                        : boost::shared_ptr<frantic::logging::progress_logger>(
                              new frantic::logging::null_progress_logger() ) )
        , m_progress( 0 ) {}

    void update_progress() { m_logger->update_progress( m_progress ); }
    void set_progress( float progress, const frantic::tstring& title ) {
        m_progress = progress;
        m_logger->set_title( title );
    }
};

class ThreadInterrupter {
  public:
    ThreadInterrupter() {}

    void start( const char* name = NULL ) {}

    void end() {}

    inline bool wasInterrupted( int percent = -1 ) { return boost::this_thread::interruption_requested(); }
};

template <typename SamplerType, typename GridType, typename GridConstPtr>
GridConstPtr resample( GridConstPtr inGrid, float resolution ) {

    if( !( SamplerType::consistent() && frantic::math::relative_equal_to( resolution, 1.f, 0.0005f ) ) ) {
        GridType::Ptr outGrid = GridType::create();
        openvdb::math::Transform::Ptr transform = inGrid->transform().copy();
        transform->preScale( openvdb::Vec3d( 1 / resolution ) );
        outGrid->setTransform( transform );
        ThreadInterrupter interrupt;
        openvdb::tools::doResampleToMatch<SamplerType>( *inGrid, *outGrid, interrupt );

        return outGrid;
    }

    return inGrid;
}

template <typename GridType, typename GridConstPtr>
GridConstPtr resample( openvdb::GridBase::ConstPtr densityGrid, float resolution, int sampler ) {

    if( sampler == SAMPLER_MODE::QUADRATIC ) {
        return resample<openvdb::tools::QuadraticSampler, GridType>( openvdb::gridConstPtrCast<GridType>( densityGrid ),
                                                                     resolution );
    } else if( sampler == SAMPLER_MODE::LINEAR ) {
        return resample<openvdb::tools::BoxSampler, GridType>( openvdb::gridConstPtrCast<GridType>( densityGrid ),
                                                               resolution );
    }
    return openvdb::gridConstPtrCast<GridType>( densityGrid );
}

bool FieldMesher::is_in_viewport() const { return !m_inRenderingMode && !frantic::max3d::is_network_render_server(); }

void FieldMesher::build_mesh( TimeValue t, LoggerUpdater* logger, frantic::tstring& outError ) {
    try {

        openvdb::initialize();
        std::vector<openvdb::Vec3s> points;
        std::vector<openvdb::Vec3I> triangles;
        std::vector<openvdb::Vec4I> quads;
        frantic::geometry::polymesh3_builder builder;
        openvdb::GridCPtrVec grids;
        tstring density = pblock2->GetStr( pb_surfaceChannel );
        openvdb::GridBase::ConstPtr densityGrid = NULL;
        bool transformed = false;
        boost::this_thread::interruption_point();
        if( get_grids( grids, transformed, t ) ) {

            // use user selection
            if( density != _T( "" ) ) {
                densityGrid = openvdb::findGridByName( grids, frantic::strings::to_string( density ) );
            }

            boost::this_thread::interruption_point();
            logger->set_progress( 5, _T( "Sampling" ) );

            if( densityGrid != NULL ) {
                if( get_display_mode( t ) != DISPLAY_MODE::BOX ) {

                    bool useRender = m_inRenderingMode && !render_using_viewport_settings( t );
                    int sampler = useRender ? get_render_sampler( t ) : get_viewport_sampler( t );
                    const float resolution = useRender ? pblock2->GetFloat( pb_renderResolution, t )
                                                       : pblock2->GetFloat( pb_viewportResolution, t );

                    if( densityGrid->isType<openvdb::FloatGrid>() ) {
                        densityGrid = resample<openvdb::FloatGrid, openvdb::FloatGrid::ConstPtr>( densityGrid,
                                                                                                  resolution, sampler );

                    } else if( densityGrid->isType<openvdb::DoubleGrid>() ) {
                        densityGrid = resample<openvdb::DoubleGrid, openvdb::DoubleGrid::ConstPtr>(
                            densityGrid, resolution, sampler );
                    }
                }

                boost::this_thread::interruption_point();
                logger->set_progress( 50, _T( "Converting to Mesh" ) );

                if( ( densityGrid )->isType<openvdb::FloatGrid>() ) {
                    openvdb::tools::volumeToMesh<openvdb::FloatGrid>(
                        *openvdb::gridConstPtrCast<openvdb::FloatGrid>( densityGrid ), points, triangles, quads,
                        pblock2->GetFloat( pb_surfaceLevel, t ), pblock2->GetFloat( pb_adaptivity, t ) );
                } else if( ( densityGrid )->isType<openvdb::DoubleGrid>() ) {
                    openvdb::tools::volumeToMesh<openvdb::DoubleGrid>(
                        *openvdb::gridConstPtrCast<openvdb::DoubleGrid>( densityGrid ), points, triangles, quads,
                        pblock2->GetFloat( pb_surfaceLevel, t ), pblock2->GetFloat( pb_adaptivity, t ) );
                }
            }
        }

        boost::this_thread::interruption_point();

        if( get_display_mode( t ) != DISPLAY_MODE::BOX ) {
            logger->set_progress( 70, _T( "Building Mesh" ) );

            for( std::vector<openvdb::Vec3s>::iterator i = points.begin(); i != points.end(); i++ ) {
                builder.add_vertex( ( *i ).x(), ( *i ).y(), ( *i ).z() );
                boost::this_thread::interruption_point();
            }
            for( std::vector<openvdb::Vec3I>::iterator i = triangles.begin(); i != triangles.end(); i++ ) {
                int face[] = { i->x(), i->y(), i->z() };
                for( int j = 0; j < 3; j++ ) {
                    if( face[j] < 0 || face[j] >= points.size() )
                        throw std::invalid_argument( "Field Mesher Error: Index out of range." );
                }
                builder.add_polygon( face, 3 );
                boost::this_thread::interruption_point();
            }
            for( std::vector<openvdb::Vec4I>::iterator i = quads.begin(); i != quads.end(); i++ ) {
                int face[] = { i->x(), i->y(), i->z(), i->w() };
                for( int j = 0; j < 4; j++ ) {
                    if( face[j] < 0 || face[j] >= points.size() )
                        throw std::invalid_argument( "Field Mesher Error: Index out of range." );
                }
                builder.add_polygon( face, 4 );
                boost::this_thread::interruption_point();
            }

            m_cachedPolymesh3 = builder.finalize();
            m_cachedPolymesh3IsTriangles = false;

            if( use_flip_normals( t ) ) {
                frantic::geometry::reverse_face_winding( m_cachedPolymesh3 );
            }

            logger->set_progress( 80, _T( "Retrieving Channels" ) );
            if( densityGrid != NULL ) {
                for( openvdb::GridCPtrVecIter g = grids.begin(); g != grids.end(); g++ ) {
                    if( to_tstring( ( *g )->getName() ) != density )
                        add_channel( *g );
                    boost::this_thread::interruption_point();
                }
            }

            if( get_vel_to_map_channel( t ) && m_cachedPolymesh3->has_channel( _T( "Velocity" ) ) ) {
                m_cachedPolymesh3->copy_channel(
                    frantic::max3d::geometry::get_map_channel_name( get_output_velocity_channel( t ) ),
                    _T( "Velocity" ) );
            }
            boost::this_thread::interruption_point();
            logger->set_progress( 85, _T( "Readying Display " ) );

            if( transformed )
                transform( m_cachedPolymesh3, pblock2->GetINode( pb_targetNode )->GetObjectTM( t ) );

            if( !in_object_space( t ) ) {
                frantic::geometry::transform( m_cachedPolymesh3, Inverse( get_inode()->GetObjectTM( t ) ) );
            }

            int displayMode = get_display_mode( t );
            if( displayMode == DISPLAY_MODE::FACES || displayMode == DISPLAY_MODE::VERTICES ) {
                float fraction = pblock2->GetFloat( pb_displayPercent, t ) / 100;
                int limit = std::numeric_limits<int>::max();
                if( frantic::max3d::get<bool>( pblock2, pb_useDisplayLimit, t ) ) {
                    limit = pblock2->GetInt( pb_displayLimit, t );
                }
                mesh_interface_ptr polymesh =
                    frantic::geometry::polymesh3_interface::create_const_instance( m_cachedPolymesh3 );
                mesh_interface_ptr fractional;

                if( displayMode == DISPLAY_MODE::FACES ) {
                    fractional = mesh_interface_ptr(
                        new frantic::geometry::fractional_face_interface( polymesh.get(), fraction, limit ) );
                } else {
                    fractional = mesh_interface_ptr(
                        new frantic::geometry::fractional_vertex_interface( polymesh.get(), fraction, limit ) );
                }
                m_cachedPolymesh3 = create_polymesh3( fractional.get() );
            }

            boost::this_thread::interruption_point();

            m_meshBoundingBox = compute_boundbox( m_cachedPolymesh3 );
            frantic::max3d::geometry::polymesh_copy( mm, m_cachedPolymesh3 );

        } else {

            logger->set_progress( 80, _T( "Building Bounding Box" ) );

            frantic::graphics::boundbox3f tempBox;
            for( std::vector<openvdb::Vec3s>::iterator i = points.begin(); i != points.end(); i++ ) {
                tempBox += frantic::graphics::vector3f( ( *i ).x(), ( *i ).y(), ( *i ).z() );
                boost::this_thread::interruption_point();
            }

            if( transformed )
                tempBox = frantic::max3d::from_max_t( frantic::max3d::to_max_t( tempBox ) *
                                                      pblock2->GetINode( pb_targetNode )->GetObjectTM( t ) );

            if( !in_object_space( t ) ) {
                tempBox = frantic::max3d::from_max_t( frantic::max3d::to_max_t( tempBox ) *
                                                      Inverse( get_inode()->GetObjectTM( t ) ) );
            }

            boost::this_thread::interruption_point();
            clear_polymesh( mm );
            m_cachedPolymesh3.reset();
            m_meshBoundingBox = tempBox;
        }

    } catch( const boost::thread_interrupted& e ) {
        m_cachedPolymesh3.reset();
        m_cachedPolymesh3IsTriangles = true;
        m_meshBoundingBox.set_to_empty();
        m_doneBuildNormals = false;
        clear_mesh( mesh );
        clear_polymesh( mm );

    } catch( const std::exception& e ) {
        m_cachedPolymesh3.reset();
        m_cachedPolymesh3IsTriangles = true;
        m_meshBoundingBox.set_to_empty();
        m_doneBuildNormals = false;
        clear_mesh( mesh );
        clear_polymesh( mm );

        outError = frantic::strings::to_tstring( e.what() ) + _T( "\n" );

    } catch( ... ) {
        outError = _T( "An unknown error occured. \n" );
    }
}

/**
 * Creates the mesh for MAX to display for the specified time
 *
 * @param time the time to build a mesh for
 */
void FieldMesher::BuildMesh( TimeValue t ) {
    try {
        // If we've got a valid mesh, just return.
        if( ivalid.InInterval( t ) )
            return;
        ivalid.SetInstant( t );

        if( get_enable_viewport_mesh( t ) ) {
            LoggerUpdater logger( this, _T( "Getting Grids" ) );
            frantic::tstring errmsg;
            boost::thread meshBuilder( &FieldMesher::build_mesh, this, t, &logger, boost::ref( errmsg ) );

            while( !meshBuilder.timed_join( boost::posix_time::milliseconds( 100 ) ) ) {
                try {
                    logger.update_progress();

                } catch( const frantic::logging::progress_cancel_exception& e ) {
                    meshBuilder.interrupt();
                    logger.set_progress( 0, _T( "Cancelling BuildMesh" ) );
                }
            }

            if( !errmsg.empty() ) {
                throw std::runtime_error( frantic::strings::to_string( errmsg ) );
            }
        } else {
            m_cachedPolymesh3.reset();
            m_cachedPolymesh3IsTriangles = true;
            m_meshBoundingBox.set_to_empty();
            m_doneBuildNormals = false;
            clear_mesh( mesh );
            clear_polymesh( mm );
        }
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T( "FieldMesher::BuildMesh (" ) + get_node_name() + _T( ") Error: " ) +
                                  frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Object Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() ) {
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
        }
    }
}

/**
 * This is just a wrapper for NotifyDependents
 */
void FieldMesher::NotifyEveryone() {
    // Called when we change the object, so it's dependents get informed
    // and it redraws, etc.
    NotifyDependents( FOREVER, (PartID)PART_ALL, REFMSG_CHANGE );
}

/**
 * Reset the cached mesh info and force a reload immediately
 */
void FieldMesher::invalidate() {
    ivalid.SetEmpty();
    NotifyEveryone();
}

void FieldMesher::clear_trimesh3_cache() {
    m_cachedPolymesh3.reset();
    m_cachedPolymesh3IsTriangles = true;
}

/**
 * Clears all cached mesh data, including the max mesh.
 */
void FieldMesher::clear_cache() {
    clear_mesh( mesh );
    clear_polymesh( mm );
    clear_trimesh3_cache();
}

void FieldMesher::clear_cache_mxs() {
    try {
        clear_cache();
        invalidate();
    } catch( std::exception& e ) {
        frantic::tstring errmsg = _T( "ClearCache: " ) + frantic::strings::to_tstring( e.what() ) + _T( "\n" );
        FF_LOG( error ) << errmsg << std::endl;
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T( "Field Mesher Error" ), _T( "%s" ), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

void FieldMesher::report_warning( const frantic::tstring& msg ) {
    try {
        FF_LOG( warning ) << msg << std::endl;
    } catch( std::exception& /*e*/ ) {
    }
}

void FieldMesher::report_error( const frantic::tstring& msg ) {
    try {
        FF_LOG( error ) << msg << std::endl;
    } catch( std::exception& /*e*/ ) {
    }
}

void FieldMesher::SetRenderTime( TimeValue t ) { m_renderTime = t; }

void FieldMesher::ClearRenderTime() { m_renderTime = TIME_NegInfinity; }

void FieldMesher::SetEmptyValidityAndNotifyDependents() {
    ivalid.SetEmpty();
    NotifyEveryone();
}

TimeValue FieldMesher::GetRenderTime() const { return m_renderTime; }

bool FieldMesher::HasValidRenderTime() const { return m_renderTime != TIME_NegInfinity; }

bool FieldMesher::has_triangle_mesh() { return m_cachedPolymesh3IsTriangles; }

void FieldMesher::build_normals() {
    if( !m_doneBuildNormals ) {
        if( has_triangle_mesh() ) {
            mesh.buildNormals();
        } else {
            mm.buildNormals();
        }
        m_doneBuildNormals = true;
    }
}

frantic::tstring FieldMesher::get_node_name() {
    frantic::tstring name = _T( "<error>" );

    ULONG handle = 0;
    this->NotifyDependents( FOREVER, (PartID)&handle, REFMSG_GET_NODE_HANDLE );

    INode* node = 0;
    if( handle ) {
        node = GetCOREInterface()->GetINodeByHandle( handle );
    }
    if( node ) {
        const TCHAR* pName = node->GetName();
        if( pName ) {
            name = pName;
        } else {
            name = _T( "<null>" );
        }
    } else {
        name = _T( "<missing>" );
    }
    return name;
}

//---------------------------------------------------------------------------
// Function publishing setup

void FieldMesher::InitializeFPDescriptor() {
    FFCreateDescriptor c( this, FieldMesher_INTERFACE_ID, _T( "FieldMesherInterface" ), GetFieldMesherClassDesc() );

    c.add_function( &FieldMesher::clear_cache_mxs, _T( "ClearCache" ) );
}

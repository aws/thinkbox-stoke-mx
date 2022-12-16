// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource_pflow.h"

#include <stoke/max3d/FieldFollowPFOperator.hpp>
#include <stoke/max3d/ISimEmber.hpp>

#pragma warning( push, 3 )
#include <ParticleFlow/IChannelContainer.h>
#include <ParticleFlow/IParticleChannelAmount.h>
#include <ParticleFlow/IParticleChannelMXSFloat.h>
#include <ParticleFlow/IParticleChannelMXSInteger.h>
#include <ParticleFlow/IParticleChannelMXSVector.h>
#include <ParticleFlow/IParticleChannelMaterialIndex.h>
#include <ParticleFlow/IParticleChannelNew.h>
#include <ParticleFlow/IParticleChannelOrientation.h>
#include <ParticleFlow/IParticleChannelPosition.h>
#include <ParticleFlow/IParticleChannelScale.h>
#include <ParticleFlow/IParticleChannelSelection.h>
#include <ParticleFlow/IParticleChannelShapeTexture.h>
#include <ParticleFlow/IParticleChannelSpeed.h>
#include <ParticleFlow/IParticleChannelSpin.h>
#include <ParticleFlow/PFClassIDs.h>
#include <ParticleFlow/PFMessages.h>
#include <decomp.h>
#pragma warning( pop )

#include <frantic/max3d/fpwrapper/max_typetraits.hpp>
#include <frantic/max3d/max_utility.hpp>
#include <frantic/max3d/volumetrics/IEmberField.hpp>

#include <frantic/channels/channel_map.hpp>
#include <frantic/files/files.hpp>
#include <frantic/files/paths.hpp>
#include <frantic/graphics/boundbox3.hpp>
#include <frantic/logging/logging_level.hpp>
#include <frantic/volumetrics/field_interface.hpp>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>

#define PFOP_FLOOD_VELOCITIES_CLASS_ID Class_ID( 0xc3f7d5b, PFActionClassID_PartB )

#if MAX_VERSION_MAJOR < 15
#define PARAM_END end
#define MAXSTRCONST
#else
#define PARAM_END p_end
#define MAXSTRCONST const
#endif

using namespace std;
using namespace frantic;
using namespace frantic::graphics;
using namespace frantic::max3d;
using namespace frantic::volumetrics;

extern HINSTANCE hInstance;
extern TCHAR* GetString( int );

bool IsNetworkRenderServer() {
    return GetCOREInterface()->IsNetworkRenderServer();
}

namespace ember {
namespace max3d {

//----------------------------------------------------
// Custom parameter dialog callbacks
//	-Used so I can have custom control over the dialog
//----------------------------------------------------

enum {
    kFloodVelocities_velocity_loader_node,
    kFloodVelocities_velocity_blend_percent,
    kFloodVelocities_spin_blend_percent,
    kFloodVelocities_no_blending_with_empty_voxels,
    kVelocityChannel,
    kVelocityBlend,
    kOrientationChannel,
    kOrientationBlend,
    kSpinChannel,
    kSpinBlend,
    kScaleChannel,
    kScaleBlend,
    kSelectedChannel,
    kMtlIDChannel,
    kScriptIntegerChannel,
    kScriptFloatChannel,
    kScriptVectorChannel,
    kColorChannel,
    kTextureChannel,
    kMapChannels
};

enum { kFloodVelocities_pblock_index };

//----------------------------------------------------
// Class Descriptor stuff for exposing the plugin to Max
//----------------------------------------------------
class FloodVelocityLoaderPFOperatorDesc : public ClassDesc2 {
  public:
    static HBITMAP m_depotIcon;
    static HBITMAP m_depotMask;
    ParamBlockDesc2 pblock_desc;
    // static ClassDesc2* theDesc;

    FloodVelocityLoaderPFOperatorDesc();

  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL ) { return new FloodVelocityLoaderPFOperator; }
    const TCHAR* ClassName() { return GetString( IDS_CLASSNAME ); }
    SClass_ID SuperClassID() { return HELPER_CLASS_ID; }
    Class_ID ClassID() { return PFOP_FLOOD_VELOCITIES_CLASS_ID; }
    Class_ID SubClassID() { return PFOperatorSubClassID; }
    const TCHAR* Category() { return GetString( IDS_CATEGORY ); }
    const TCHAR* InternalName() { return GetString( IDS_CLASSNAME ); }
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return GetString( IDS_CLASSNAME ); }
#endif

    INT_PTR Execute( int cmd, ULONG_PTR arg1, ULONG_PTR arg2, ULONG_PTR arg3 );
};

HBITMAP FloodVelocityLoaderPFOperatorDesc::m_depotIcon = NULL;
HBITMAP FloodVelocityLoaderPFOperatorDesc::m_depotMask = NULL;
// ClassDesc2* FloodVelocityLoaderPFOperatorDesc::theDesc = new FloodVelocityLoaderPFOperatorDesc();

ClassDesc2* GetFloodVelocityLoaderPFOperatorDesc() {
    // return FloodVelocityLoaderPFOperatorDesc::theDesc;
    static FloodVelocityLoaderPFOperatorDesc theFloodVelocityLoaderPFOperatorDesc;
    return &theFloodVelocityLoaderPFOperatorDesc;
}

INT_PTR FloodVelocityLoaderPFOperatorDesc::Execute( int cmd, ULONG_PTR arg1, ULONG_PTR arg2, ULONG_PTR /*arg3*/ ) {
    TCHAR** res = NULL;
    bool* isPublic;
    bool* isFertile;
    HBITMAP* depotIcon;
    HBITMAP* depotMask;
    switch( cmd ) {
    case kPF_GetActionDescription:
        if( arg1 == NULL )
            return 0;
        res = (TCHAR**)arg1;
        *res = GetString( IDS_DESCRIPTION );
        break;
    case kPF_PViewPublic:
        if( arg1 == NULL )
            return 0;
        isPublic = (bool*)arg1;
        *isPublic = true;
        break;
    case kPF_PViewCategory:
        if( arg1 == NULL )
            return 0;
        res = (TCHAR**)arg1;
        *res = GetString( IDS_CATEGORY );
        break;
    case kPF_IsFertile:
        if( arg1 == NULL )
            return 0;
        isFertile = (bool*)arg1;
        *isFertile = false;
        break;
    case kPF_PViewDepotIcon:
        if( arg1 == NULL )
            return 0;
        depotIcon = (HBITMAP*)arg1;
        if( arg2 == NULL )
            return 0;
        depotMask = (HBITMAP*)arg2;
        if( m_depotIcon == NULL )
            m_depotIcon = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_DEPOT_ICON ) );
        if( m_depotMask == NULL )
            m_depotMask = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_DEPOT_MASK ) );
        *depotIcon = m_depotIcon;
        *depotMask = m_depotMask;
        break;
    default:
        return 0;
    }
    return 1;
}
//----------------------------------------------------
// End of FloodVelocityLoaderPFOperatorDesc
//----------------------------------------------------

#define WM_EMBERFOLLOW_UPDATELISTS ( WM_USER + 0x4563 )

class EmberFollowDlgProc : public ParamMap2UserDlgProc {
  public:
    static EmberFollowDlgProc* GetInstance() {
        static EmberFollowDlgProc s_instance;
        return &s_instance;
    }

    void init_combobox( HWND comboBox, const std::vector<frantic::tstring>& channelList, const TCHAR* currentValue ) {
        ComboBox_ResetContent( comboBox );

        ComboBox_SetCueBannerText( comboBox, L"Select a channel..." );

        ComboBox_AddString( comboBox, _T("None") );

        for( std::vector<frantic::tstring>::const_iterator it = channelList.begin(), itEnd = channelList.end();
             it != itEnd; ++it ) {
            ComboBox_AddString( comboBox, it->c_str() );
        }

        // Maybe this isn't necessary and calling SetText would suffice?
        if( !currentValue || currentValue[0] == _T( '\0' ) ) { // ie. Empty string
            ComboBox_SetCurSel( comboBox, -1 );                // Clear the selection
        } else {
            int index = ComboBox_FindStringExact( comboBox, 0, currentValue );

            if( index != CB_ERR ) {
                ComboBox_SetCurSel( comboBox, index );
            } else {
                ComboBox_SetText( comboBox, currentValue );
            }
        }
    }

    void init_dialog( HWND hWnd, IParamBlock2* pblock, TimeValue t ) {
        Interval dontCare = FOREVER;

        boost::shared_ptr<frantic::volumetrics::field_interface> field = frantic::max3d::volumetrics::create_field(
            pblock->GetINode( kFloodVelocities_velocity_loader_node ), t, dontCare );

        std::vector<frantic::tstring> channels;

        if( field ) {
            for( std::size_t i = 0, iEnd = field->get_channel_map().channel_count(); i < iEnd; ++i )
                channels.push_back( field->get_channel_map()[i].name() );
        }

        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_VELOCITY_COMBO ), channels,
                       pblock->GetStr( kVelocityChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_ORIENTATION_COMBO ), channels,
                       pblock->GetStr( kOrientationChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_SPIN_COMBO ), channels, pblock->GetStr( kSpinChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_SCALE_COMBO ), channels, pblock->GetStr( kScaleChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_SELECTED_COMBO ), channels,
                       pblock->GetStr( kSelectedChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_VERTEXCOLOR_COMBO ), channels,
                       pblock->GetStr( kColorChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_TEXTURECOORD_COMBO ), channels,
                       pblock->GetStr( kTextureChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_MTLID_COMBO ), channels, pblock->GetStr( kMtlIDChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_SCRIPTINT_COMBO ), channels,
                       pblock->GetStr( kScriptIntegerChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_SCRIPTFLOAT_COMBO ), channels,
                       pblock->GetStr( kScriptFloatChannel ) );
        init_combobox( GetDlgItem( hWnd, IDC_EMBERFOLLOW_SCRIPTVECTOR_COMBO ), channels,
                       pblock->GetStr( kScriptVectorChannel ) );
    }

    void handle_selection( HWND comboBox, IParamBlock2* pblock, ParamID param ) {
        int curSel = ComboBox_GetCurSel( comboBox );
        if( curSel == 0 || curSel == CB_ERR ) {
            pblock->SetValue( param, 0, _T("") );

            ComboBox_SetCurSel( comboBox, -1 ); // Clear the selection if "None" is selected.
        } else {
            int size = ComboBox_GetLBTextLen( comboBox, curSel );

            frantic::tstring result;
            result.resize( size );

            ComboBox_GetLBText( comboBox, curSel, const_cast<frantic::tchar*>( result.c_str() ) );

            pblock->SetValue( param, 0, result.c_str() );
        }
    }

    virtual INT_PTR DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
        IParamBlock2* pblock = map->GetParamBlock();
        if( !pblock )
            return FALSE;

        switch( msg ) {
        case WM_INITDIALOG:
            init_dialog( hWnd, pblock, t );

            break;
        case WM_EMBERFOLLOW_UPDATELISTS:
            /*init_combobox( GetDlgItem(hWnd, IDC_EMBERFOLLOW_VELOCITY_COMBO), *(const
            std::vector<frantic::tstring>*)lParam, pblock->GetStr(kVelocityChannel) ); init_combobox( GetDlgItem(hWnd,
            IDC_EMBERFOLLOW_ORIENTATION_COMBO), *(const std::vector<frantic::tstring>*)lParam,
            pblock->GetStr(kOrientationChannel) ); init_combobox( GetDlgItem(hWnd, IDC_EMBERFOLLOW_SPIN_COMBO), *(const
            std::vector<frantic::tstring>*)lParam, pblock->GetStr(kSpinChannel) ); init_combobox( GetDlgItem(hWnd,
            IDC_EMBERFOLLOW_SCALE_COMBO), *(const std::vector<frantic::tstring>*)lParam, pblock->GetStr(kScaleChannel)
            ); init_combobox( GetDlgItem(hWnd, IDC_EMBERFOLLOW_SELECTED_COMBO), *(const
            std::vector<frantic::tstring>*)lParam, pblock->GetStr(kSelectedChannel) ); init_combobox( GetDlgItem(hWnd,
            IDC_EMBERFOLLOW_MTLID_COMBO), *(const std::vector<frantic::tstring>*)lParam, pblock->GetStr(kMtlIDChannel)
            ); init_combobox( GetDlgItem(hWnd, IDC_EMBERFOLLOW_SCRIPTINT_COMBO), *(const
            std::vector<frantic::tstring>*)lParam, pblock->GetStr(kScriptIntegerChannel) ); init_combobox(
            GetDlgItem(hWnd, IDC_EMBERFOLLOW_SCRIPTFLOAT_COMBO), *(const std::vector<frantic::tstring>*)lParam,
            pblock->GetStr(kScriptFloatChannel) ); init_combobox( GetDlgItem(hWnd, IDC_EMBERFOLLOW_SCRIPTVECTOR_COMBO),
            *(const std::vector<frantic::tstring>*)lParam, pblock->GetStr(kScriptVectorChannel) );*/
            init_dialog( hWnd, pblock, t );

            break;
        case WM_COMMAND:
            if( LOWORD( wParam ) == IDC_EMBERFOLLOW_VELOCITY_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kVelocityChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_ORIENTATION_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kOrientationChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_SPIN_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kSpinChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_SCALE_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kScaleChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_SELECTED_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kSelectedChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_VERTEXCOLOR_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kColorChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_TEXTURECOORD_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kTextureChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_MTLID_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kMtlIDChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_SCRIPTINT_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kScriptIntegerChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_SCRIPTFLOAT_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kScriptFloatChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_SCRIPTVECTOR_COMBO ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    handle_selection( (HWND)lParam, pblock, kScriptVectorChannel );
                    return TRUE;
                }
            } else if( LOWORD( wParam ) == IDC_EMBERFOLLOW_CONTEXTMENU ) {
                if( HIWORD( wParam ) == BN_CLICKED ) {
                    ReferenceTarget* rtarg = NULL;
                    ReferenceMaker* rmaker = pblock->GetOwner();
                    if( rmaker && rmaker->IsRefTarget() )
                        rtarg = static_cast<ReferenceTarget*>( rmaker );

                    frantic::max3d::mxs::expression(
                        _T("try(::StokeContextMenuStruct.OpenFieldFollowMenu owner)catch()") )
                        .bind( _T("owner"), rtarg )
                        .evaluate<void>();
                    return TRUE;
                }
            }
            break;
        }

        return FALSE;
    }

    virtual void DeleteThis() {}
};

//----------------------------------------------------
// Param Block info
//----------------------------------------------------

FloodVelocityLoaderPFOperatorDesc::FloodVelocityLoaderPFOperatorDesc()
    : pblock_desc(
          // required block spec
          kFloodVelocities_pblock_index, _T("Parameters"), IDS_PARAMETERS,
          NULL /*GetFloodVelocityLoaderPFOperatorDesc()*/, P_AUTO_CONSTRUCT + P_AUTO_UI,

          // auto constuct block refno : none
          kFloodVelocities_pblock_index,

          // auto ui parammap specs : none
          IDD_EMBERFOLLOW, IDS_PARAMETERS, 0,
          0, // open/closed

          EmberFollowDlgProc::GetInstance(), // no dialog proc
          PARAM_END ) {
    pblock_desc.SetClassDesc( this );

    pblock_desc.AddParam( kFloodVelocities_velocity_loader_node, _T("FieldNode"), TYPE_INODE, P_RESET_DEFAULT,
                          IDS_VELOCITY_LOADER_NODE, PARAM_END );
    pblock_desc.ParamOption( kFloodVelocities_velocity_loader_node, p_ui, TYPE_PICKNODEBUTTON, IDC_VELOCITY_LOADER_NODE,
                             PARAM_END );

    // pblock_desc.AddParam( kFloodVelocities_velocity_blend_percent, _T("VelocityBlendPercent"), TYPE_PCNT_FRAC,
    // P_ANIMATABLE + P_RESET_DEFAULT, IDS_VELOCITYBLENDPERCENT, PARAM_END ); pblock_desc.ParamOption(
    // kFloodVelocities_velocity_blend_percent, p_default, 1.f, PARAM_END ); pblock_desc.ParamOption(
    // kFloodVelocities_velocity_blend_percent, p_ui, TYPE_SPINNER, EDITTYPE_POS_FLOAT, IDC_VELOCITY_BLEND_PER_FRAME,
    // IDC_VELOCITY_BLEND_PER_FRAME_SPIN, SPIN_AUTOSCALE, PARAM_END );

    pblock_desc.AddParam( kVelocityChannel, _T("VelocityChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kVelocityChannel, p_default, _T("Velocity"), PARAM_END );

    pblock_desc.AddParam( kVelocityBlend, _T("VelocityBlend"), TYPE_PCNT_FRAC, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kVelocityBlend, p_default, 1.f, PARAM_END );
    pblock_desc.ParamOption( kVelocityBlend, p_ui, TYPE_SPINNER, EDITTYPE_POS_FLOAT, IDC_EMBERFOLLOW_VELOCITYBLEND_EDIT,
                             IDC_EMBERFOLLOW_VELOCITYBLEND_SPIN, SPIN_AUTOSCALE, PARAM_END );

    pblock_desc.AddParam( kOrientationChannel, _T("OrientationChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kOrientationChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kOrientationBlend, _T("OrientationBlend"), TYPE_PCNT_FRAC, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kOrientationBlend, p_default, 1.f, PARAM_END );
    pblock_desc.ParamOption( kOrientationBlend, p_ui, TYPE_SPINNER, EDITTYPE_POS_FLOAT,
                             IDC_EMBERFOLLOW_ORIENTATIONBLEND_EDIT, IDC_EMBERFOLLOW_ORIENTATIONBLEND_SPIN,
                             SPIN_AUTOSCALE, PARAM_END );

    pblock_desc.AddParam( kSpinChannel, _T("SpinChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kSpinChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kSpinBlend, _T("SpinBlend"), TYPE_PCNT_FRAC, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kSpinBlend, p_default, 1.f, PARAM_END );
    pblock_desc.ParamOption( kSpinBlend, p_ui, TYPE_SPINNER, EDITTYPE_POS_FLOAT, IDC_EMBERFOLLOW_SPINBLEND_EDIT,
                             IDC_EMBERFOLLOW_SPINBLEND_SPIN, SPIN_AUTOSCALE, PARAM_END );

    pblock_desc.AddParam( kScaleChannel, _T("ScaleChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kScaleChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kScaleBlend, _T("ScaleBlend"), TYPE_PCNT_FRAC, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kScaleBlend, p_default, 1.f, PARAM_END );
    pblock_desc.ParamOption( kScaleBlend, p_ui, TYPE_SPINNER, EDITTYPE_POS_FLOAT, IDC_EMBERFOLLOW_SCALEBLEND_EDIT,
                             IDC_EMBERFOLLOW_SCALEBLEND_SPIN, SPIN_AUTOSCALE, PARAM_END );

    pblock_desc.AddParam( kSelectedChannel, _T("SelectedChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kSelectedChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kMtlIDChannel, _T("MtlIDChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kMtlIDChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kScriptIntegerChannel, _T("ScriptIntegerChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kScriptIntegerChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kScriptFloatChannel, _T("ScriptFloatChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kScriptFloatChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kScriptVectorChannel, _T("ScriptVectorChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kScriptVectorChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kColorChannel, _T("ColorChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kColorChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kTextureChannel, _T("TextureChannel"), TYPE_STRING, 0, 0, PARAM_END );
    pblock_desc.ParamOption( kTextureChannel, p_default, _T(""), PARAM_END );

    pblock_desc.AddParam( kMapChannels, _T("MapChannels"), TYPE_STRING_TAB, 0, P_VARIABLE_SIZE, 0, PARAM_END );
    // pblock_desc.ParamOption( kMapChannels, p_default, _T(""), PARAM_END );
}
//----------------------------------------------------
// End of Param Block info
//----------------------------------------------------

//+>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>+
//|							Function Publishing System
//|
//+>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>+
FPInterfaceDesc floodVelocities_action_FPInterfaceDesc(
    PFACTION_INTERFACE, _T("action"), 0, GetFloodVelocityLoaderPFOperatorDesc(), FP_MIXIN,

    IPFAction::kInit, _T("init"), 0, TYPE_bool, 0, 5, _T("container"), 0, TYPE_IOBJECT, _T("particleSystem"), 0,
    TYPE_OBJECT, _T("particleSystemNode"), 0, TYPE_INODE, _T("actions"), 0, TYPE_OBJECT_TAB_BR, _T("actionNodes"), 0,
    TYPE_INODE_TAB_BR, IPFAction::kRelease, _T("release"), 0, TYPE_bool, 0, 1, _T("container"), 0, TYPE_IOBJECT,
    // reserved for future use
    // IPFAction::kChannelsUsed, _T("channelsUsed"), 0, TYPE_VOID, 0, 2,
    //	_T("interval"), 0, TYPE_INTERVAL_BR,
    //	_T("channels"), 0, TYPE_FPVALUE,
    IPFAction::kActivityInterval, _T("activityInterval"), 0, TYPE_INTERVAL_BV, 0, 0, IPFAction::kIsFertile,
    _T("isFertile"), 0, TYPE_bool, 0, 0, IPFAction::kIsNonExecutable, _T("isNonExecutable"), 0, TYPE_bool, 0, 0,
    IPFAction::kSupportRand, _T("supportRand"), 0, TYPE_bool, 0, 0, IPFAction::kIsMaterialHolder,
    _T("isMaterialHolder"), 0, TYPE_bool, 0, 0, IPFAction::kSupportScriptWiring, _T("supportScriptWiring"), 0,
    TYPE_bool, 0, 0, IPFAction::kGetUseScriptWiring, _T("getUseScriptWiring"), 0, TYPE_bool, 0, 0,
    IPFAction::kSetUseScriptWiring, _T("setUseScriptWiring"), 0, TYPE_VOID, 0, 1, _T("useState"), 0, TYPE_bool,

    PARAM_END );

FPInterfaceDesc floodVelocities_operator_FPInterfaceDesc(
    PFOPERATOR_INTERFACE, _T("operator"), 0, GetFloodVelocityLoaderPFOperatorDesc(), FP_MIXIN,

    IPFOperator::kProceed, _T("proceed"), 0, TYPE_bool, 0, 7, _T("container"), 0, TYPE_IOBJECT, _T("timeStart"), 0,
    TYPE_TIMEVALUE, _T("timeEnd"), 0, TYPE_TIMEVALUE_BR, _T("particleSystem"), 0, TYPE_OBJECT, _T("particleSystemNode"),
    0, TYPE_INODE, _T("actionNode"), 0, TYPE_INODE, _T("integrator"), 0, TYPE_INTERFACE,

    PARAM_END );

FPInterfaceDesc floodVelocities_PViewItem_FPInterfaceDesc(
    PVIEWITEM_INTERFACE, _T("PViewItem"), 0, GetFloodVelocityLoaderPFOperatorDesc(), FP_MIXIN,

    IPViewItem::kNumPViewParamBlocks, _T("numPViewParamBlocks"), 0, TYPE_INT, 0, 0, IPViewItem::kGetPViewParamBlock,
    _T("getPViewParamBlock"), 0, TYPE_REFTARG, 0, 1, _T("index"), 0, TYPE_INDEX, IPViewItem::kHasComments,
    _T("hasComments"), 0, TYPE_bool, 0, 1, _T("actionNode"), 0, TYPE_INODE, IPViewItem::kGetComments, _T("getComments"),
    0, TYPE_STRING, 0, 1, _T("actionNode"), 0, TYPE_INODE, IPViewItem::kSetComments, _T("setComments"), 0, TYPE_VOID, 0,
    2, _T("actionNode"), 0, TYPE_INODE, _T("comments"), 0, TYPE_STRING,

    PARAM_END );

//----------------------------------------------------
// Simple useless functions
//----------------------------------------------------
FloodVelocityLoaderPFOperator::FloodVelocityLoaderPFOperator() { GetClassDesc()->MakeAutoParamBlocks( this ); }

FloodVelocityLoaderPFOperator::~FloodVelocityLoaderPFOperator() {}

Class_ID FloodVelocityLoaderPFOperator::ClassID() { return PFOP_FLOOD_VELOCITIES_CLASS_ID; }

#if MAX_VERSION_MAJOR < 24
void FloodVelocityLoaderPFOperator::GetClassName( TSTR& s ) { s = GetString( IDS_CLASSNAME ); }
#else
void FloodVelocityLoaderPFOperator::GetClassName( TSTR& s, bool localized ) const { s = GetString( IDS_CLASSNAME ); }
#endif

ClassDesc* FloodVelocityLoaderPFOperator::GetClassDesc() const { return GetFloodVelocityLoaderPFOperatorDesc(); }

HBITMAP FloodVelocityLoaderPFOperator::GetActivePViewIcon() {
    if( activeIcon() == NULL )
        _activeIcon() = (HBITMAP)LoadImage( hInstance, MAKEINTRESOURCE( IDB_ACTIVE ), IMAGE_BITMAP, kActionImageWidth,
                                            kActionImageHeight, LR_SHARED );
    return activeIcon();
}

HBITMAP FloodVelocityLoaderPFOperator::GetInactivePViewIcon() {
    if( inactiveIcon() == NULL )
        _inactiveIcon() = (HBITMAP)LoadImage( hInstance, MAKEINTRESOURCE( IDB_INACTIVE ), IMAGE_BITMAP,
                                              kActionImageWidth, kActionImageHeight, LR_SHARED );
    return inactiveIcon();
}

bool FloodVelocityLoaderPFOperator::HasDynamicName( TSTR& nameSuffix ) {
    if( INode* node = m_pblock->GetINode( kFloodVelocities_velocity_loader_node ) )
        nameSuffix = node->GetName();

    return true;
}

FPInterfaceDesc* FloodVelocityLoaderPFOperator::GetDescByID( Interface_ID id ) {
    if( id == PFACTION_INTERFACE )
        return &floodVelocities_action_FPInterfaceDesc;
    if( id == PFOPERATOR_INTERFACE )
        return &floodVelocities_operator_FPInterfaceDesc;
    if( id == PVIEWITEM_INTERFACE )
        return &floodVelocities_PViewItem_FPInterfaceDesc;
    return NULL;
}

void FloodVelocityLoaderPFOperator::BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev ) {
    GetClassDesc()->BeginEditParams( ip, this, flags, prev );
}

void FloodVelocityLoaderPFOperator::EndEditParams( IObjParam* ip, ULONG flags, Animatable* next ) {
    GetClassDesc()->EndEditParams( ip, this, flags, next );
}

const ParticleChannelMask& FloodVelocityLoaderPFOperator::ChannelsUsed( const Interval& /*time*/ ) const {
    static ParticleChannelMask mask(
        PCU_Amount | PCU_New | PCU_Position | PCU_Speed | PCU_Orientation | PCU_Spin | PCU_Scale, // Read
        PCU_Speed | PCU_Orientation | PCU_Spin | PCU_Scale | PCU_Selection | PCU_MtlIndex | PCU_MXSInteger |
            PCU_MXSFloat | PCU_MXSVector // Write
    );

    return mask;
}

const Interval FloodVelocityLoaderPFOperator::ActivityInterval() const { return FOREVER; }

RefTargetHandle FloodVelocityLoaderPFOperator::Clone( RemapDir& remap ) {
    FloodVelocityLoaderPFOperator* newOp = new FloodVelocityLoaderPFOperator();
    newOp->ReplaceReference( 0, remap.CloneRef( pblock() ) );
    BaseClone( this, newOp, remap );
    return newOp;
}

#if MAX_VERSION_MAJOR < 24
MAXSTRCONST TCHAR* FloodVelocityLoaderPFOperator::GetObjectName() { return GetString( IDS_OBJECTNAME ); }
#else
MAXSTRCONST TCHAR* FloodVelocityLoaderPFOperator::GetObjectName( bool localized ) const {
    return GetString( IDS_OBJECTNAME );
}
#endif

void FloodVelocityLoaderPFOperator::PassMessage( int message, LPARAM param ) {
    // is this function needed?
    if( pblock() == NULL )
        return;

    IParamMap2* map = pblock()->GetMap();
    if( ( map == NULL ) && ( NumPViewParamMaps() == 0 ) )
        return;

    HWND hWnd;
    if( map != NULL ) {
        hWnd = map->GetHWnd();
        if( hWnd ) {
            SendMessage( hWnd, WM_COMMAND, message, param );
        }
    }

    for( int i = 0; i < NumPViewParamMaps(); i++ ) {
        hWnd = GetPViewParamMap( i )->GetHWnd();
        if( hWnd ) {
            SendMessage( hWnd, WM_COMMAND, message, param );
        }
    }
}

void NotifyUI( IParamBlock2* pblock, IPViewItem* pview, UINT msg, WPARAM wParam, LPARAM lParam ) {
    // Update the pblock's param map.
    if( pblock != NULL ) {
        if( IParamMap2* map = pblock->GetMap() )
            SendMessage( map->GetHWnd(), msg, wParam, lParam );
    }

    // Update the param maps owned by ParticleFlow.
    for( int i = 0, iEnd = pview->NumPViewParamMaps(); i < iEnd; ++i ) {
        if( IParamMap2* map = pview->GetPViewParamMap( i ) )
            SendMessage( map->GetHWnd(), msg, wParam, lParam );
    }
}

//----------------------------------------------------
// Meaty Important Functions
//----------------------------------------------------
RefResult FloodVelocityLoaderPFOperator::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget,
                                                           PartID& /*partID*/, RefMessage message,
                                                           BOOL /*propagate*/ ) {
    switch( message ) {
    case REFMSG_CHANGE:
        if( hTarget == pblock() ) {
            int lastUpdateID = pblock()->LastNotifyParamID();
            switch( lastUpdateID ) {
            // case kFloodVelocities_velocity_blend_percent:
            // case kFloodVelocities_spin_blend_percent:
            case kFloodVelocities_velocity_loader_node:
                NotifyDependents( FOREVER, 0, kPFMSG_InvalidateParticles, NOTIFY_ALL, TRUE );
                NotifyUI( pblock(), this, WM_EMBERFOLLOW_UPDATELISTS, 0, 0 );
                break;
            }

            UpdatePViewUI( static_cast<ParamID>( lastUpdateID ) );
        }
        break;
    }
    return REF_SUCCEED;
}

inline const TCHAR* get_string( IParamBlock2* pblock, ParamID param, TimeValue t = 0, int tabIndex = 0 ) {
    const TCHAR* result = pblock->GetStr( param, t, tabIndex );
    if( !result )
        result = _T("");
    return result;
}

template <class T>
struct pflow_channel_traits {};

#define PFLOW_CHANNEL_TRAITS( MaxType, realType )                                                                      \
    template <>                                                                                                        \
    struct pflow_channel_traits<realType> {                                                                            \
        typedef IParticleChannel##MaxType##W write_type;                                                               \
        typedef IParticleChannel##MaxType##R read_type;                                                                \
        inline static Class_ID class_id() { return ParticleChannel##MaxType##_Class_ID; }                              \
    };

PFLOW_CHANNEL_TRAITS( Bool, bool );
PFLOW_CHANNEL_TRAITS( Int, int );
PFLOW_CHANNEL_TRAITS( Float, float );
PFLOW_CHANNEL_TRAITS( Point3, Point3 );
PFLOW_CHANNEL_TRAITS( AngAxis, AngAxis );
PFLOW_CHANNEL_TRAITS( Quat, Quat );

template <typename T>
inline void GetPFlowAccessors( IChannelContainer* chCont, Interface_ID readWrapInterface,
                               Interface_ID writeWrapInterface, typename pflow_channel_traits<T>::read_type*& outRead,
                               typename pflow_channel_traits<T>::write_type*& outWrite, bool* outNeedsInit = NULL ) {
    Class_ID cid = pflow_channel_traits<typename T>::class_id();
    outWrite = (typename pflow_channel_traits<T>::write_type*)chCont->EnsureInterface(
        writeWrapInterface, cid, true, readWrapInterface, writeWrapInterface, true, NULL, NULL, outNeedsInit );
    outRead = (typename pflow_channel_traits<T>::read_type*)chCont->GetInterface( readWrapInterface );
}

bool FloodVelocityLoaderPFOperator::Proceed( IObject* pCont, PreciseTimeValue timeStart, PreciseTimeValue& timeEnd,
                                             Object* /*pSystem*/, INode* /*pNode*/, INode* actionNode,
                                             IPFIntegrator* /*integrator*/ ) {
    try {
        INode* node = pblock()->GetINode( kFloodVelocities_velocity_loader_node );
        if( !node )
            return false;

        Interval valid = FOREVER; // TODO: Incorporate this.

        TimeValue timeMiddle = ( timeStart.TimeValue() + timeEnd.TimeValue() ) / 2;

        boost::shared_ptr<frantic::volumetrics::field_interface> field =
            frantic::max3d::volumetrics::create_field( node, timeMiddle, valid );
        if( !field )
            return false;

        // Make sure we can check on the current particles in the container
        IParticleChannelAmountR* chAmountR = GetParticleChannelAmountRInterface( pCont );
        if( chAmountR == NULL )
            return false;

        IParticleChannelNewR* chNew = GetParticleChannelNewRInterface( pCont );
        if( chNew == NULL )
            return false;

        // Get access to ChannelContainer interface
        IChannelContainer* chCont;
        chCont = GetChannelContainerInterface( pCont );
        if( chCont == NULL )
            return false;

        // PFlow takes non-const references, so we can't pass temporary values (rvalues) in.
        Class_ID BoolID = ParticleChannelBool_Class_ID;
        Class_ID IntID = ParticleChannelInt_Class_ID;
        Class_ID FloatID = ParticleChannelFloat_Class_ID;
        Class_ID Point3ID = ParticleChannelPoint3_Class_ID;
        Class_ID AngAxisID = ParticleChannelAngAxis_Class_ID;
        Class_ID QuatID = ParticleChannelQuat_Class_ID;

        Interface_ID PosRID = PARTICLECHANNELPOSITIONR_INTERFACE, PosWID = PARTICLECHANNELPOSITIONW_INTERFACE;

        // Ensure access to reading the position channel
        IParticleChannelPoint3R* chPos =
            (IParticleChannelPoint3R*)chCont->EnsureInterface( PosRID, Point3ID, true, PosRID, PosWID, true );
        if( chPos == NULL )
            return false;

        // Get the fraction of a frame this time step is for
        float frameFraction = float( timeEnd.TimeValue() - timeStart.TimeValue() ) / GetTicksPerFrame();

        // Get the particle count
        int particleCount = chAmountR->Count();

        // If there are no particles to affect, don't bother loading the level set.  This
        // saves a lot of time if there are parts of the system where the event with this operator
        // has no particles in it.
        if( particleCount == 0 )
            return true;

        // Get the transform matrix for the field node. This affects the lookup point, Velocity, Orientation, Spin, and
        // maybe ??Scale??.
        // Matrix3 toWorld = node->GetNodeTM( timeMiddle );
        // Matrix3 fromWorld = Inverse( toWorld );

        frantic::tstring velocityChannelName = get_string( m_pblock, kVelocityChannel );
        frantic::tstring orientationChannelName = get_string( m_pblock, kOrientationChannel );
        frantic::tstring spinChannelName = get_string( m_pblock, kSpinChannel );
        frantic::tstring scaleChannelName = get_string( m_pblock, kScaleChannel );
        frantic::tstring selectedChannelName = get_string( m_pblock, kSelectedChannel );
        frantic::tstring mtlIDChannelName = get_string( m_pblock, kMtlIDChannel );
        frantic::tstring mxsIntChannelName = get_string( m_pblock, kScriptIntegerChannel );
        frantic::tstring mxsFloatChannelName = get_string( m_pblock, kScriptFloatChannel );
        frantic::tstring mxsVectorChannelName = get_string( m_pblock, kScriptVectorChannel );
        frantic::tstring colorChannelName = get_string( m_pblock, kColorChannel );
        frantic::tstring textureChannelName = get_string( m_pblock, kTextureChannel );

        IParticleChannelPoint3W* chSpeedW = NULL;
        IParticleChannelPoint3R* chSpeedR = NULL;

        frantic::channels::channel_cvt_accessor<vector3f> velocityChannel;
        float velocityBlendAlpha;

        if( field->get_channel_map().has_channel( velocityChannelName ) &&
            field->get_channel_map()[velocityChannelName].arity() == 3 &&
            frantic::channels::is_channel_data_type_float(
                field->get_channel_map()[velocityChannelName].data_type() ) ) {
            velocityChannel =
                field->get_channel_map().get_cvt_accessor<frantic::graphics::vector3f>( velocityChannelName );
            velocityBlendAlpha = pblock()->GetFloat( kVelocityBlend, timeMiddle );
            if( velocityBlendAlpha > 0 && velocityBlendAlpha < 1.f )
                velocityBlendAlpha =
                    1.f -
                    std::pow( 1.f - velocityBlendAlpha,
                              frameFraction ); // Found by expanding 1, 2, 3 & 4 subdivisions and finding the pattern.

            if( velocityBlendAlpha > 0.f ) {
                GetPFlowAccessors<Point3>( chCont, PARTICLECHANNELSPEEDR_INTERFACE, PARTICLECHANNELSPEEDW_INTERFACE,
                                           chSpeedR, chSpeedW );
                if( !chSpeedR )
                    velocityBlendAlpha = 1.f;
            }
        }

        IParticleChannelQuatW* chOrientW = NULL;
        IParticleChannelQuatR* chOrientR = NULL;

        frantic::channels::channel_cvt_accessor<frantic::graphics::quat4f> orientationAcc(
            frantic::graphics::quat4f::from_identity() );
        float orientationBlendAlpha;

        if( field->get_channel_map().has_channel( orientationChannelName ) &&
            field->get_channel_map()[orientationChannelName].arity() == 4 &&
            frantic::channels::is_channel_data_type_float(
                field->get_channel_map()[orientationChannelName].data_type() ) ) {
            orientationAcc =
                field->get_channel_map().get_cvt_accessor<frantic::graphics::quat4f>( orientationChannelName );
            orientationBlendAlpha = pblock()->GetFloat( kOrientationBlend, timeMiddle );
            if( orientationBlendAlpha > 0 && orientationBlendAlpha < 1.f )
                orientationBlendAlpha =
                    1.f -
                    std::pow( 1.f - orientationBlendAlpha,
                              frameFraction ); // Found by expanding 1, 2, 3 & 4 subdivisions and finding the pattern.

            if( orientationBlendAlpha > 0.f ) {
                GetPFlowAccessors<Quat>( chCont, PARTICLECHANNELORIENTATIONR_INTERFACE,
                                         PARTICLECHANNELORIENTATIONW_INTERFACE, chOrientR, chOrientW );
                if( !chOrientR )
                    orientationBlendAlpha = 1.f;
            }
        }

        IParticleChannelAngAxisW* chSpinW = NULL;
        IParticleChannelAngAxisR* chSpinR = NULL;
        IParticleChannelQuatW* chOrientInitW = NULL;

        frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> spinAcc(
            frantic::graphics::vector3f( 0, 0, 0 ) );
        float spinBlendAlpha;
        bool initOrient = false;

        if( field->get_channel_map().has_channel( spinChannelName ) &&
            field->get_channel_map()[spinChannelName].arity() == 3 &&
            frantic::channels::is_channel_data_type_float( field->get_channel_map()[spinChannelName].data_type() ) ) {
            spinAcc = field->get_channel_map().get_cvt_accessor<frantic::graphics::vector3f>( spinChannelName );
            spinBlendAlpha = pblock()->GetFloat( kSpinBlend, timeMiddle );
            if( spinBlendAlpha > 0 && spinBlendAlpha < 1.f )
                spinBlendAlpha =
                    1.f -
                    std::pow( 1.f - spinBlendAlpha,
                              frameFraction ); // Found by expanding 1, 2, 3 & 4 subdivisions and finding the pattern.

            if( spinBlendAlpha > 0.f ) {
                // We need to initialize the orientation channel for spin to work.
                if( chOrientW == NULL ) {
                    IParticleChannelQuatR* chOrientInitR = NULL;
                    GetPFlowAccessors<Quat>( chCont, PARTICLECHANNELORIENTATIONR_INTERFACE,
                                             PARTICLECHANNELORIENTATIONW_INTERFACE, chOrientInitR, chOrientInitW,
                                             &initOrient );
                    if( !chOrientInitW )
                        initOrient = false;
                }

                GetPFlowAccessors<AngAxis>( chCont, PARTICLECHANNELSPINR_INTERFACE, PARTICLECHANNELSPINW_INTERFACE,
                                            chSpinR, chSpinW );
                if( !chSpinR )
                    spinBlendAlpha = 1.f;
            }
        }

        IParticleChannelPoint3W* chScaleW = NULL;
        IParticleChannelPoint3R* chScaleR = NULL;

        frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> scaleAcc;
        frantic::channels::channel_cvt_accessor<float> scaleScalarAcc;
        float scaleBlendAlpha;

        if( field->get_channel_map().has_channel( scaleChannelName ) ) {
            Interface_ID ScaleRID = PARTICLECHANNELSCALER_INTERFACE, ScaleWID = PARTICLECHANNELSCALEW_INTERFACE;

            if( field->get_channel_map()[scaleChannelName].arity() == 3 &&
                frantic::channels::is_channel_data_type_float(
                    field->get_channel_map()[scaleChannelName].data_type() ) ) {
                scaleAcc = field->get_channel_map().get_cvt_accessor<frantic::graphics::vector3f>( scaleChannelName );
            } else if( field->get_channel_map()[scaleChannelName].arity() == 1 &&
                       frantic::channels::is_channel_data_type_float(
                           field->get_channel_map()[scaleChannelName].data_type() ) ) {
                scaleScalarAcc = field->get_channel_map().get_cvt_accessor<float>( scaleChannelName );
            }

            if( scaleAcc.is_valid() || scaleScalarAcc.is_valid() ) {
                scaleBlendAlpha = pblock()->GetFloat( kScaleBlend, timeMiddle );
                if( scaleBlendAlpha > 0 && scaleBlendAlpha < 1.f )
                    scaleBlendAlpha =
                        1.f -
                        std::pow(
                            1.f - scaleBlendAlpha,
                            frameFraction ); // Found by expanding 1, 2, 3 & 4 subdivisions and finding the pattern.

                if( scaleBlendAlpha > 0.f ) {
                    GetPFlowAccessors<Point3>( chCont, PARTICLECHANNELSCALER_INTERFACE, PARTICLECHANNELSCALEW_INTERFACE,
                                               chScaleR, chScaleW );
                    if( !chScaleR )
                        scaleBlendAlpha = 1.f;
                }
            }
        }

        IParticleChannelMeshMapW* chTexture = NULL;

        typedef std::vector<
            std::pair<IParticleChannelMapW*, frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f>>>
            map_channel_list;

        map_channel_list mapChannels;

        if( field->get_channel_map().has_channel( colorChannelName ) &&
            field->get_channel_map()[colorChannelName].arity() == 3 &&
            frantic::channels::is_channel_data_type_float( field->get_channel_map()[colorChannelName].data_type() ) ) {
            if( !chTexture ) {
                Class_ID meshMapID = ParticleChannelMeshMap_Class_ID;
                Interface_ID chShapeTextureWID = PARTICLECHANNELSHAPETEXTUREW_INTERFACE,
                             chShapeTextureRID = PARTICLECHANNELSHAPETEXTURER_INTERFACE;

                chTexture = (IParticleChannelMeshMapW*)chCont->EnsureInterface( chShapeTextureWID, meshMapID, true,
                                                                                chShapeTextureRID, chShapeTextureWID,
                                                                                /*false*/ true, actionNode, NULL );
            }

            if( chTexture ) {
                chTexture->SetMapSupport( 0, true );
                IParticleChannelMapW* chMap = chTexture->GetMapChannel( 0 );
                if( chMap ) {
                    chMap->SetTVFace( NULL ); // no tvFace data for all particles
                    mapChannels.push_back( std::make_pair(
                        chMap,
                        field->get_channel_map().get_cvt_accessor<frantic::graphics::vector3f>( colorChannelName ) ) );
                }
            }
        }

        if( field->get_channel_map().has_channel( textureChannelName ) &&
            field->get_channel_map()[textureChannelName].arity() == 3 &&
            frantic::channels::is_channel_data_type_float(
                field->get_channel_map()[textureChannelName].data_type() ) ) {
            if( !chTexture ) {
                Class_ID meshMapID = ParticleChannelMeshMap_Class_ID;
                Interface_ID chShapeTextureWID = PARTICLECHANNELSHAPETEXTUREW_INTERFACE,
                             chShapeTextureRID = PARTICLECHANNELSHAPETEXTURER_INTERFACE;

                chTexture = (IParticleChannelMeshMapW*)chCont->EnsureInterface( chShapeTextureWID, meshMapID, true,
                                                                                chShapeTextureRID, chShapeTextureWID,
                                                                                /*false*/ true, actionNode, NULL );
            }

            if( chTexture ) {
                chTexture->SetMapSupport( 1, true );
                IParticleChannelMapW* chMap = chTexture->GetMapChannel( 1 );
                if( chMap ) {
                    chMap->SetTVFace( NULL ); // no tvFace data for all particles
                    mapChannels.push_back(
                        std::make_pair( chMap, field->get_channel_map().get_cvt_accessor<frantic::graphics::vector3f>(
                                                   textureChannelName ) ) );
                }
            }
        }

        for( int i = 0, iEnd = m_pblock->Count( kMapChannels ); i < iEnd; ++i ) {
            frantic::tstring mapChannelName = get_string( m_pblock, kMapChannels, 0, i );

            if( !mapChannelName.empty() && field->get_channel_map().has_channel( mapChannelName ) &&
                field->get_channel_map()[mapChannelName].arity() == 3 &&
                frantic::channels::is_channel_data_type_float(
                    field->get_channel_map()[mapChannelName].data_type() ) ) {
                if( !chTexture ) {
                    Class_ID meshMapID = ParticleChannelMeshMap_Class_ID;
                    Interface_ID chShapeTextureWID = PARTICLECHANNELSHAPETEXTUREW_INTERFACE,
                                 chShapeTextureRID = PARTICLECHANNELSHAPETEXTURER_INTERFACE;

                    chTexture = (IParticleChannelMeshMapW*)chCont->EnsureInterface(
                        chShapeTextureWID, meshMapID, true, chShapeTextureRID, chShapeTextureWID, /*false*/ true,
                        actionNode, NULL );
                }

                if( chTexture ) {
                    chTexture->SetMapSupport( i + 2, true );
                    IParticleChannelMapW* chMap = chTexture->GetMapChannel( i + 2 );
                    if( chMap ) {
                        chMap->SetTVFace( NULL ); // no tvFace data for all particles
                        mapChannels.push_back( std::make_pair(
                            chMap, field->get_channel_map().get_cvt_accessor<frantic::graphics::vector3f>(
                                       mapChannelName ) ) );
                    }
                }
            }
        }

        IParticleChannelBoolW* chSelectedW = NULL;
        // IParticleChannelBoolR* chSelectedR = NULL;

        frantic::channels::channel_cvt_accessor<float> selectionAcc;

        if( field->get_channel_map().has_channel( selectedChannelName ) ) {
            Interface_ID SelectionRID = PARTICLECHANNELSELECTIONR_INTERFACE,
                         SelectionWID = PARTICLECHANNELSELECTIONW_INTERFACE;

            if( field->get_channel_map()[selectedChannelName].arity() == 1 &&
                frantic::channels::is_channel_data_type_float(
                    field->get_channel_map()[selectedChannelName].data_type() ) ) {
                selectionAcc = field->get_channel_map().get_cvt_accessor<float>( selectedChannelName );

                chSelectedW = (IParticleChannelBoolW*)chCont->EnsureInterface( SelectionWID, BoolID, true, SelectionRID,
                                                                               SelectionWID, true );
                // chSelectedR = GetParticleChannelSelectionRInterface(pCont);
            }
        }

        IParticleChannelIntW* chMtlIDW = NULL;
        // IParticleChannelIntR* chMtlIDR = NULL;

        frantic::channels::channel_cvt_accessor<int> mtlIDAcc;

        if( field->get_channel_map().has_channel( mtlIDChannelName ) ) {
            Interface_ID MtlIDRID = PARTICLECHANNELMTLINDEXR_INTERFACE, MtlIDWID = PARTICLECHANNELMTLINDEXW_INTERFACE;

            if( field->get_channel_map()[mtlIDChannelName].arity() == 1 &&
                frantic::channels::is_channel_data_type_int(
                    field->get_channel_map()[mtlIDChannelName].data_type() ) ) {
                mtlIDAcc = field->get_channel_map().get_cvt_accessor<int>( mtlIDChannelName );
                chMtlIDW =
                    (IParticleChannelIntW*)chCont->EnsureInterface( MtlIDWID, IntID, true, MtlIDRID, MtlIDWID, true );
                // chMtlIDR = GetParticleChannelMXSIntRInterface(pCont);
            }
        }

        IParticleChannelIntW* chMXSIntW = NULL;
        // IParticleChannelIntR* chMXSIntR = NULL;

        frantic::channels::channel_cvt_accessor<int> mxsIntAcc;

        if( field->get_channel_map().has_channel( mxsIntChannelName ) ) {
            Interface_ID MXSIntRID = PARTICLECHANNELMXSFLOATR_INTERFACE, MXSIntWID = PARTICLECHANNELMXSFLOATW_INTERFACE;

            if( field->get_channel_map()[mxsIntChannelName].arity() == 1 &&
                frantic::channels::is_channel_data_type_int(
                    field->get_channel_map()[mxsIntChannelName].data_type() ) ) {
                mxsIntAcc = field->get_channel_map().get_cvt_accessor<int>( mxsIntChannelName );
                chMXSIntW = (IParticleChannelIntW*)chCont->EnsureInterface( MXSIntWID, IntID, true, MXSIntRID,
                                                                            MXSIntWID, true );
                // chMXSIntR = GetParticleChannelMXSIntRInterface(pCont);
            }
        }

        IParticleChannelFloatW* chMXSFloatW = NULL;
        // IParticleChannelFloatR* chMXSFloatR = NULL;

        frantic::channels::channel_cvt_accessor<float> mxsFloatAcc;

        if( field->get_channel_map().has_channel( mxsFloatChannelName ) ) {
            Interface_ID MXSFloatRID = PARTICLECHANNELMXSFLOATR_INTERFACE,
                         MXSFloatWID = PARTICLECHANNELMXSFLOATW_INTERFACE;

            if( field->get_channel_map()[mxsFloatChannelName].arity() == 1 &&
                frantic::channels::is_channel_data_type_float(
                    field->get_channel_map()[mxsFloatChannelName].data_type() ) ) {
                mxsFloatAcc = field->get_channel_map().get_cvt_accessor<float>( mxsFloatChannelName );
                chMXSFloatW = (IParticleChannelFloatW*)chCont->EnsureInterface( MXSFloatWID, FloatID, true, MXSFloatRID,
                                                                                MXSFloatWID, true );
                // chMXSFloatR = GetParticleChannelMXSFloatRInterface(pCont);
            }
        }

        IParticleChannelPoint3W* chMXSVecW = NULL;
        // IParticleChannelPoint3R* chMXSVecR = NULL;

        frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> mxsVecAcc;

        if( field->get_channel_map().has_channel( mxsVectorChannelName ) ) {
            Interface_ID MXSVecRID = PARTICLECHANNELMXSVECTORR_INTERFACE,
                         MXSVecWID = PARTICLECHANNELMXSVECTORW_INTERFACE;

            if( field->get_channel_map()[mxsVectorChannelName].arity() == 3 &&
                frantic::channels::is_channel_data_type_float(
                    field->get_channel_map()[mxsVectorChannelName].data_type() ) ) {
                mxsVecAcc =
                    field->get_channel_map().get_cvt_accessor<frantic::graphics::vector3f>( mxsVectorChannelName );
                chMXSVecW = (IParticleChannelPoint3W*)chCont->EnsureInterface( MXSVecWID, Point3ID, true, MXSVecRID,
                                                                               MXSVecWID, true );
                // chMXSVecR = GetParticleChannelMXSVectorRInterface(pCont);
            }
        }

        // AffineParts decompToWorld;
        // if( chOrientW != NULL || chScaleW != NULL ){
        //	decomp_affine( toWorld, &decompToWorld ); //TODO: Is this too expensive?! Only going to do this if
        //necessary.
        // }

        char* buffer = (char*)alloca( field->get_channel_map().structure_size() );

        for( int particleIndex = 0; particleIndex < particleCount; ++particleIndex ) {
            frantic::graphics::vector3f pos =
                frantic::max3d::from_max_t( chPos->GetValue( particleIndex ) /** fromWorld*/ );

            bool valid = field->evaluate_field( buffer, pos );
            if( !valid )
                continue;

            if( chSpeedW != NULL ) {
                Point3 newVelocity = frantic::max3d::to_max_t( velocityChannel.get( buffer ) );
                // newVelocity = toWorld.VectorTransform( newVelocity ); //Transform into worldspace.
                newVelocity = newVelocity / static_cast<float>(
                                                TIME_TICKSPERSEC ); // Convert from units per second to units per tick.

                if( velocityBlendAlpha < 1.f ) {
                    const Point3& velocity = chSpeedR->GetValue( particleIndex );
                    newVelocity = velocity + velocityBlendAlpha * ( newVelocity - velocity );
                }

                chSpeedW->SetValue( particleIndex, newVelocity );
            }

            if( chOrientW != NULL ) {
                Quat newOrient = frantic::max3d::to_max_t( orientationAcc.get( buffer ) );
                // newOrient = newOrient * decompToWorld.q; //Transform to world-space using the orientation part of the
                // node's decomposed transformation matrix.

                if( orientationBlendAlpha < 1.f ) {
                    const Quat& orient = chOrientR->GetValue( particleIndex );
                    newOrient = Slerp( orient, newOrient, orientationBlendAlpha );
                }

                chOrientW->SetValue( particleIndex, newOrient );
            }

            if( chSpinW != NULL ) {
                if( initOrient && chNew->IsNew( particleIndex ) ) // We might need to initialize the orientation if
                                                                  // there wasn't already one there for the particle.
                    chOrientInitW->SetValue( particleIndex, Quat( 0.f, 0.f, 0.f, 1.f ) );

                AngAxis newSpin;
                newSpin.axis = frantic::max3d::to_max_t( spinAcc.get( buffer ) );
                newSpin.angle = newSpin.axis.Length() /
                                static_cast<float>( TIME_TICKSPERSEC ); // Get magnitude before transform to avoid
                                                                        // scaling. Convert to radians per tick.

                // newSpin.axis = toWorld.VectorTransform( newSpin.axis ); //Transform into worldspace.
                newSpin.axis.Normalize();

                if( spinBlendAlpha < 1.f ) {
                    const AngAxis& spin = chSpinR->GetValue( particleIndex );

                    Point3 oldSpinAxis = spin.axis.Normalize(); // Normalize this, just in case.

                    newSpin.axis = ( oldSpinAxis + spinBlendAlpha * ( newSpin.axis - oldSpinAxis ) ).Normalize();
                    newSpin.angle = spin.angle + spinBlendAlpha * ( newSpin.angle - spin.angle );
                }

                chSpinW->SetValue( particleIndex, newSpin );
            }

            if( chScaleW != NULL ) {
                Point3 newScale;
                if( scaleAcc.is_valid() ) {
                    newScale = frantic::max3d::to_max_t( scaleAcc.get( buffer ) );
                } else {
                    newScale = frantic::max3d::to_max_t( frantic::graphics::vector3f( scaleScalarAcc.get( buffer ) ) );
                }

                // newScale = decompToWorld.k * newScale; //Transform into world space using the scale component of the
                // decomposed transform matrix.

                if( scaleBlendAlpha < 1.f ) {
                    const Point3& oldScale = chScaleR->GetValue( particleIndex );
                    newScale = oldScale + scaleBlendAlpha * ( newScale - oldScale );
                }

                chScaleW->SetValue( particleIndex, newScale );
            }

            if( chSelectedW != NULL )
                chSelectedW->SetValue( particleIndex, selectionAcc.get( buffer ) > 0.f );

            for( map_channel_list::const_iterator it = mapChannels.begin(), itEnd = mapChannels.end(); it != itEnd;
                 ++it )
                it->first->SetUVVert( particleIndex, frantic::max3d::to_max_t( it->second.get( buffer ) ) );

            if( chMtlIDW != NULL )
                chMtlIDW->SetValue( particleIndex, std::max( -1, mtlIDAcc.get( buffer ) ) );

            if( chMXSIntW != NULL )
                chMXSIntW->SetValue( particleIndex, mxsIntAcc.get( buffer ) );

            if( chMXSFloatW != NULL )
                chMXSFloatW->SetValue( particleIndex, mxsFloatAcc.get( buffer ) );

            if( chMXSVecW != NULL )
                chMXSVecW->SetValue( particleIndex, frantic::max3d::to_max_t( mxsVecAcc.get( buffer ) ) );
        }

    } catch( exception& e ) {
        GetCOREInterface()->Log()->LogEntry( SYSLOG_ERROR, NO_DIALOG,
                                             _T("Error processing FloodVelocityLoaderPFOperator"), _T("%s"),
                                             frantic::strings::to_tstring( e.what() ).c_str() );
        mprintf( _T("Error in FloodVelocityLoaderPFOperator::Proceed()\n\t%s\n"),
                 frantic::strings::to_tstring( e.what() ).c_str() );
        if( IsNetworkRenderServer() )
            throw;
        return false;
    }

    return true;
}

} // namespace max3d
} // namespace ember

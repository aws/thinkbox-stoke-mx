// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource_genome.h"

#include <frantic/magma/magma_exception.hpp>
#include <frantic/magma/max3d/IMagmaHolder.hpp>
#include <frantic/magma/simple_compiler/simple_mesh_compiler.hpp>

#include <frantic/max3d/GenericReferenceTarget.hpp>
#include <frantic/max3d/geometry/max_mesh_interface.hpp>
#include <frantic/max3d/max_utility.hpp>
#include <frantic/max3d/maxscript/maxscript.hpp>

#include <frantic/channels/named_channel_data.hpp>

#include <IRefTargMonitor.h>
#include <ref.h>

#include <boost/scope_exit.hpp>

#include "StokeMXVersion.h"
#include <stoke/max3d/Genome/Genome.hpp>
#include <stoke/max3d/Genome/GenomeMagmaContext.hpp>
#include <stoke/max3d/Genome/MaxGeometryMagmaHolder.h>
#include <stoke/max3d/Genome/MaxGeometryMagmaSingleton.hpp>

#include <Shellapi.h>

#define WM_USER_UPDATE WM_USER + 0x456d

extern int GetMaximumDebuggerIterations();

extern void ShowLogWindow( bool visible = true );

using namespace GeometryMagmaHolderParams;

class GenomeMod : public GenericReferenceTarget<OSModifier, GenomeMod>, public IGenome, public IRefTargMonitor {
  private:
    enum {
        pbMagmaHolderRef,
        pbMeshIterationPattern,
        pbRemoveDegenerateFaces,
        pbClampVertexSelection,
        pbIterationCount
    };

    static GenSubObjType SOT_Vertex;
    static GenSubObjType SOT_Face;

    typedef GenericReferenceTarget<OSModifier, GenomeMod> super_class;

    friend class GenomeModifierDlgProc;
    friend ClassDesc2* GetMagmaGeometryChannelModifierDesc();

    class MyClassDesc : public ClassDesc2 {
        ParamBlockDesc2 m_paramDesc;
        FPInterfaceDesc m_fpDesc;

      public:
        MyClassDesc();

        FPInterfaceDesc* GetDescByID( Interface_ID id );

        int IsPublic() { return TRUE; }
        void* Create( BOOL loading ) { return new GenomeMod( loading ); }
        const TCHAR* ClassName() { return _M( "Genome Modifier" ); }
        SClass_ID SuperClassID() { return OSM_CLASS_ID; }
        Class_ID ClassID() { return GenomeMod_CLASS_ID; }
        const TCHAR* Category() { return _T(""); }

        const TCHAR* InternalName() {
            return _M( "GenomeModifier" );
        } // returns fixed parsable name (scripter-visible name)
        HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
        const MCHAR* NonLocalizedClassName() { return _M( "Genome Modifier" ); }
#endif
    };

    enum iteration_pattern { vertices, faces, face_corners };

    static const TCHAR* IterationPatternNames[];

    bool m_isEditing;
    bool m_isThreaded;

    struct node_monitor {
        bool suspendChange, suspendEnumDep;
        // RefTargMonitorRefMaker* monitor; //Moved to LocalModData in ModContext.

        node_monitor()
            : /*monitor( NULL ),*/ suspendChange( false )
            , suspendEnumDep( false ) {}

        /*~node_monitor(){
                if( monitor )
                        monitor->DeleteMe();
        }*/
    } m_nodeMonitor;

    struct errorInfoStruct {
        bool present;
        M_STD_STRING message;
        frantic::magma::magma_interface::magma_id nodeID;

        errorInfoStruct()
            : present( false )
            , nodeID( -43 )
            , message( _M( "No Error" ) ) {}
    } m_errorInfo;

  private:
    // Will find a scene node connected to this modifier, and monitor it for TM changes.
    INode* GetSceneNode( ModContext* mc );

    void clear_error();
    void set_error( const frantic::magma::magma_exception& e );
    void set_error( const frantic::tstring& message,
                    frantic::magma::magma_interface::magma_id id = frantic::magma::magma_interface::INVALID_ID );
    void notify_error_changed();

    void open_editor_window();
    void open_editor_context_menu();

    bool writes_to_channel( const frantic::tstring& channelName ) const;

  protected:
    virtual ClassDesc2* GetClassDesc();

  public:
    // static const frantic::tchar* s_iterationStrings[];
    // static const frantic::geometry::mesh_channel::channel_type s_iterationPattern;

  public:
    GenomeMod( BOOL loading = FALSE );
    virtual ~GenomeMod();

    // This is different from Modifier::LocalValidity(), because LocalValidity() return NEVER (to force a cache before
    // it) if the modifier is being actively edited.
    inline Interval GetValidity( TimeValue t );

    // From FPMixinInterface
    virtual FPInterfaceDesc* GetDescByID( Interface_ID id );

    // From IGenome
    virtual bool GetLastError( TYPE_TSTR_BR_TYPE outMessage, TYPE_INT_BR_TYPE outMagmaID );
    virtual Value* GetChannelNames( int iterationType, bool forOutput = false );
    virtual void EvaluateDebug( TimeValue t, ModContext& mc, ObjectState* os, INode* node,
                                std::vector<frantic::magma::debug_data>& outVals );
    virtual void SetThreading( bool threads = true );
    virtual bool GetThreading() const;

    // From IRefTargMonitor
#if MAX_VERSION_MAJOR < 17
    virtual RefResult ProcessRefTargMonitorMsg( Interval changeInt, RefTargetHandle hTarget, PartID& partID,
                                                RefMessage message, bool fromMonitoredTarget );
#else
    virtual RefResult ProcessRefTargMonitorMsg( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                                RefMessage message, bool fromMonitoredTarget, bool propagate,
                                                RefTargMonitorRefMaker* caller );
#endif
    virtual int ProcessEnumDependents( DependentEnumProc* dep );

    // From Animatable
    virtual void BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev = NULL );
    virtual void EndEditParams( IObjParam* ip, ULONG flags, Animatable* next = NULL );
    virtual BaseInterface* GetInterface( Interface_ID id );

    // From ReferenceMaker
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );
    virtual IOResult Load( ILoad* iload );

    // From ReferenceTarget
    virtual void BaseClone( ReferenceTarget* from, ReferenceTarget* to, RemapDir& remap );

    // From BaseObject
    virtual
#if MAX_VERSION_MAJOR >= 15
        const
#endif
#if MAX_VERSION_MAJOR < 24
        TYPE_STRING_TYPE
        GetObjectName();
#else
        TYPE_STRING_TYPE
        GetObjectName( bool localized );
#endif
    virtual CreateMouseCallBack* GetCreateMouseCallBack( void );
    virtual int Display( TimeValue t, INode* inode, ViewExp* vpt, int flags, ModContext* mc );
    virtual void GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* vpt, Box3& box, ModContext* mc );
    // virtual int NumSubObjTypes();
    virtual ISubObjType* GetSubObjType( int i );
    // virtual int GetSubObjectLevel();

    // From Modifier
    virtual Interval LocalValidity( TimeValue t );
    virtual ChannelMask ChannelsUsed();
    virtual ChannelMask ChannelsChanged();
    virtual Class_ID InputType();
    virtual void ModifyObject( TimeValue t, ModContext& mc, ObjectState* os, INode* node );
};

GenSubObjType GenomeMod::SOT_Vertex( _M( "Vertex" ), _M( "SubObjectIcons" ), 1 );
GenSubObjType GenomeMod::SOT_Face( _M( "Face" ), _M( "SubObjectIcons" ), 3 );
// GenSubObjType GenomeMod::SOT_FaceVertex(_M("FaceVertices"), _M("SubObjectIcons"), 30);

const TCHAR* GenomeMod::IterationPatternNames[] = { _T("Vertices"), _T("Faces"), _T("Face Corners") };

class GenomeModifierDlgProc : public ParamMap2UserDlgProc {
  public:
    static GenomeModifierDlgProc* GetInstance() {
        static GenomeModifierDlgProc s_instance;
        return &s_instance;
    }

    virtual INT_PTR DlgProc( TimeValue /*t*/, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM /*lParam*/ ) {
        IParamBlock2* pblock = map->GetParamBlock();
        if( !pblock )
            return FALSE;

        BOOL checkedVal = BST_INDETERMINATE;
        HWND hComboBox;

        switch( msg ) {

        case WM_INITDIALOG:
            if( ReferenceTarget* rtarg = pblock->GetReferenceTarget( GenomeMod::pbMagmaHolderRef ) )
                checkedVal =
                    rtarg->GetParamBlock( 0 )->GetInt( kMagmaAutoUpdate ) != FALSE ? BST_CHECKED : BST_UNCHECKED;
            CheckDlgButton( hWnd, IDC_MAGMAMODIFIER_AUTOUPDATE_CHECK, checkedVal );

            if( ReferenceTarget* rtarg = pblock->GetReferenceTarget( GenomeMod::pbMagmaHolderRef ) )
                checkedVal = rtarg->GetParamBlock( 0 )->GetInt( kMagmaAutomaticRenameOFF ) == FALSE ? BST_CHECKED
                                                                                                    : BST_UNCHECKED;
            CheckDlgButton( hWnd, IDC_MAGMAMODIFIER_AUTORENAME_CHECK, checkedVal );

            hComboBox = GetDlgItem( map->GetHWnd(), IDC_MESH_ITERATION_PATTERN );

            ComboBox_AddString( hComboBox, GenomeMod::IterationPatternNames[0] );
            ComboBox_AddString( hComboBox, GenomeMod::IterationPatternNames[1] );
            ComboBox_AddString( hComboBox, GenomeMod::IterationPatternNames[2] );

            ComboBox_SelectString( hComboBox, -1, pblock->GetStr( GenomeMod::pbMeshIterationPattern ) );
            break;

        case WM_COMMAND:

            if( LOWORD( wParam ) == IDC_MESH_ITERATION_PATTERN ) {
                if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                    hComboBox = GetDlgItem( map->GetHWnd(), IDC_MESH_ITERATION_PATTERN );

                    int curSel = ComboBox_GetCurSel( hComboBox );
                    if( curSel < 0 || curSel > 2 )
                        curSel = 0;

                    pblock->SetValue( GenomeMod::pbMeshIterationPattern, 0, GenomeMod::IterationPatternNames[curSel] );
                }
            } else if( HIWORD( wParam ) == BN_CLICKED ) {
                if( LOWORD( wParam ) == IDC_MAGMAMODIFIER_OPEN_BUTTON ) {
                    try {
                        static_cast<GenomeMod*>( pblock->GetOwner() )->open_editor_window();
                    } catch( const std::exception& e ) {
                        FF_LOG( error ) << e.what() << std::endl;
                    } catch( ... ) {
                        // Can't allow max to crash by propogating any exceptions.
                        FF_LOG( error ) << "Unhandled exception in: " << __FILE__ << " line: " << __LINE__ << std::endl;
                    }

                    return TRUE;
                } else if( LOWORD( wParam ) == IDC_GENOME_EDITORCONTEXT_BUTTON ) {
                    try {
                        static_cast<GenomeMod*>( pblock->GetOwner() )->open_editor_context_menu();
                    } catch( const std::exception& e ) {
                        FF_LOG( error ) << e.what() << std::endl;
                    } catch( ... ) {
                        // Can't allow max to crash by propogating any exceptions.
                        FF_LOG( error ) << "Unhandled exception in: " << __FILE__ << " line: " << __LINE__ << std::endl;
                    }

                    return TRUE;
                } else if( LOWORD( wParam ) == IDC_MAGMAMODIFIER_AUTORENAME_CHECK ) {
                    // Need to explicitly handle this due to the parameter being held by the magma holder and not the
                    // modifier.
                    checkedVal = ( IsDlgButtonChecked( hWnd, IDC_MAGMAMODIFIER_AUTORENAME_CHECK ) == BST_CHECKED );
                    if( ReferenceTarget* rtarg = pblock->GetReferenceTarget( GenomeMod::pbMagmaHolderRef ) )
                        rtarg->GetParamBlock( 0 )->SetValue( kMagmaAutomaticRenameOFF, 0, !checkedVal );
                } else if( LOWORD( wParam ) == IDC_MAGMAMODIFIER_AUTOUPDATE_CHECK ) {
                    // Need to explicitly handle this due to the parameter being held by the magma holder and not the
                    // modifier.
                    checkedVal = ( IsDlgButtonChecked( hWnd, IDC_MAGMAMODIFIER_AUTOUPDATE_CHECK ) == BST_CHECKED );
                    if( ReferenceTarget* rtarg = pblock->GetReferenceTarget( GenomeMod::pbMagmaHolderRef ) )
                        rtarg->GetParamBlock( 0 )->SetValue( kMagmaAutoUpdate, 0, checkedVal );
                }
            }
            break;

        case WM_USER_UPDATE:
            if( LOWORD( wParam ) == IDC_MAGMAMODIFIER_AUTOUPDATE_CHECK ) {
                if( ReferenceTarget* rtarg = pblock->GetReferenceTarget( GenomeMod::pbMagmaHolderRef ) )
                    checkedVal =
                        rtarg->GetParamBlock( 0 )->GetInt( kMagmaAutoUpdate ) != FALSE ? BST_CHECKED : BST_UNCHECKED;
                CheckDlgButton( hWnd, IDC_MAGMAMODIFIER_AUTOUPDATE_CHECK, checkedVal );
            } else if( LOWORD( wParam ) == IDC_MAGMAMODIFIER_AUTORENAME_CHECK ) {
                if( ReferenceTarget* rtarg = pblock->GetReferenceTarget( GenomeMod::pbMagmaHolderRef ) )
                    checkedVal = rtarg->GetParamBlock( 0 )->GetInt( kMagmaAutomaticRenameOFF ) == FALSE ? BST_CHECKED
                                                                                                        : BST_UNCHECKED;
                CheckDlgButton( hWnd, IDC_MAGMAMODIFIER_AUTORENAME_CHECK, checkedVal );
            }
            break;
        }

        return FALSE;
    }

    virtual void DeleteThis() {}
};

class GenomeAboutDlgProc : public ParamMap2UserDlgProc {
  public:
    static GenomeAboutDlgProc* GetInstance() {
        static GenomeAboutDlgProc s_instance;
        return &s_instance;
    }

    virtual INT_PTR DlgProc( TimeValue /*t*/, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM /*lParam*/ ) {
        switch( msg ) {
        case WM_INITDIALOG:
            SetWindowText( GetDlgItem( hWnd, IDC_GENOMEABOUT_VERSION_STATIC ), _T("Version: ") _T( FRANTIC_VERSION ) );
            break;
        case WM_COMMAND:
            switch( LOWORD( wParam ) ) {
            case IDC_GENOMEABOUT_OPENHELP_BUTTON:
                if( HIWORD( wParam ) == BN_CLICKED ) {
                    // Windows docs say its good practice to init com before using ShellExecute.
                    // CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
                    ShellExecute( hWnd, _T("open"), _T("http://www.thinkboxsoftware.com"), NULL, NULL,
                                  SW_SHOWNORMAL );
                }
                break;
            case IDC_GENOMEABOUT_OPENLOG_BUTTON:
                if( HIWORD( wParam ) == BN_CLICKED ) {
                    ShowLogWindow( true );
                }
                break;
            }
        }
        return FALSE;
    }

    virtual void DeleteThis() {}
};

GenomeMod::MyClassDesc::MyClassDesc()
    : m_paramDesc( (BlockID)0, _T("Parameters"), IDS_PARAMETERS, NULL,
                   P_AUTO_CONSTRUCT | P_VERSION | P_AUTO_UI | P_MULTIMAP, 0, 0, 2, 1, IDD_GENOME_ABOUT,
                   IDS_GENOME_ABOUTROLLOUT_TITLE, 0, APPENDROLL_CLOSED, GenomeAboutDlgProc::GetInstance(), 0,
                   IDD_GENOME, IDS_GENOME_MAINROLLOUT_TITLE, 0, 0, GenomeModifierDlgProc::GetInstance(), PARAMTYPE_END )
    , m_fpDesc( Genome_INTERFACE, _M( "Genome" ), 0, NULL, FP_MIXIN, PARAMTYPE_END ) {
    m_paramDesc.SetClassDesc( this );
    m_fpDesc.SetClassDesc( this );

    m_paramDesc.AddParam( pbMagmaHolderRef, _T("magmaHolder"), TYPE_REFTARG, P_SUBANIM, IDS_GENOME_MAGMAHOLDER,
                          PARAMTYPE_END );
    m_paramDesc.ParamOption( pbMagmaHolderRef, p_classID, GeometryMagmaHolder_CLASS_ID );

    m_paramDesc.AddParam( pbMeshIterationPattern, _T("meshIterationPattern"), TYPE_STRING, 0,
                          IDS_MESH_ITERATION_PATTERN, PARAMTYPE_END );
    m_paramDesc.ParamOption( pbMeshIterationPattern, p_default, IterationPatternNames[vertices] );

    m_paramDesc.AddParam( pbRemoveDegenerateFaces, _T("removeDegenerateFaces"), TYPE_BOOL, 0,
                          IDS_GENOME_REMOVEDEGENERATE, PARAMTYPE_END );
    m_paramDesc.ParamOption( pbRemoveDegenerateFaces, p_default, FALSE );
    m_paramDesc.ParamOption( pbRemoveDegenerateFaces, p_ui, 0, TYPE_SINGLECHEKBOX,
                             IDC_MAGMAMODIFIER_REMOVEDEGENERATE_CHECK, PARAMTYPE_END );

    m_paramDesc.AddParam( pbClampVertexSelection, _T("clampVertexSelection"), TYPE_BOOL, 0, IDS_GENOME_CLAMPSELECTION,
                          PARAMTYPE_END );
    m_paramDesc.ParamOption( pbClampVertexSelection, p_default, TRUE );
    m_paramDesc.ParamOption( pbClampVertexSelection, p_ui, 0, TYPE_SINGLECHEKBOX,
                             IDC_MAGMAMODIFIER_CLAMPVERTSELECTION_CHECK );

    m_paramDesc.AddParam( pbIterationCount, _T("IterationCount"), TYPE_INT, P_RESET_DEFAULT | P_ANIMATABLE, 0,
                          PARAMTYPE_END );
    m_paramDesc.ParamOption( pbIterationCount, p_default, 1 );
    m_paramDesc.ParamOption( pbIterationCount, p_range, 0, INT_MAX );
    m_paramDesc.ParamOption( pbIterationCount, p_ui, 0, TYPE_SPINNER, EDITTYPE_POS_INT,
                             IDC_MAGMAMODIFIER_ITERATIONS_EDIT, IDC_MAGMAMODIFIER_ITERATIONS_SPIN, SPIN_AUTOSCALE );

    IGenome::init_fpinterface_desc( m_fpDesc );
}

FPInterfaceDesc* GenomeMod::MyClassDesc::GetDescByID( Interface_ID id ) {
    if( id == Genome_INTERFACE )
        return &m_fpDesc;
    return NULL;
}

ClassDesc2* GetMagmaGeometryChannelModifierDesc() {
    static GenomeMod::MyClassDesc theClassDesc;
    return &theClassDesc;
}

GenomeMod::GenomeMod( BOOL loading ) {
    m_pblock = NULL;
    m_isEditing = false;
    m_isThreaded = true;

    if( !loading ) {
        GetMagmaGeometryChannelModifierDesc()->MakeAutoParamBlocks( this );
        m_pblock->SetValue( pbMagmaHolderRef, 0, CreateGenomeMagmaHolder() );
    }
}

GenomeMod::~GenomeMod() {}

void GenomeMod::clear_error() {
    if( m_errorInfo.present ) {
        m_errorInfo.present = false;
        notify_error_changed();
    }
}

void GenomeMod::set_error( const frantic::magma::magma_exception& e ) {
    set_error( e.get_message( true ), e.get_node_id() );
}

void GenomeMod::set_error( const frantic::tstring& message, frantic::magma::magma_interface::magma_id id ) {
    if( !m_errorInfo.present || m_errorInfo.nodeID != id || message != m_errorInfo.message ) {
        m_errorInfo.present = true;
        m_errorInfo.nodeID = id;
        m_errorInfo.message = message;

        notify_error_changed();
    }
}

void GenomeMod::notify_error_changed() {
    frantic::max3d::mxs::expression expr( _M(
        "try(::GenomeMFEditor_Functions.ErrorCallback __magmaHolder __nodeID __errorMessage __futureArgs)catch()" ) );

    if( m_errorInfo.present ) {
        FF_LOG( error ) << m_errorInfo.message << std::endl;

        expr.bind( _M( "__magmaHolder" ), this );
        expr.bind( _M( "__nodeID" ), Integer::intern( m_errorInfo.nodeID ) );
        expr.bind( _M( "__errorMessage" ), new String( m_errorInfo.message.c_str() ) );
        expr.bind( _M( "__futureArgs" ), &undefined );
    } else {
        expr.bind( _M( "__magmaHolder" ), this );
        expr.bind( _M( "__nodeID" ), &undefined );
        expr.bind( _M( "__errorMessage" ), &undefined );
        expr.bind( _M( "__futureArgs" ), &undefined );
    }

    expr.evaluate<void>();
}

void GenomeMod::open_editor_window() {
    frantic::max3d::mxs::expression(
        _M( "try(::GenomeModifier_ImplementationObject.openEditor magmaHolder magmaOwner offscreen:false)catch()" ) )
        .bind( _M( "magmaHolder" ), m_pblock->GetReferenceTarget( pbMagmaHolderRef ) )
        .bind( _M( "magmaOwner" ), this )
        .evaluate<void>();
}

void GenomeMod::open_editor_context_menu() {
    frantic::max3d::mxs::expression( _M( "try(::GenomeMFEditor_Functions.createPresetsRCMenu mode:#modifier)catch()" ) )
        .bind( _M( "magmaHolder" ), m_pblock->GetReferenceTarget( pbMagmaHolderRef ) )
        .evaluate<void>();
}

bool GenomeMod::writes_to_channel( const frantic::tstring& channelName ) const {
    using frantic::magma::magma_interface;

    ReferenceTarget* magmaHolder = m_pblock->GetReferenceTarget( pbMagmaHolderRef );

    boost::shared_ptr<magma_interface> magmaInterface;
    if( frantic::magma::max3d::IMagmaHolder* iHolder = frantic::magma::max3d::GetMagmaHolderInterface( magmaHolder ) )
        magmaInterface = iHolder->get_magma_interface();

    if( !magmaInterface )
        return false;

    for( int i = 0, iEnd = magmaInterface->get_num_outputs( magma_interface::TOPMOST_LEVEL ); i < iEnd; ++i ) {
        magma_interface::magma_id outputID = magmaInterface->get_output( magma_interface::TOPMOST_LEVEL, i ).first;
        if( outputID != magma_interface::INVALID_ID ) {
            bool enabled = false;
            if( magmaInterface->get_property( outputID, _M( "enabled" ), enabled ) && enabled ) {
                frantic::tstring curChannelName;
                if( magmaInterface->get_property( outputID, _M( "channelName" ), curChannelName ) &&
                    curChannelName == channelName )
                    return true;
            }
        }
    }

    return false;
}

ClassDesc2* GenomeMod::GetClassDesc() { return GetMagmaGeometryChannelModifierDesc(); }

class GenomeModData : public LocalModData {
    RefTargMonitorRefMaker* monitor;

  public:
    GenomeModData()
        : monitor( NULL ) {}

    virtual ~GenomeModData() {
        if( monitor )
            monitor->DeleteMe();
        monitor = NULL;
    }

    virtual LocalModData* Clone() { return new GenomeModData; }

    INode* GetNode( GenomeMod* mod, ModContext& mc ) {
        if( monitor && monitor->GetRef() != NULL )
            return static_cast<INode*>( monitor->GetRef() );

        int modIndex = -1;
        IDerivedObject* derivObj = NULL;

        mod->GetIDerivedObject( &mc, derivObj, modIndex );

        INode* result = NULL;
        if( derivObj )
            result = GetCOREInterface7()->FindNodeFromBaseObject( derivObj ); // Gotta find the node first!

        if( !monitor )
            monitor = new RefTargMonitorRefMaker( *mod, NULL );
        monitor->SetRef( result );

        return result;
    }
};

INode* GenomeMod::GetSceneNode( ModContext* mc ) {
    // if( !m_nodeMonitor.monitor )
    //	m_nodeMonitor.monitor = new RefTargMonitorRefMaker( *this, NULL );

    // INode* result = (INode*)m_nodeMonitor.monitor->GetRef();
    // if( !result ){
    //	int modIndex = -1;
    //	IDerivedObject* derivObj = NULL;

    //	this->GetIDerivedObject( mc, derivObj, modIndex );

    //	if( derivObj ){
    //		result = GetCOREInterface7()->FindNodeFromBaseObject( derivObj ); //Gotta find the node first!
    //		if( result )
    //			m_nodeMonitor.monitor->SetRef( result );
    //	}
    //}

    // return result;

    if( mc->localData == NULL )
        mc->localData = new GenomeModData;

    return static_cast<GenomeModData*>( mc->localData )->GetNode( this, *mc );
}

Interval GenomeMod::GetValidity( TimeValue t ) {
    using namespace frantic::magma::max3d;

    ReferenceTarget* magmaHolder = m_pblock->GetReferenceTarget( pbMagmaHolderRef );

    Interval iv = FOREVER;

    // Get the validity of the magma holder's node collection.
    if( IMagmaHolder* holder = GetMagmaHolderInterface( magmaHolder ) )
        iv &= holder->get_validity( t );

    // Combine the animated paramters of the modifier
    m_pblock->GetValidity( t, iv );

    return iv;
}

FPInterfaceDesc* GenomeMod::GetDescByID( Interface_ID id ) {
    if( FPInterfaceDesc* desc = static_cast<MyClassDesc*>( GetMagmaGeometryChannelModifierDesc() )->GetDescByID( id ) )
        return desc;
    return FPMixinInterface::GetDescByID( id );
}

bool GenomeMod::GetLastError( TYPE_TSTR_BR_TYPE outMessage, TYPE_INT_BR_TYPE outMagmaID ) {
    if( m_errorInfo.present ) {
        if( &outMessage != NULL )
            outMessage = m_errorInfo.message.c_str();

        if( &outMagmaID != NULL )
            outMagmaID = m_errorInfo.nodeID;
    }

    return m_errorInfo.present;
}

Value* GenomeMod::GetChannelNames( int iterationType, bool forOutput ) {
    using namespace frantic::channels;
    using frantic::geometry::mesh_channel;
    using frantic::max3d::geometry::MaxMeshInterface;

    std::vector<MaxMeshInterface::channel_info> channelInfo;

    MaxMeshInterface::get_predefined_channels( channelInfo, static_cast<mesh_channel::channel_type>( iterationType ),
                                               forOutput );

    if( !forOutput ) {
        channelInfo.push_back(
            MaxMeshInterface::channel_info( _T("Index"), data_type_int32, 1, _T("Iteration Index") ) );
        switch( iterationType ) {
        case mesh_channel::vertex:
            channelInfo.push_back(
                MaxMeshInterface::channel_info( _T("VertexIndex"), data_type_int32, 1, _T("Vertex Iteration Index") ) );

            // Legacy
            channelInfo.push_back(
                MaxMeshInterface::channel_info( _T("SelectionFromFaceAvg"), data_type_float32, 1,
                                                _T("Vertex Selection Weight, from average of FaceSelection") ) );
            channelInfo.push_back(
                MaxMeshInterface::channel_info( _T("SelectionFromFaceUnion"), data_type_int8, 1,
                                                _T("Vertex Selection Weight, from union of FaceSelection") ) );
            channelInfo.push_back(
                MaxMeshInterface::channel_info( _T("SelectionFromFaceIntersect"), data_type_int8, 1,
                                                _T("Vertex Selection Weight, from intersection of FaceSelection") ) );
            break;
        case mesh_channel::face:
            channelInfo.push_back(
                MaxMeshInterface::channel_info( _T("FaceIndex"), data_type_int32, 1, _T("Face Iteration Index") ) );
            break;
        case mesh_channel::face_vertex:
            channelInfo.push_back(
                MaxMeshInterface::channel_info( _T("FaceIndex"), data_type_int32, 1, _T("Face Iteration Index") ) );
            channelInfo.push_back( MaxMeshInterface::channel_info(
                _T("FaceCornerIndex"), data_type_int32, 1,
                _T("Face Corner Iteration Index. There are N corners for a face containing N vertices.") ) );
            channelInfo.push_back(
                MaxMeshInterface::channel_info( _T("VertexIndex"), data_type_int32, 1, _T("Vertex Iteration Index") ) );
            break;
        }
    }

    frantic::max3d::mxs::frame<2> f;
    frantic::max3d::mxs::local<Array> result( f, new Array( 10 ) );
    frantic::max3d::mxs::local<Array> current( f );

    for( std::vector<MaxMeshInterface::channel_info>::const_iterator it = channelInfo.begin(),
                                                                     itEnd = channelInfo.end();
         it != itEnd; ++it ) {
        current = new Array( 4 );
        current->append( new String( it->get<0>().c_str() ) );
        current->append( new String( channel_data_type_str( it->get<1>() ) ) );
        current->append( Integer::intern( (int)it->get<2>() ) );
        current->append( new String( it->get<3>().c_str() ) );

        result->append( current );
    }
#if MAX_VERSION_MAJOR < 19
    return_protected( result.ptr() );
#else
    return_value( result.ptr() );
#endif
}

void GenomeMod::SetThreading( bool threads ) { this->m_isThreaded = threads; }

bool GenomeMod::GetThreading() const { return this->m_isThreaded; }

#if MAX_VERSION_MAJOR < 17
RefResult GenomeMod::ProcessRefTargMonitorMsg( Interval changeInt, RefTargetHandle /*hTarget*/, PartID& partID,
                                               RefMessage message, bool fromMonitoredTarget ) {
    BOOL propagate = TRUE;
#else
RefResult GenomeMod::ProcessRefTargMonitorMsg( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                               RefMessage message, bool fromMonitoredTarget, bool propagate,
                                               RefTargMonitorRefMaker* /*caller*/ ) {
#endif
    if( fromMonitoredTarget ) {
        if( message == REFMSG_CHANGE && ( partID & PART_TM ) != 0 ) {
            if( !m_nodeMonitor.suspendChange ) {
                m_nodeMonitor.suspendChange = true;
                NotifyDependents( changeInt, PART_OBJ, REFMSG_CHANGE, NOTIFY_ALL, propagate, this );
                m_nodeMonitor.suspendChange = false;
            }
        } else if( message == REFMSG_TARGET_DELETED ) {
            // Do nothing? We might want to see if there is another node we can connect to, but I'm worried about
            // badness happening during scene destruction
        }
    }

    return REF_SUCCEED;
}

int GenomeMod::ProcessEnumDependents( DependentEnumProc* dep ) {
    int res = 0;
    if( !m_nodeMonitor.suspendEnumDep ) {
        m_nodeMonitor.suspendEnumDep = true;
        res = DoEnumDependents( dep );
        m_nodeMonitor.suspendEnumDep = false;
    }

    return res;
}

void GenomeMod::BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev ) {
    m_isEditing = true;

    TimeValue t = ip->GetTime();

    NotifyDependents( Interval( t, t ), (PartID)PART_ALL, REFMSG_BEGIN_EDIT );
    NotifyDependents( Interval( t, t ), (PartID)PART_ALL, REFMSG_MOD_DISPLAY_ON );
    SetAFlag( A_MOD_BEING_EDITED );

    super_class::BeginEditParams( ip, flags, prev );
}

void GenomeMod::EndEditParams( IObjParam* ip, ULONG flags, Animatable* next ) {
    m_isEditing = false;

    TimeValue t = ip->GetTime();

    // NOTE: This flag must be cleared before sending the REFMSG_END_EDIT
    ClearAFlag( A_MOD_BEING_EDITED );
    NotifyDependents( Interval( t, t ), (PartID)PART_ALL, REFMSG_END_EDIT );
    NotifyDependents( Interval( t, t ), (PartID)PART_ALL, REFMSG_MOD_DISPLAY_OFF );

    super_class::EndEditParams( ip, flags, next );
}

BaseInterface* GenomeMod::GetInterface( Interface_ID id ) {
    if( id == Genome_INTERFACE )
        return static_cast<IGenome*>( this );
    if( BaseInterface* bi = IGenome::GetInterface( id ) )
        return bi;
    return GenericReferenceTarget<OSModifier, GenomeMod>::GetInterface( id );
}

namespace {
void update_mxs_ui( GenomeMod* genomeMod, const frantic::tstring& paramName ) {
    try {
        frantic::max3d::mxs::expression( _M( "try(GenomeMFEditor_Functions.OnGenomeParamChange genomeMod \"" ) +
                                         paramName + _M( "\")catch()" ) )
            .bind( _M( "genomeMod" ), genomeMod )
            .evaluate<void>();
    } catch( const std::exception& e ) {
        FF_LOG( error ) << frantic::strings::to_tstring( e.what() ) << std::endl;
    }
}
} // namespace

RefResult GenomeMod::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& partID,
                                       RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        int tabIndex;
        ParamID paramID = m_pblock->LastNotifyParamID( tabIndex );

        if( paramID == pbMagmaHolderRef ) {
            if( message == REFMSG_MAGMALEGACYCHANGE ) {
                update_mxs_ui( this, m_pblock->GetReferenceTarget( pbMagmaHolderRef )
                                         ->GetParamBlock( 0 )
                                         ->GetParamDef( (ParamID)partID )
                                         .int_name );

                // BUG: Is this proper usage of PartID ?!?!?
                if( partID == (PartID)kMagmaAutoUpdate ) {
                    if( IParamMap2* pm2 = m_pblock->GetMap() )
                        SendMessage( pm2->GetHWnd(), WM_USER_UPDATE, MAKELONG( IDC_MAGMAMODIFIER_AUTOUPDATE_CHECK, 0 ),
                                     0 );
                } else if( partID == (PartID)kMagmaAutomaticRenameOFF ) {
                    if( IParamMap2* pm2 = m_pblock->GetMap() )
                        SendMessage( pm2->GetHWnd(), WM_USER_UPDATE, MAKELONG( IDC_MAGMAMODIFIER_AUTORENAME_CHECK, 0 ),
                                     0 );
                }
                return REF_STOP;
            }
            return REF_SUCCEED;
        } else {
            if( paramID >= 0 ) // Stupid -1 paramID sometimes.
                update_mxs_ui( this, m_pblock->GetParamDef( paramID ).int_name );

            if( paramID == pbMeshIterationPattern ) {
                if( message == REFMSG_CHANGE ) {
                    NotifyDependents( FOREVER, partID, REFMSG_CHANGE );
                    NotifyDependents( FOREVER, 0,
                                      REFMSG_NUM_SUBOBJECTTYPES_CHANGED ); // Need to alert the subsel is different
                    return REF_STOP;
                }
            } else if( paramID == pbClampVertexSelection ) {
                if( std::char_traits<MCHAR>::compare( m_pblock->GetStr( pbMeshIterationPattern ),
                                                      IterationPatternNames[vertices],
                                                      std::numeric_limits<std::size_t>::max() ) == 0 &&
                    this->writes_to_channel( _T("Selection") ) )
                    return REF_SUCCEED;
                return REF_STOP;
            } else if( paramID == pbIterationCount ) {
                if( message == REFMSG_CHANGE )
                    return REF_SUCCEED;
            }
        }
    }

    return REF_DONTCARE;
}

IOResult GenomeMod::Load( ILoad* iload ) {
    class fix_pb_v0 : public PostLoadCallback {
        GenomeMod* m_obj;

      public:
        fix_pb_v0( GenomeMod* obj )
            : m_obj( obj ) {}

        virtual void proc( ILoad* /*iload*/ ) {
            M_STD_STRING iterPattern = m_obj->m_pblock->GetStr( pbMeshIterationPattern );

            // See if we need to remap the value.
            int index = -1;
            if( iterPattern == _M( "VERTEX" ) )
                index = vertices;
            else if( iterPattern == _M( "FACE" ) )
                index = faces;
            else if( iterPattern == _M( "CUSTOM FACE VERTEX" ) )
                index = face_corners;

            if( index >= 0 )
                m_obj->m_pblock->SetValue( pbMeshIterationPattern, TIME_NegInfinity,
                                           GenomeMod::IterationPatternNames[index] );

            delete this;
        }
    };

    IOResult r = super_class::Load( iload );
    if( r == IO_OK )
        iload->RegisterPostLoadCallback( new fix_pb_v0( this ) );
    return r;
}

void GenomeMod::BaseClone( ReferenceTarget* from, ReferenceTarget* to, RemapDir& remap ) {
    GenericReferenceTarget<OSModifier, GenomeMod>::BaseClone( from, to, remap );
}

CreateMouseCallBack* GenomeMod::GetCreateMouseCallBack( void ) { return NULL; }

#if MAX_VERSION_MAJOR >= 15
const
#endif

#if MAX_VERSION_MAJOR < 24
    TYPE_STRING_TYPE
    GenomeMod::GetObjectName() {
    return _T("Genome");
}
#else
TYPE_STRING_TYPE GenomeMod::GetObjectName( bool localized ) { return _T("Genome"); }
#endif

int GenomeMod::Display( TimeValue /*t*/, INode* /*inode*/, ViewExp* vpt, int /*flags*/, ModContext* mc ) {
    if( !mc->box || mc->box->IsEmpty() )
        return FALSE;

    GraphicsWindow* gw = vpt->getGW();

    gw->setColor( LINE_COLOR, GetUIColor( COLOR_GIZMOS ) );

    if( mc->box->pmin == mc->box->pmax ) {
        gw->marker( &mc->box->pmin, ASTERISK_MRKR );
    } else {
        frantic::max3d::DrawBox( gw, *mc->box );
    }

    return TRUE;
}

void GenomeMod::GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* /*vpt*/, Box3& box, ModContext* mc ) {
    if( !mc->box || mc->box->IsEmpty() )
        return;

    Matrix3 tm;
    if( inode )
        tm = inode->GetNodeTM( t );
    else
        tm.IdentityMatrix();

    box = ( *mc->box ) * tm;
}

ISubObjType* GenomeMod::GetSubObjType( int i ) {
    ISubObjType* result = NULL;

    switch( i ) {
    case -1: {
        M_STD_STRING iterPattern = m_pblock->GetStr( pbMeshIterationPattern );
        if( iterPattern == IterationPatternNames[vertices] ) {
            if( this->writes_to_channel( _T("Selection") ) )
                result = &GenomeMod::SOT_Vertex;
        } else if( iterPattern == IterationPatternNames[faces] ) {
            if( this->writes_to_channel( _T("FaceSelection") ) )
                result = &GenomeMod::SOT_Face;
        }
    } break;
    case 0:
        break;
    case 1:
        break;
    case 2:
        break;
    }

    return result;
}

Interval GenomeMod::LocalValidity( TimeValue t ) {
    if( m_isEditing )
        return NEVER;
    return GetValidity( t );
}

// TODO:  All channels currently flagged as potentially used/changed.  Can I get some info from the flow
//			and be more specific?
ChannelMask GenomeMod::ChannelsUsed() { return OBJ_CHANNELS; }

ChannelMask GenomeMod::ChannelsChanged() { return OBJ_CHANNELS; }

Class_ID GenomeMod::InputType() { return triObjectClassID; }

void GenomeMod::ModifyObject( TimeValue t, ModContext& mc, ObjectState* os, INode* node ) {
    using namespace frantic::max3d::geometry;
    using namespace frantic::geometry;
    using namespace frantic::magma::simple_compiler;
    using frantic::magma::magma_interface;

    if( !node )
        node = GetSceneNode( &mc );

    TriObject* tobj = NULL;

    if( os->obj->IsSubClassOf( triObjectClassID ) ) {
        tobj = static_cast<TriObject*>( os->obj );
        //}else if( os->obj->CanConvertToType(triObjectClassID) ){
        // os->obj = tobj = static_cast<TriObject*>( os->obj->ConvertToType(t, triObjectClassID) );
    } else {
        return;
    }

    Mesh* mesh = &tobj->GetMesh();

    if( m_pblock->GetInt( pbRemoveDegenerateFaces ) )
        mesh->RemoveDegenerateFaces();

    boost::shared_ptr<MaxMeshInterface> meshInterface( new MaxMeshInterface );
    meshInterface->set_mesh( mesh );

    bool setSelection = false;
    M_STD_STRING meshIter = IterationPatternNames[vertices];

    ReferenceTarget* magmaHolder = m_pblock->GetReferenceTarget( pbMagmaHolderRef );
    if( magmaHolder != NULL ) {
        try {
            boost::shared_ptr<GenomeContext> ctx( new GenomeContext );

            ctx->set_time( t );
            ctx->set_node( node );
            ctx->set_modcontext( mc );
            ctx->set_mesh( *mesh );

            boost::shared_ptr<magma_interface> magmaInterface;
            if( frantic::magma::max3d::IMagmaHolder* iHolder =
                    frantic::magma::max3d::GetMagmaHolderInterface( magmaHolder ) )
                magmaInterface = iHolder->get_magma_interface();

            if( magmaInterface ) {
                simple_mesh_compiler compiler;

                compiler.set_abstract_syntax_tree( magmaInterface );
                compiler.set_compilation_context( ctx );
                compiler.set_threading( this->GetThreading() );
                compiler.set_mesh_interface( meshInterface );

                meshIter = m_pblock->GetStr( pbMeshIterationPattern );
                if( meshIter == IterationPatternNames[vertices] ) {
                    compiler.set_iteration_pattern( simple_mesh_compiler::VERTEX );
                    setSelection = writes_to_channel( _M( "Selection" ) );
                } else if( meshIter == IterationPatternNames[faces] ) {
                    compiler.set_iteration_pattern( simple_mesh_compiler::FACE );
                    setSelection = writes_to_channel( _M( "FaceSelection" ) );
                } else if( meshIter == IterationPatternNames[face_corners] ) {
                    compiler.set_iteration_pattern( simple_mesh_compiler::VERTEX_CUSTOM_FACES );
                } else
                    throw frantic::magma::magma_exception() << frantic::magma::magma_exception::error_name(
                        _M( "Unknown mesh iteration pattern '" ) + meshIter + _M( "'" ) );

                compiler.build();

                for( int i = 0, iEnd = m_pblock->GetInt( pbIterationCount, t ); i < iEnd; ++i ) {
                    compiler.eval();

                    // Any channels that had buffered writes should be comitted.
                    meshInterface->commit_writes();
                }
            }

            clear_error();
        } catch( const frantic::magma::magma_exception& e ) {
            set_error( e );
        } catch( const std::exception& e ) {
            set_error( frantic::strings::to_tstring( e.what() ) );
        }
    }

    Interval localValidity = GetValidity( t );

    if( node )
        node->GetNodeTM( t, &localValidity );

    if( !localValidity.InInterval( t ) )
        localValidity.SetInstant( t );

    if( setSelection ) {
        mesh->dispFlags = 0; // Clear all display flags.
        mesh->ClearFlag( MESH_SMOOTH_SUBSEL );

        if( meshIter == IterationPatternNames[vertices] ) {
            // Set the vertex selection bit array values.
            float* vertSel = mesh->getVSelectionWeights(); // This can end up NULL (ex. an exception during the
                                                           // expression compilation)
            if( vertSel != NULL ) {
                if( m_pblock->GetInt( pbClampVertexSelection ) ) {
                    for( int i = 0, iEnd = mesh->getNumVerts(); i < iEnd; ++i, ++vertSel ) {
                        if( *vertSel >= 1.f ) {
                            *vertSel = 1.f;
                            mesh->VertSel().Set( i );
                        } else {
                            if( *vertSel < 0 )
                                *vertSel = 0;
                            mesh->VertSel().Clear( i );
                        }
                    }
                } else {
                    for( int i = 0, iEnd = mesh->getNumVerts(); i < iEnd; ++i, ++vertSel )
                        mesh->VertSel().Set( i, *vertSel ==
                                                    1.f ); // This is sketchy, but its how Max modifiers seem to work
                                                           // (via inspection of source code in SDK & DEBUG build).
                }
            }

            mesh->selLevel = MESH_VERTEX;
            mesh->SetDispFlag( DISP_VERTTICKS | DISP_SELVERTS );

            os->obj->UpdateValidity( DISP_ATTRIB_CHAN_NUM, localValidity );
            os->obj->UpdateValidity( SUBSEL_TYPE_CHAN_NUM, localValidity );
            os->obj->UpdateValidity( SELECT_CHAN_NUM, localValidity );
        } else {
            mesh->selLevel = MESH_FACE;
            mesh->SetFlag( MESH_SMOOTH_SUBSEL ); // Maybe this draws the faces fancy-like. Maybe we should only do this
                                                 // if our modifier is being edited.
            mesh->SetDispFlag( DISP_SELPOLYS );

            os->obj->UpdateValidity( DISP_ATTRIB_CHAN_NUM, localValidity );
            os->obj->UpdateValidity( SUBSEL_TYPE_CHAN_NUM, localValidity );
            os->obj->UpdateValidity( SELECT_CHAN_NUM, localValidity );
        }
    }

    // TODO: Determine which minimum channels I need to update
    os->obj->UpdateValidity( GEOM_CHAN_NUM, localValidity );
    os->obj->UpdateValidity( TEXMAP_CHAN_NUM, localValidity );
    os->obj->UpdateValidity( VERT_COLOR_CHAN_NUM, localValidity );
}

void GenomeMod::EvaluateDebug( TimeValue t, ModContext& mc, ObjectState* os, INode* node,
                               std::vector<frantic::magma::debug_data>& outVals ) {
    using namespace frantic::max3d::geometry;
    using namespace frantic::magma::simple_compiler;
    using frantic::magma::magma_interface;

    if( !os->obj->CanConvertToType( triObjectClassID ) )
        return;

    TriObject* tobj = (TriObject*)os->obj->ConvertToType( t, triObjectClassID );
    BOOL needsDelete = ( tobj != os->obj );

    BOOST_SCOPE_EXIT( (needsDelete)( tobj ) ) {
        if( needsDelete && tobj )
            tobj->MaybeAutoDelete();
    }
    BOOST_SCOPE_EXIT_END

    Mesh* mesh = &tobj->GetMesh();
    if( m_pblock->GetInt( pbRemoveDegenerateFaces ) )
        mesh->RemoveDegenerateFaces();

    boost::shared_ptr<MaxMeshInterface> meshInterface( new MaxMeshInterface );
    meshInterface->set_mesh( mesh );

    M_STD_STRING meshIter = IterationPatternNames[vertices];

    ReferenceTarget* magmaHolder = m_pblock->GetReferenceTarget( pbMagmaHolderRef );
    if( magmaHolder != NULL ) {
        boost::shared_ptr<GenomeContext> ctx( new GenomeContext );

        ctx->set_time( t );
        ctx->set_node( node );
        ctx->set_modcontext( mc );
        ctx->set_mesh( *mesh );

        boost::shared_ptr<magma_interface> magmaInterface;
        if( frantic::magma::max3d::IMagmaHolder* iHolder =
                frantic::magma::max3d::GetMagmaHolderInterface( magmaHolder ) )
            magmaInterface = iHolder->get_magma_interface();

        if( magmaInterface ) {
            simple_mesh_compiler compiler;

            compiler.set_abstract_syntax_tree( magmaInterface );
            compiler.set_compilation_context( ctx );
            compiler.set_threading( this->GetThreading() );
            compiler.set_mesh_interface( meshInterface );

            meshIter = m_pblock->GetStr( pbMeshIterationPattern );
            if( meshIter == IterationPatternNames[vertices] ) {
                compiler.set_iteration_pattern( simple_mesh_compiler::VERTEX );
            } else if( meshIter == IterationPatternNames[faces] ) {
                compiler.set_iteration_pattern( simple_mesh_compiler::FACE );
            } else if( meshIter == IterationPatternNames[face_corners] ) {
                compiler.set_iteration_pattern( simple_mesh_compiler::VERTEX_CUSTOM_FACES );
            } else
                throw frantic::magma::magma_exception() << frantic::magma::magma_exception::error_name(
                    _M( "Unknown mesh iteration pattern '" ) + meshIter + _M( "'" ) );

            compiler.build();
            compiler.eval_debug( outVals, (std::size_t)std::max( GetMaximumDebuggerIterations(), 0 ) );
        }
    }
}

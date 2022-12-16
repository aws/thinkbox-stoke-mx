// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource_ember.h"

#include <stoke/max3d/EmberPipeObject.hpp>

#include <ember/concatenated_field.hpp>
#include <ember/openvdb.hpp>

#pragma warning( disable : 4503 ) // "decorated name length exceeded, name was truncated" casued by OpenVDB template
                                  // instantiations.

#include <krakatoa/max3d/PRTObject.hpp>

#include <frantic/max3d/convert.hpp>
#include <frantic/max3d/fnpublish/MixinInterface.hpp>
#include <frantic/max3d/maxscript/includes.hpp>
#include <frantic/max3d/maxscript/maxscript.hpp>
#include <frantic/max3d/maxscript/shared_value_ptr.hpp>
#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>
#include <frantic/max3d/volumetrics/IEmberField.hpp>

#include <krakatoa/particle_volume.hpp>

#include <frantic/graphics/poisson_sphere.hpp>
#include <frantic/particles/streams/apply_function_particle_istream.hpp>
#include <frantic/particles/streams/empty_particle_istream.hpp>
#include <frantic/particles/streams/fractional_particle_istream.hpp>
#include <frantic/particles/streams/selection_culled_particle_istream.hpp>
#include <frantic/particles/streams/set_channel_particle_istream.hpp>
#include <frantic/volumetrics/levelset/rle_trilerp.hpp>

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#pragma warning( push, 3 )
#include <boost/bind.hpp>
#include <boost/random.hpp>
#pragma warning( pop )

#include <iparamm2.h>

#define PRTEmberBase_CLASS_ID Class_ID( 0x34d96c50, 0x7f532d01 )
#define PRTEmberBase_CLASS_NAME _T("PRT Field")
#define PRTEmberBase_CLASS_DISPLAYNAME _T("PRTField")
#define PRTEmberBase_VERSION 1

#define PRTEmberBase_INTERFACE Interface_ID( 0x24b958e5, 0x13aa75b2 )

namespace ember {
namespace max3d {

extern Mesh* GetPRTEmberIconMesh();
#if MAX_VERSION_MAJOR >= 25
extern MaxSDK::SharedMeshPtr GetPRTEmberIconMeshShared();
#endif

namespace {
enum { kMainBlock };

enum { kSourceRollout, kSeedingRollout, kViewportRollout };

enum {
    kTargetNode,
    kRandomSeed,
    kRandomCount,
    kSubdivideEnabled,
    kSubdivideCount,
    kJitterSample,
    kJitterWellDistributed,
    kMultiplePerRegion,
    kMultipleCount,
    kViewportEnabled,
    kViewportDisableSubdivision,
    kViewportUsePercentage,
    kViewportPercentage,
    kViewportUseLimit,
    kViewportLimit,
    kViewportIgnoreMaterial,
    kIconSize,
    kDensityChannel,
    kDensityMin,
    // TODO: Parameters for controlling which channels to seed in or make available.
};
} // namespace

using frantic::particles::particle_istream_ptr;

ClassDesc2* GetPRTEmberBaseDesc();

class IPRTEmberBase : public frantic::max3d::fnpublish::MixinInterface<IPRTEmberBase> {
  public:
    virtual ~IPRTEmberBase() {}

    virtual void ReduceViewportCaches() = 0;

    static ThisInterfaceDesc* GetStaticDesc() {
        static ThisInterfaceDesc theDesc( PRTEmberBase_INTERFACE, _T("PRTEmberBase"), 0 );

        if( theDesc.empty() ) { // ie. Check if we haven't SetRollout the descriptor yet
            theDesc.SetClassDesc( GetPRTEmberBaseDesc() );

            theDesc.function( _T("ReduceViewportCaches"), &IPRTEmberBase::ReduceViewportCaches );
        }

        return &theDesc;
    }

    virtual ThisInterfaceDesc* GetDesc() { return IPRTEmberBase::GetStaticDesc(); }
};

class PRTEmberBase : public krakatoa::max3d::PRTObject<PRTEmberBase>, public IPRTEmberBase {
    struct sampler_settings {
        unsigned int subdivCount;
        bool doJitter;
        int numPerVoxel;
        unsigned int randomSeed;
        size_t randomCount;
        bool jitterWellDistributed;
    } m_cachedSamplerSettings; // Cache the sampler settings so we don't have to recreate our voxel sampler.

    boost::shared_ptr<krakatoa::particle_volume_voxel_sampler> m_cachedSampler;

  protected:
    virtual Mesh* GetIconMesh( Matrix3& outTM );
#if MAX_VERSION_MAJOR >= 25
    virtual MaxSDK::SharedMeshPtr GetIconMeshShared( Matrix3& outTM );
#endif
    virtual ClassDesc2* GetClassDesc();

  public:
    PRTEmberBase();
    virtual ~PRTEmberBase() {}

    virtual void ReduceViewportCaches();

    // From PRTObject
    virtual void SetIconSize( float size );

    virtual void GetStreamNativeChannels( INode* pNode, TimeValue t, frantic::channels::channel_map& outChannelMap );
    virtual particle_istream_ptr GetInternalStream( INode* pNode, TimeValue t, Interval& outValidity,
                                                    boost::shared_ptr<IEvalContext> pEvalContext );

    // From Animatable
    virtual BaseInterface* GetInterface( Interface_ID id );
    virtual void BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev = NULL );
    virtual void EndEditParams( IObjParam* ip, ULONG flags, Animatable* next = NULL );

    // From ReferenceMaker
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

    // From BaseObject
#if MAX_VERSION_MAJOR < 24
    virtual TYPE_STRING_TYPE GetObjectName();
#else
    virtual TYPE_STRING_TYPE GetObjectName( bool localized );
#endif

    // From Object
    virtual void InitNodeName( MSTR& s );
    virtual void GetDeformBBox( TimeValue t, Box3& box, Matrix3* tm = NULL, BOOL useSel = FALSE );
    virtual Interval ObjectValidity( TimeValue t );

  private:
    friend class PRTEmberDlgProc;

    void get_available_channels( TimeValue t, std::vector<frantic::tstring>& outChannels );
};

class PRTEmberBaseDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_pbDesc;

  public:
    PRTEmberBaseDesc();
    virtual ~PRTEmberBaseDesc();

    int IsPublic() { return TRUE; }
    void* Create( BOOL /*loading*/ ) { return new PRTEmberBase; }
    const TCHAR* ClassName() { return PRTEmberBase_CLASS_NAME; }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return PRTEmberBase_CLASS_ID; }
    const TCHAR* Category() { return _T("Stoke"); }

    const TCHAR* InternalName() {
        return PRTEmberBase_CLASS_DISPLAYNAME;
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return PRTEmberBase_CLASS_NAME; }
#endif
};

ClassDesc2* GetPRTEmberBaseDesc() {
    static PRTEmberBaseDesc theDesc;
    return &theDesc;
}

class PRTEmberValidator : public PBValidator {
  public:
    static PRTEmberValidator* GetInstance() {
        static PRTEmberValidator thePRTEmberValidator;
        return &thePRTEmberValidator;
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

class PRTEmberDlgProc : public ParamMap2UserDlgProc {
  public:
    static PRTEmberDlgProc& GetInstance();

    virtual INT_PTR DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

    virtual void DeleteThis() {}
};

PRTEmberDlgProc& PRTEmberDlgProc::GetInstance() {
    static PRTEmberDlgProc theFieldTexmapDlgProc;
    return theFieldTexmapDlgProc;
}

#define WM_UPDATE_LIST ( WM_APP + 0xd3d )

INT_PTR PRTEmberDlgProc::DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
    switch( msg ) {
    case WM_INITDIALOG:
        if( IParamBlock2* pb = map->GetParamBlock() ) {
            if( HWND hWndCombo = GetDlgItem( hWnd, IDC_PRTEMBER_DENSITYCHANNEL_COMBO ) ) {
                ComboBox_SetCueBannerText( hWndCombo, _T("Select a channel ...") );

                if( PRTEmberBase* pe = static_cast<PRTEmberBase*>( pb->GetOwner() ) ) {
                    std::vector<frantic::tstring> options;

                    pe->get_available_channels( t, options );

                    for( std::vector<frantic::tstring>::const_iterator it = options.begin(), itEnd = options.end();
                         it != itEnd; ++it )
                        ComboBox_AddString( hWndCombo, it->c_str() );
                }

                const MCHAR* curText = pb->GetStr( kDensityChannel );

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
        if( LOWORD( wParam ) == IDC_PRTEMBER_DENSITYCHANNEL_COMBO ) {
            if( HIWORD( wParam ) == CBN_SELCHANGE ) {
                TCHAR curText[64];

                ComboBox_GetText( reinterpret_cast<HWND>( lParam ), curText, 64 );

                if( IParamBlock2* pb = map->GetParamBlock() )
                    pb->SetValue( kDensityChannel, 0, curText );

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

                frantic::max3d::mxs::expression( _T("try(::StokeContextMenuStruct.OpenPRTFieldMenu owner)catch()") )
                    .bind( _T("owner"), rtarg )
                    .evaluate<void>();

                return TRUE;
            }
        }
        break;
    case WM_UPDATE_LIST:
        if( HWND hWndCombo = GetDlgItem( hWnd, IDC_PRTEMBER_DENSITYCHANNEL_COMBO ) ) {
            ComboBox_ResetContent( hWndCombo );

            if( IParamBlock2* pb = map->GetParamBlock() ) {
                if( PRTEmberBase* pe = static_cast<PRTEmberBase*>( pb->GetOwner() ) ) {
                    std::vector<frantic::tstring> options;

                    pe->get_available_channels( t, options );

                    for( std::vector<frantic::tstring>::const_iterator it = options.begin(), itEnd = options.end();
                         it != itEnd; ++it )
                        ComboBox_AddString( hWndCombo, it->c_str() );
                }

                const MCHAR* curText = pb->GetStr( kDensityChannel );

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

PRTEmberBaseDesc::PRTEmberBaseDesc()
    : m_pbDesc( kMainBlock, _T("Parameters"), 0, PRTEmberBase_VERSION ) {
    m_pbDesc.OwnerClassDesc( this );
    m_pbDesc.OwnerRefNum( 0 );

    m_pbDesc.RolloutTemplate( kSourceRollout, IDD_PRTEMBER_SOURCE, IDS_PRTEMBER_SOURCE_TITLE,
                              &PRTEmberDlgProc::GetInstance() );
    m_pbDesc.RolloutTemplate( kSeedingRollout, IDD_PRTEMBER_SEEDING, IDS_PRTEMBER_SEEDING_TITLE );
    m_pbDesc.RolloutTemplate( kViewportRollout, IDD_PRTEMBER_VIEWPORT, IDS_PRTEMBER_VIEWPORT_TITLE );

    m_pbDesc.Parameter<INode*>( kTargetNode, _T("TargetNode"), 0 )
        .Validator( PRTEmberValidator::GetInstance() )
        .PickNodeButtonUI( kSourceRollout, IDC_PRTEMBER_SOURCE_BUTTON, IDS_PRTEMBER_SOURCE_CAPTION );

    m_pbDesc.Parameter<int>( kRandomSeed, _T("RandomSeed"), IDS_RANDOMSEED, true )
        .Range( 0, INT_MAX )
        .DefaultValue( 1234 )
        .SpinnerUI( kSeedingRollout, EDITTYPE_POS_INT, IDC_PRTEMBER_SEED_EDIT, IDC_PRTEMBER_SEED_SPIN, 1 );

    m_pbDesc.Parameter<int>( kRandomCount, _T("RandomCount"), 0 )
        .Range( 1, INT_MAX )
        .DefaultValue( 1024 )
        .SpinnerUI( kSeedingRollout, EDITTYPE_POS_INT, IDC_PRTEMBER_DISTINCTVALUES_EDIT,
                    IDC_PRTEMBER_DISTINCTVALUES_SPIN, 1 );

    m_pbDesc.Parameter<bool>( kSubdivideEnabled, _T("SubdivideEnabled"), 0 )
        .DefaultValue( true )
        .CheckBoxUI( kSeedingRollout, IDC_PRTEMBER_SUBDIVIDEREGION_CHECK );

    m_pbDesc.Parameter<int>( kSubdivideCount, _T("SubdivideCount"), 0 )
        .Range( 1, INT_MAX )
        .DefaultValue( 1 )
        .SpinnerUI( kSeedingRollout, EDITTYPE_POS_INT, IDC_PRTEMBER_SUBDIVISIONS_EDIT, IDC_PRTEMBER_SUBDIVISIONS_SPIN,
                    1 );

    m_pbDesc.Parameter<bool>( kJitterSample, _T("JitterSamples"), 0 )
        .DefaultValue( true )
        .CheckBoxUI( kSeedingRollout, IDC_PRTEMBER_JITTERSAMPLES_CHECK );

    m_pbDesc.Parameter<bool>( kJitterWellDistributed, _T("JitterWellDistributed"), 0 )
        .DefaultValue( false )
        .CheckBoxUI( kSeedingRollout, IDC_PRTEMBER_WELLDISTRIBUTED_CHECK );

    m_pbDesc.Parameter<bool>( kMultiplePerRegion, _T("MultiplePerRegion"), 0 )
        .DefaultValue( false )
        .CheckBoxUI( kSeedingRollout, IDC_PRTEMBER_MULTIPLEPERREGION_CHECK );

    m_pbDesc.Parameter<int>( kMultipleCount, _T("MultiplePerRegionCount"), 0 )
        .Range( 2, INT_MAX )
        .DefaultValue( 2 )
        .SpinnerUI( kSeedingRollout, EDITTYPE_POS_INT, IDC_PRTEMBER_COUNT_EDIT, IDC_PRTEMBER_COUNT_SPIN, 1 );

    m_pbDesc.Parameter<bool>( kViewportEnabled, _T("ViewportEnabled"), 0 )
        .DefaultValue( true )
        .CheckBoxUI( kViewportRollout, IDC_PRTEMBER_ENABLEDINVIEWPORT_CHECK );

    m_pbDesc.Parameter<bool>( kViewportDisableSubdivision, _T("ViewportDisableSubdivision"), 0 )
        .DefaultValue( true )
        .CheckBoxUI( kViewportRollout, IDC_PRTEMBER_DISABLESUBDIVISION_CHECK );

    m_pbDesc.Parameter<bool>( kViewportUsePercentage, _T("ViewportUsePercentage"), 0 )
        .DefaultValue( true )
        .CheckBoxUI( kViewportRollout, IDC_PRTEMBER_PERCENTAGE_CHECK );

    m_pbDesc.Parameter<float>( kViewportPercentage, _T("ViewportPercentage"), 0 )
        .Range( 0.f, 100.f )
        .DefaultValue( 1.f )
        .SpinnerUI( kViewportRollout, EDITTYPE_POS_FLOAT, IDC_PRTEMBER_PERCENTAGE_EDIT, IDC_PRTEMBER_PERCENTAGE_SPIN,
                    boost::optional<float>() );

    m_pbDesc.Parameter<bool>( kViewportUseLimit, _T("ViewportUseLimit"), 0 )
        .DefaultValue( true )
        .CheckBoxUI( kViewportRollout, IDC_PRTEMBER_LIMIT_CHECK );

    m_pbDesc.Parameter<float>( kViewportLimit, _T("ViewportLimit"), 0 )
        .Range( 0.f, FLT_MAX )
        .DefaultValue( 1000.f )
        .SpinnerUI( kViewportRollout, EDITTYPE_POS_FLOAT, IDC_PRTEMBER_LIMIT_EDIT, IDC_PRTEMBER_LIMIT_SPIN,
                    boost::optional<float>() );

    m_pbDesc.Parameter<bool>( kViewportIgnoreMaterial, _T("ViewportIgnoreMaterial"), 0 ).DefaultValue( false );

    m_pbDesc.Parameter<float>( kIconSize, _T("IconSize"), 0 )
        .Range( 0.f, FLT_MAX )
        .DefaultValue( 30.f )
        .SpinnerUI( kViewportRollout, EDITTYPE_POS_FLOAT, IDC_PRTEMBER_ICONSIZE_EDIT, IDC_PRTEMBER_ICONSIZE_SPIN,
                    boost::optional<float>() );

    m_pbDesc.Parameter<const MCHAR*>( kDensityChannel, _T("DensityChannel"), 0 ).DefaultValue( _T("") );
    //.EditBoxUI( kSourceRollout, IDC_PRTEMBER_DENSITYCHANNEL_EDIT );

    m_pbDesc.Parameter<float>( kDensityMin, _T("DensityMin"), IDS_DENSITYMIN, true )
        .Range( -FLT_MAX, FLT_MAX )
        .DefaultValue( 0.f )
        .SpinnerUI( kSourceRollout, EDITTYPE_FLOAT, IDC_PRTEMBER_THRESHOLD_EDIT, IDC_PRTEMBER_THRESHOLD_SPIN,
                    boost::optional<float>() );
}

PRTEmberBaseDesc::~PRTEmberBaseDesc() {}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

ClassDesc2* PRTEmberBase::GetClassDesc() { return GetPRTEmberBaseDesc(); }

Mesh* PRTEmberBase::GetIconMesh( Matrix3& outTM ) {
    float iconSize = m_pblock->GetFloat( kIconSize );

    outTM.SetScale( Point3( iconSize, iconSize, iconSize ) );
    return ember::max3d::GetPRTEmberIconMesh();
}

#if MAX_VERSION_MAJOR >= 25
MaxSDK::SharedMeshPtr PRTEmberBase::GetIconMeshShared( Matrix3& outTM ) {
    float iconSize = m_pblock->GetFloat( kIconSize );

    outTM.SetScale( Point3( iconSize, iconSize, iconSize ) );
    return ember::max3d::GetPRTEmberIconMeshShared();
}
#endif

// From PRTEmberBase
PRTEmberBase::PRTEmberBase() {
    for( int i = 0, iEnd = NumRefs(); i < iEnd; ++i )
        SetReference( i, NULL );

    GetPRTEmberBaseDesc()->MakeAutoParamBlocks( this );
}

void PRTEmberBase::ReduceViewportCaches() { this->ClearViewportCaches(); }

void PRTEmberBase::SetIconSize( float size ) { m_pblock->SetValue( kIconSize, 0, size ); }

class ember_field_particle_istream : public frantic::particles::streams::particle_istream {
  public:
    ember_field_particle_istream( const frantic::channels::channel_map& outMap );

    virtual ~ember_field_particle_istream();

    void set_field( const boost::shared_ptr<frantic::volumetrics::field_interface>& pField );

    void set_domain( int bounds[], float spacing );

    void set_sampler( const boost::shared_ptr<krakatoa::particle_volume_voxel_sampler>& pSampler );

    void set_density_field( const frantic::tstring& channelName );

    void set_density_compensation( bool enabled );

    void set_minimum_density( float minDensity );

  public:
    virtual void close();

    // The stream can return its filename or other identifier for better error messages.
    virtual frantic::tstring name() const;

    // TODO: We should add a verbose_name function, which all wrapping streams are required to mark up in some way

    // This is the size of the particle structure which will be loaded, in bytes.
    virtual std::size_t particle_size() const;

    // Returns the number of particles, or -1 if unknown
    virtual boost::int64_t particle_count() const;
    virtual boost::int64_t particle_index() const;
    virtual boost::int64_t particle_count_left() const;

    virtual boost::int64_t particle_progress_count() const;
    virtual boost::int64_t particle_progress_index() const;

    // If a stream does not know how many particles it has, it can optionally override this function
    // to produce a guess of how many there will be. This guess will be used to pre-allocate storage
    // for this many particles if the user is concerned about memory performance.
    virtual boost::int64_t particle_count_guess() const;

    // This allows you to change the particle layout that's being loaded on the fly, in case it couldn't
    // be set correctly at creation time.
    virtual void set_channel_map( const frantic::channels::channel_map& particleChannelMap );

    // This is the particle channel map which specifies the byte layout of the particle structure that is being used.
    virtual const frantic::channels::channel_map& get_channel_map() const;

    // This is the particle channel map which specifies the byte layout of the input to this stream.
    // NOTE: This value is allowed to change after the following conditions:
    //    * set_channel_map() is called (for example, the empty_particle_istream equates the native map with the
    //    external map)
    virtual const frantic::channels::channel_map& get_native_channel_map() const;

    /** This provides a default particle which should be used to fill in channels of the requested channel map
     *	which are not supplied by the native channel map.
     *	IMPORTANT: Make sure the buffer you pass in is at least as big as particle_size() bytes.
     */
    virtual void set_default_particle( char* rawParticleBuffer );

    // This reads a particle into a buffer matching the channel_map.
    // It returns true if a particle was read, false otherwise.
    // IMPORTANT: Make sure the buffer you pass in is at least as big as particle_size() bytes.
    virtual bool get_particle( char* rawParticleBuffer );

    // This reads a group of particles. Returns false if the end of the source
    // was reached during the read.
    virtual bool get_particles( char* rawParticleBuffer, std::size_t& numParticles );

  private:
    bool advance_iterator();

  private:
    boost::shared_ptr<frantic::volumetrics::field_interface> m_pField;
    boost::shared_ptr<krakatoa::particle_volume_voxel_sampler> m_pSampler;
    int m_min[3], m_max[3], m_cur[3];
    float m_curP[3];
    float m_spacing;
    float m_minDensity;
    bool m_compensateDensity;

    frantic::tstring m_densityChannel;

    frantic::diagnostics::profiling_section m_profiler;

  private:
    boost::int64_t m_sampleIndex,
        m_maxSamples; // Records the number of samples made, and the maximum number (ie. m_size[0]*m_size[1]*m_size[2]).
    boost::int64_t m_particleIndex; // Records the number of particles generated so far.

    frantic::channels::channel_map m_outMap, m_nativeMap;
    frantic::channels::channel_map_adaptor m_outAdaptor;
    frantic::channels::channel_accessor<frantic::graphics::vector3f>
        m_posAccessor; // Access the position value *after* adapting to the output stream.
    frantic::channels::channel_cvt_accessor<float>
        m_densityAccessor; // Access the density value *before* adapting to the output stream.

    boost::scoped_array<char> m_defaultParticle;
    boost::scoped_array<char> m_cachedData;
    std::size_t m_cacheStride;

  private:
    // void do_get_particles( const voxel_range& range, lockable_buffer& buffer ) const ;
};

void PRTEmberBase::GetStreamNativeChannels( INode* /*pNode*/, TimeValue t,
                                            frantic::channels::channel_map& outChannelMap ) {
    outChannelMap.reset();

    INode* pFieldNode = m_pblock->GetINode( kTargetNode );
    if( pFieldNode != NULL ) {
        ObjectState os = pFieldNode->EvalWorldState( t );

        if( boost::shared_ptr<ember::max3d::EmberPipeObject> pPipeObj =
                frantic::max3d::safe_convert_to_type<ember::max3d::EmberPipeObject>( os.obj, t,
                                                                                     EmberPipeObject_CLASSID ) ) {
            outChannelMap.define_channel<frantic::graphics::vector3f>( _T("Position") );
            outChannelMap.union_channel_map( pPipeObj->GetChannels() );
        }
    }

    outChannelMap.end_channel_definition();
}

particle_istream_ptr PRTEmberBase::GetInternalStream( INode* /*pNode*/, TimeValue t, Interval& outValidity,
                                                      boost::shared_ptr<IEvalContext> pEvalContext ) {
    bool enabledInViewport = m_pblock->GetInt( kViewportEnabled, t ) != FALSE;
    if( !enabledInViewport && !m_inRenderMode )
        return boost::make_shared<frantic::particles::streams::empty_particle_istream>(
            pEvalContext->GetDefaultChannels() );

    INode* pFieldNode = m_pblock->GetINode( kTargetNode );

    const MCHAR* szDensityChannel = m_pblock->GetStr( kDensityChannel );

    if( !pFieldNode || !szDensityChannel || szDensityChannel[0] == _T( '\0' ) )
        return boost::make_shared<frantic::particles::streams::empty_particle_istream>(
            pEvalContext->GetDefaultChannels() );

    boost::shared_ptr<frantic::particles::streams::particle_istream> pin;

    unsigned subdivCount = 0;
    if( m_pblock->GetInt( kSubdivideEnabled ) &&
        ( m_inRenderMode || !m_pblock->GetInt( kViewportDisableSubdivision ) ) )
        subdivCount = static_cast<unsigned>( std::max( 1, m_pblock->GetInt( kSubdivideCount ) ) );

    bool doJitter = ( m_pblock->GetInt( kJitterSample ) != FALSE );
    bool jitterWellDistributed = ( m_pblock->GetInt( kJitterWellDistributed ) != FALSE );

    int numPerVoxel = 1;
    if( doJitter && m_pblock->GetInt( kMultiplePerRegion ) &&
        ( m_inRenderMode || !m_pblock->GetInt( kViewportDisableSubdivision ) ) )
        numPerVoxel = std::max( 2, m_pblock->GetInt( kMultipleCount ) );

    std::size_t requestedSamples = static_cast<std::size_t>( m_pblock->GetInt( kRandomCount ) );
    if( static_cast<std::size_t>( numPerVoxel ) > requestedSamples )
        throw std::runtime_error( "The number of samples per voxel is larger than the number of available random "
                                  "values. Please increase the number of potential random values." );

    unsigned randomSeed = static_cast<unsigned>( m_pblock->GetInt( kRandomSeed, t ) );

    // If !doJitter, only subdivCount & numPerVoxel are used.
    if( !m_cachedSampler || m_cachedSamplerSettings.doJitter != doJitter ||
        m_cachedSamplerSettings.subdivCount != subdivCount || m_cachedSamplerSettings.numPerVoxel != numPerVoxel ||
        ( doJitter && ( m_cachedSamplerSettings.jitterWellDistributed != jitterWellDistributed ||
                        m_cachedSamplerSettings.randomCount != requestedSamples ||
                        m_cachedSamplerSettings.randomSeed != randomSeed ) ) ) {
        m_cachedSampler = krakatoa::get_particle_volume_voxel_sampler( subdivCount, doJitter, numPerVoxel, randomSeed,
                                                                       requestedSamples, jitterWellDistributed );

        m_cachedSamplerSettings.subdivCount = subdivCount;
        m_cachedSamplerSettings.doJitter = doJitter;
        m_cachedSamplerSettings.jitterWellDistributed = jitterWellDistributed;
        m_cachedSamplerSettings.numPerVoxel = numPerVoxel;
        m_cachedSamplerSettings.randomCount = requestedSamples;
        m_cachedSamplerSettings.randomSeed = randomSeed;
    }

    ObjectState os = pFieldNode->EvalWorldState( t );

    if( boost::shared_ptr<ember::max3d::EmberPipeObject> pPipeObj =
            frantic::max3d::safe_convert_to_type<ember::max3d::EmberPipeObject>( os.obj, t,
                                                                                 EmberPipeObject_CLASSID ) ) {
        frantic::graphics::boundbox3f bounds = frantic::max3d::from_max_t( pPipeObj->GetBounds() );
        float spacing = pPipeObj->GetDefaultSpacing();

        boost::shared_ptr<ember_field_particle_istream> result =
            boost::make_shared<ember_field_particle_istream>( pEvalContext->GetDefaultChannels() );

        openvdb::CoordBBox coordBBox = ember::get_ijk_inner_bounds( bounds.minimum(), bounds.maximum(), spacing );

        int voxelBounds[6];
        coordBBox.min().asXYZ( voxelBounds[0], voxelBounds[2], voxelBounds[4] );
        coordBBox.max().offsetBy( 1 ).asXYZ( voxelBounds[1], voxelBounds[3], voxelBounds[5] );

        float minDensity = m_pblock->GetFloat( kDensityMin, t );

        result->set_field( pPipeObj->GetFuture()->get() );
        result->set_sampler( m_cachedSampler );
        result->set_domain( voxelBounds, spacing );
        result->set_minimum_density( minDensity );
        result->set_density_field( szDensityChannel );

        pin = result;
    }

    if( !pin )
        return boost::make_shared<frantic::particles::streams::empty_particle_istream>(
            pEvalContext->GetDefaultChannels() );

    if( !m_inRenderMode ) {
        float loadPercentage = 1.f;
        if( m_pblock->GetInt( kViewportUsePercentage ) )
            loadPercentage = m_pblock->GetFloat( kViewportPercentage ) / 100.f;

        boost::int64_t loadLimit = std::numeric_limits<boost::int64_t>::max();
        if( m_pblock->GetInt( kViewportUseLimit ) )
            loadLimit = ( boost::int64_t )( 1000.0 * (double)m_pblock->GetFloat( kViewportLimit ) );

        pin = frantic::particles::streams::apply_fractional_particle_istream( pin, loadPercentage, loadLimit, true );
    }

    outValidity.SetInstant( t );

    return pin;
}

// From Animatable
BaseInterface* PRTEmberBase::GetInterface( Interface_ID id ) {
    if( BaseInterface* result = IPRTEmberBase::GetInterface( id ) )
        return result;
    return krakatoa::max3d::PRTObject<PRTEmberBase>::GetInterface( id );
}

void PRTEmberBase::BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev ) {
    krakatoa::max3d::PRTObject<PRTEmberBase>::BeginEditParams( ip, flags, prev );
}

void PRTEmberBase::EndEditParams( IObjParam* ip, ULONG flags, Animatable* next ) {
    krakatoa::max3d::PRTObject<PRTEmberBase>::EndEditParams( ip, flags, next );
}

// From ReferenceMaker
RefResult PRTEmberBase::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& partID,
                                          RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        int param = m_pblock->LastNotifyParamID();

        if( message == REFMSG_CHANGE ) {
            switch( param ) {
            case kTargetNode:
                if( !( partID & PART_GEOM ) )
                    return REF_STOP;

                if( IParamMap2* map = m_pblock->GetMap( kSourceRollout ) )
                    SendMessage( map->GetHWnd(), WM_UPDATE_LIST, 0, 0 );

                break;
            case kRandomSeed:
            case kRandomCount:
                if( !m_pblock->GetInt( kJitterSample ) )
                    return REF_STOP;
                break;
            case kSubdivideCount:
                if( !m_pblock->GetInt( kSubdivideEnabled ) ||
                    ( !m_inRenderMode && m_pblock->GetInt( kViewportDisableSubdivision ) ) )
                    return REF_STOP;
                break;
            case kJitterWellDistributed:
            case kMultiplePerRegion:
                if( !m_pblock->GetInt( kJitterSample ) )
                    return REF_STOP;
                break;
            case kMultipleCount:
                if( !m_pblock->GetInt( kMultiplePerRegion ) || !m_pblock->GetInt( kJitterSample ) ||
                    ( !m_inRenderMode && m_pblock->GetInt( kViewportDisableSubdivision ) ) )
                    return REF_STOP;
                break;
            case kViewportDisableSubdivision:
                if( !m_pblock->GetInt( kSubdivideEnabled ) && !m_pblock->GetInt( kMultiplePerRegion ) &&
                    !m_pblock->GetInt( kJitterSample ) )
                    return REF_STOP;
                break;
            case kIconSize:
                return REF_SUCCEED; // Skip invalidating any cache since its just the icon size changing
            default:
                break;
            }

            InvalidateViewport();
        }
    }

    return REF_SUCCEED;
}

// From BaseObject
#if MAX_VERSION_MAJOR < 24
TYPE_STRING_TYPE PRTEmberBase::GetObjectName() { return PRTEmberBase_CLASS_NAME; }
#else
TYPE_STRING_TYPE PRTEmberBase::GetObjectName( bool localized ) { return PRTEmberBase_CLASS_NAME; }
#endif

// From Object
void PRTEmberBase::InitNodeName( MSTR& s ) { s = PRTEmberBase_CLASS_NAME; }

void PRTEmberBase::GetDeformBBox( TimeValue t, Box3& box, Matrix3* tm, BOOL /*useSel*/ ) {
    INode* pFieldNode = m_pblock->GetINode( kTargetNode );
    if( pFieldNode ) {
        ObjectState os = pFieldNode->EvalWorldState( t );

        if( boost::shared_ptr<ember::max3d::EmberPipeObject> pPipeObj =
                frantic::max3d::safe_convert_to_type<ember::max3d::EmberPipeObject>( os.obj, t,
                                                                                     EmberPipeObject_CLASSID ) ) {
            ViewExp* viewExp =
#if MAX_VERSION_MAJOR >= 15
                &GetCOREInterface()->GetActiveViewExp();
#else
                GetCOREInterface()->GetActiveViewport();
#endif
            pPipeObj->GetLocalBoundBox( t, pFieldNode, viewExp, box );
            if( tm )
                box = box * ( *tm );
            return;
        }
    }

    box.Init();
    box.pmin.x = -10.f;
    box.pmax.x = 10.f;
    box.pmin.y = -10.f;
    box.pmax.y = 10.f;
    box.pmin.z = 0.f;
    box.pmax.z = 20.f;
}

Interval PRTEmberBase::ObjectValidity( TimeValue t ) {
    INode* pFieldNode = m_pblock->GetINode( kTargetNode );
    if( pFieldNode )
        return pFieldNode->EvalWorldState( t ).obj->ObjectValidity( t );
    return FOREVER;
}

void PRTEmberBase::get_available_channels( TimeValue t, std::vector<frantic::tstring>& outChannels ) {
    INode* pFieldNode = m_pblock->GetINode( kTargetNode );
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

ember_field_particle_istream::ember_field_particle_istream( const frantic::channels::channel_map& outMap )
    : m_outMap( outMap )
    , m_compensateDensity( false )
    , m_minDensity( 0.f ) {
    m_defaultParticle.reset( new char[m_outMap.structure_size()] );
    m_outMap.construct_structure( m_defaultParticle.get() );
    m_posAccessor = m_outMap.get_accessor<frantic::graphics::vector3f>( _T("Position") );

    m_nativeMap.define_channel<frantic::graphics::vector3f>( _T("Position") );
    m_nativeMap.end_channel_definition();

    m_outAdaptor.clear();

    m_particleIndex = m_sampleIndex = -1;
    m_maxSamples = 0;
    m_cur[0] = m_min[0] = m_max[0] = 0;
    m_cur[1] = m_min[1] = m_max[1] = 0;
    m_cur[2] = m_min[2] = m_max[2] = 0;
}

ember_field_particle_istream::~ember_field_particle_istream() {}

void ember_field_particle_istream::set_field( const boost::shared_ptr<frantic::volumetrics::field_interface>& pField ) {
    m_pField = pField;

    m_outAdaptor.set( m_outMap, pField->get_channel_map() );

    m_nativeMap.reset();
    m_nativeMap.define_channel<frantic::graphics::vector3f>( _T("Position") );
    m_nativeMap.union_channel_map( pField->get_channel_map() );
    m_nativeMap.end_channel_definition();

    m_densityAccessor.reset();
    if( !m_densityChannel.empty() && m_pField->get_channel_map().has_channel( m_densityChannel ) )
        m_densityAccessor = m_pField->get_channel_map().get_cvt_accessor<float>( m_densityChannel );

    m_cacheStride = m_pField->get_channel_map().structure_size();
    m_cachedData.reset( new char[m_cacheStride * 8] );
}

void ember_field_particle_istream::set_domain( int bounds[], float spacing ) {
    m_min[0] = bounds[0];
    m_max[0] = bounds[1] - 1; // Make m_max inclusive.
    m_min[1] = bounds[2];
    m_max[1] = bounds[3] - 1;
    m_min[2] = bounds[4];
    m_max[2] = bounds[5] - 1;
    m_cur[0] = m_min[0];
    m_cur[1] = m_min[1];
    m_cur[2] = m_min[2];
    m_spacing = spacing;
    m_maxSamples = static_cast<boost::int64_t>( bounds[1] - bounds[0] ) *
                   static_cast<boost::int64_t>( bounds[3] - bounds[2] ) *
                   static_cast<boost::int64_t>( bounds[5] - bounds[4] );

    m_curP[0] = ( static_cast<float>( m_cur[0] ) + 0.5f ) * m_spacing;
    m_curP[1] = ( static_cast<float>( m_cur[1] ) + 0.5f ) * m_spacing;
    m_curP[2] = ( static_cast<float>( m_cur[2] ) + 0.5f ) * m_spacing;

    if( m_pSampler )
        m_pSampler->update_for_voxel( frantic::graphics::vector3( m_cur[0], m_cur[1], m_cur[2] ) );
}

void ember_field_particle_istream::set_sampler(
    const boost::shared_ptr<krakatoa::particle_volume_voxel_sampler>& pSampler ) {
    m_pSampler = pSampler;

    if( m_maxSamples > 0 )
        m_pSampler->update_for_voxel( frantic::graphics::vector3( m_cur[0], m_cur[1], m_cur[2] ) );
}

void ember_field_particle_istream::set_density_field( const frantic::tstring& channelName ) {
    m_densityChannel = channelName;

    m_densityAccessor.reset();
    if( !m_densityChannel.empty() && m_pField->get_channel_map().has_channel( m_densityChannel ) )
        m_densityAccessor = m_pField->get_channel_map().get_cvt_accessor<float>( m_densityChannel );
}

void ember_field_particle_istream::set_density_compensation( bool enabled ) { m_compensateDensity = enabled; }

void ember_field_particle_istream::set_minimum_density( float minDensity ) { m_minDensity = minDensity; }

void ember_field_particle_istream::close() {}

frantic::tstring ember_field_particle_istream::name() const { return _T("ember_field_particle_istream"); }

std::size_t ember_field_particle_istream::particle_size() const { return m_outMap.structure_size(); }

boost::int64_t ember_field_particle_istream::particle_count() const { return -1; }

boost::int64_t ember_field_particle_istream::particle_index() const { return m_particleIndex; }

boost::int64_t ember_field_particle_istream::particle_count_left() const { return -1; }

boost::int64_t ember_field_particle_istream::particle_progress_count() const { return m_maxSamples; }

boost::int64_t ember_field_particle_istream::particle_progress_index() const { return m_sampleIndex; }

boost::int64_t ember_field_particle_istream::particle_count_guess() const { return m_maxSamples; }

void ember_field_particle_istream::set_channel_map( const frantic::channels::channel_map& particleChannelMap ) {
    boost::scoped_array<char> newDefault( new char[particleChannelMap.structure_size()] );

    frantic::channels::channel_map_adaptor adaptor( particleChannelMap, m_outMap );

    particleChannelMap.construct_structure( newDefault.get() );

    adaptor.copy_structure( newDefault.get(), m_defaultParticle.get() );

    m_defaultParticle.swap( newDefault );

    m_outMap = particleChannelMap;
    m_outAdaptor.set( m_outMap, m_pField->get_channel_map() );
    m_posAccessor = m_outMap.get_accessor<frantic::graphics::vector3f>( _T("Position") );
}

const frantic::channels::channel_map& ember_field_particle_istream::get_channel_map() const { return m_outMap; }

const frantic::channels::channel_map& ember_field_particle_istream::get_native_channel_map() const {
    return m_nativeMap;
}

void ember_field_particle_istream::set_default_particle( char* rawParticleBuffer ) {
    boost::scoped_array<char> newDefault( new char[m_outMap.structure_size()] );

    m_outMap.copy_structure( newDefault.get(), rawParticleBuffer );

    m_defaultParticle.swap( newDefault );
}

bool ember_field_particle_istream::advance_iterator() {
    if( ++m_cur[2] >= m_max[2] ) {
        if( ++m_cur[1] >= m_max[1] ) {
            if( ++m_cur[0] >= m_max[0] )
                return false;
            m_cur[1] = m_min[1];
            m_curP[0] = ( static_cast<float>( m_cur[0] ) + 0.5f ) * m_spacing;
        }
        m_cur[2] = m_min[2];
        m_curP[1] = ( static_cast<float>( m_cur[1] ) + 0.5f ) * m_spacing;

        // Need to fill all 8 cache entries.
        /*frantic::graphics::vector3f p( ( static_cast<float>( m_cur[0] ) + 0.5f ) * m_spacing, ( static_cast<float>(
        m_cur[1] ) + 0.5f ) * m_spacing, ( static_cast<float>( m_cur[2] ) + 0.5f ) * m_spacing );

        m_pField->evaluate_field( m_cachedData.get(), p );
        m_pField->evaluate_field( m_cachedData.get() + 1 * m_cacheStride, p + frantic::graphics::vector3f(0.f      , 0.f
        , m_spacing) ); m_pField->evaluate_field( m_cachedData.get() + 2 * m_cacheStride, p +
        frantic::graphics::vector3f(0.f      , m_spacing, 0.f) ); m_pField->evaluate_field( m_cachedData.get() + 3 *
        m_cacheStride, p + frantic::graphics::vector3f(0.f      , m_spacing, m_spacing) ); m_pField->evaluate_field(
        m_cachedData.get() + 4 * m_cacheStride, p + frantic::graphics::vector3f(m_spacing, 0.f      , 0.f) );
        m_pField->evaluate_field( m_cachedData.get() + 5 * m_cacheStride, p + frantic::graphics::vector3f(m_spacing,
        0.f, m_spacing) ); m_pField->evaluate_field( m_cachedData.get() + 6 * m_cacheStride, p +
        frantic::graphics::vector3f(m_spacing, m_spacing, 0.f) ); m_pField->evaluate_field( m_cachedData.get() + 7 *
        m_cacheStride, p + frantic::graphics::vector3f(m_spacing, m_spacing, m_spacing) );*/
    } else {
        // memcpy( m_cachedData.get(), m_cachedData.get() + m_cacheStride * 4, m_cacheStride * 4 );

        // Only need to fill 4 cache entries.
        /*frantic::graphics::vector3f p( ( static_cast<float>( m_cur[0] ) + 0.5f ) * m_spacing, ( static_cast<float>(
        m_cur[1] ) + 0.5f ) * m_spacing, ( static_cast<float>( m_cur[2] ) + 0.5f ) * m_spacing );

        m_pField->evaluate_field( m_cachedData.get() + 4 * m_cacheStride, p + frantic::graphics::vector3f(m_spacing, 0.f
        , 0.f) ); m_pField->evaluate_field( m_cachedData.get() + 5 * m_cacheStride, p +
        frantic::graphics::vector3f(m_spacing, 0.f, m_spacing) ); m_pField->evaluate_field( m_cachedData.get() + 6 *
        m_cacheStride, p + frantic::graphics::vector3f(m_spacing, m_spacing, 0.f) ); m_pField->evaluate_field(
        m_cachedData.get() + 7 * m_cacheStride, p + frantic::graphics::vector3f(m_spacing, m_spacing, m_spacing) );*/
    }

    m_curP[2] = ( static_cast<float>( m_cur[2] ) + 0.5f ) * m_spacing;

    m_pSampler->update_for_voxel( frantic::graphics::vector3( m_cur[0], m_cur[1], m_cur[2] ) );

    return true;
}

bool ember_field_particle_istream::get_particle( char* rawParticleBuffer ) {
    if( m_sampleIndex >= m_maxSamples )
        return false;

    // float weights[8];
    float density;
    float compensationFactor;
    frantic::graphics::vector3f p, samplePos;

    do {
        if( !m_pSampler->get_next_position( samplePos, compensationFactor ) ) {
            do {
                if( !advance_iterator() )
                    return false;

                ++m_sampleIndex;

                if( m_sampleIndex >= m_maxSamples )
                    return false;
            } while( !m_pSampler->get_next_position( samplePos, compensationFactor ) );
        }

        // p.set(
        //	( static_cast<float>( m_cur[0] ) + 0.5f + samplePos.x ) * m_spacing,
        //	( static_cast<float>( m_cur[1] ) + 0.5f + samplePos.y ) * m_spacing,
        //	( static_cast<float>( m_cur[2] ) + 0.5f + samplePos.z ) * m_spacing );
        p.set( m_curP[0] + samplePos.x * m_spacing, m_curP[1] + samplePos.y * m_spacing,
               m_curP[2] + samplePos.z * m_spacing );

        m_pField->evaluate_field( m_cachedData.get(), p );

        density = m_densityAccessor.get( m_cachedData.get() );

        // frantic::volumetrics::levelset::get_trilerp_weights( &samplePos.x, weights );

        // density =
        //	weights[0] * m_densityAccessor.get( m_cachedData.get()                     ) + weights[1] *
        //m_densityAccessor.get( m_cachedData.get() + 4 * m_cacheStride ) + 	weights[2] * m_densityAccessor.get(
        //m_cachedData.get() + 2 * m_cacheStride ) + weights[3] * m_densityAccessor.get( m_cachedData.get() + 6 *
        //m_cacheStride ) + 	weights[4] * m_densityAccessor.get( m_cachedData.get() + 1 * m_cacheStride ) + weights[5] *
        //m_densityAccessor.get( m_cachedData.get() + 5 * m_cacheStride ) + 	weights[6] * m_densityAccessor.get(
        //m_cachedData.get() + 3 * m_cacheStride ) + weights[7] * m_densityAccessor.get( m_cachedData.get() + 7 *
        //m_cacheStride );
    } while( density <= m_minDensity );

    ++m_particleIndex;

    // frantic::graphics::vector3f p(
    //	( static_cast<float>( m_cur[0] ) + 0.5f + samplePos.x ) * m_spacing,
    //	( static_cast<float>( m_cur[1] ) + 0.5f + samplePos.y ) * m_spacing,
    //	( static_cast<float>( m_cur[2] ) + 0.5f + samplePos.z ) * m_spacing );

    m_outAdaptor.copy_structure( rawParticleBuffer, m_cachedData.get(), m_defaultParticle.get() );

    m_posAccessor.get( rawParticleBuffer ) = p;

    return true;
}

bool ember_field_particle_istream::get_particles( char* rawParticleBuffer, std::size_t& numParticles ) {
    for( std::size_t i = 0, stride = m_outMap.structure_size(); i < numParticles; ++i, rawParticleBuffer += stride ) {
        if( !this->get_particle( rawParticleBuffer ) ) {
            numParticles = i;
            return false;
        }
    }

    return true;
}

} // namespace max3d
} // namespace ember

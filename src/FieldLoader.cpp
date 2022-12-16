// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource_ember.h"

#include <AssetManagement/AssetType.h>
#include <AssetManagement/IAssetAccessor.h>
#include <AssetManagement/IAssetManager.h>
#include <IPathConfigMgr.h>
#include <ember/data_types.hpp>
#include <ember/field3d.hpp>
#include <ember/openvdb.hpp>
#include <stoke/max3d/EmberObjectBase.hpp>

#include <frantic/max3d/GenericReferenceTarget.hpp>
#include <frantic/max3d/convert.hpp>
#include <frantic/max3d/create_mouse_callback.hpp>
#include <frantic/max3d/exception.hpp>
#include <frantic/max3d/fnpublish/MixinInterface.hpp>
#include <frantic/max3d/fnpublish/StandaloneInterface.hpp>
#include <frantic/max3d/max_utility.hpp>
#include <frantic/max3d/maxscript/shared_value_ptr.hpp>
#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>
#include <frantic/max3d/time.hpp>
#include <frantic/max3d/volumetrics/fumefx_field_factory.hpp>

#include <frantic/files/filename_sequence.hpp>
#include <frantic/files/sequence_cache.hpp>
#include <frantic/logging/logging_level.hpp>
#include <frantic/volumetrics/field_interface.hpp>

#pragma warning( push, 3 )
#pragma warning( disable : 4800 4244 4146 4267 4355 4503 )
#include <openvdb\io\File.h>
#pragma warning( pop )

#pragma warning( disable : 4503 )

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/tuple/tuple.hpp>

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#define EmberVolumeCache_INTERFACE Interface_ID( 0xa7c630e, 0x56984d6a )

#define EmberLoader_CLASSID Class_ID( 0x3e405d65, 0x4dff7e00 )
#define EmberLoader_NAME "StokeFieldLoaderBase"
#define EmberLoader_DISPLAYNAME "Stoke Field Loader Base"
#define EmberLoader_VERSION 2

extern HINSTANCE hInstance;

extern void BuildMesh_Text_EMBER_Loader( Mesh& outMesh );

namespace ember {
namespace max3d {

class EmberLoaderDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_paramDesc;

    friend class EmberLoader;

  public:
    EmberLoaderDesc();

    int IsPublic() { return FALSE; }
    void* Create( BOOL loading );
    const TCHAR* ClassName() { return _T( EmberLoader_DISPLAYNAME ); }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return EmberLoader_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() {
        return _T( EmberLoader_NAME );
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( EmberLoader_DISPLAYNAME ); }
#endif
};

enum { kMainPBlock };

enum {
    kFilenamePattern_old,
    kCacheCapacityMB,
    kUsePlaybackTime,
    kPlaybackTime,
    kViewportDisplayMode,
    kViewportMaskChannel,
    kViewportVectorChannel,
    kViewportColorChannel,
    kViewportVectorScale,
    kViewportVectorNormalize,
    kViewportReduce,
    kSubstituteSequenceNumber,
    kPlaybackOffset,
    kUsePlaybackRange,
    kPlaybackRangeStart,
    kPlaybackRangeEnd,
    kPlaybackRangeStartORT,
    kPlaybackRangeEndORT,
    kIconSize,
    kViewportEnabled,
    kViewportBoundsEnabled,
    kInWorldSpace,
    kFilenamePattern
    // kUsePlaybackInterpolation, // We might need to have multiple strategies for mapping an inbetween-frame time to
    // data.
};

namespace ort_type {
enum option {
    hold,
    blank,
    COUNT // Must be last entry. Not a valid option.
};
}

class FilenamePatternAccessor : public PBAccessor {
  public:
    virtual void Set( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t );
};

EmberLoaderDesc::EmberLoaderDesc()
    : m_paramDesc( kMainPBlock, _T("Parameters"), 0, EmberLoader_VERSION ) {
    m_paramDesc.OwnerRefNum( 0 );
    m_paramDesc.OwnerClassDesc( this );

    static FilenamePatternAccessor theFilenamePatternAccessor;

    m_paramDesc.Parameter<float>( kIconSize, _T("IconSize"), 0 ).DefaultValue( 30.f ).Range( 0.f, FLT_MAX );

    m_paramDesc.Parameter<const MCHAR*>( kFilenamePattern_old, _T( "" ), 0, false, P_OBSOLETE );

    m_paramDesc.Parameter<boost::filesystem::path>( kFilenamePattern, _T( "FilenamePattern" ), 0 )
        .AssetTypeID( MaxSDK::AssetManagement::kAnimationAsset )
        .DefaultValue( _T( "" ) )
        .Accessor( &theFilenamePatternAccessor );

    m_paramDesc.Parameter<int>( kCacheCapacityMB, _T("CacheCapacityMB"), 0 )
        .DefaultValue( 512 )
        .Range( 0, INT_MAX )
        .Accessor( &theFilenamePatternAccessor );

    m_paramDesc.Parameter<bool>( kUsePlaybackTime, _T("UsePlaybackTime"), 0 ).DefaultValue( false );

    m_paramDesc.Parameter<float>( kPlaybackTime, _T("PlaybackTime"), IDS_PLAYBACKTIME, true )
        .DefaultValue( 0.f )
        .Range( -FLT_MAX, FLT_MAX );

    m_paramDesc.Parameter<bool>( kViewportEnabled, _T("ViewportEnabled"), 0 ).DefaultValue( true );

    m_paramDesc.Parameter<int>( kViewportReduce, _T("ViewportReduce"), 0 ).DefaultValue( 0 ).Range( 0, INT_MAX );

    m_paramDesc.Parameter<TYPE_STRING_TYPE>( kViewportMaskChannel, _T("ViewportMaskChannel"), 0 )
        .DefaultValue( _T("") );

    m_paramDesc.Parameter<TYPE_STRING_TYPE>( kViewportVectorChannel, _T("ViewportVectorChannel"), 0 )
        .DefaultValue( _T("") );

    m_paramDesc.Parameter<TYPE_STRING_TYPE>( kViewportColorChannel, _T("ViewportColorChannel"), 0 )
        .DefaultValue( _T("") );

    m_paramDesc.Parameter<float>( kViewportVectorScale, _T("ViewportVectorScale"), IDS_VIEWPORTVECTORSCALE, true )
        .DefaultValue( 1.f )
        .Range( 0.f, FLT_MAX );

    m_paramDesc.Parameter<bool>( kViewportVectorNormalize, _T("ViewportVectorNormalize"), 0 ).DefaultValue( false );

    m_paramDesc.Parameter<bool>( kViewportBoundsEnabled, _T("ViewportBoundsEnabled"), 0 ).DefaultValue( false );

    m_paramDesc.Parameter<bool>( kSubstituteSequenceNumber, _T("SubstituteSequenceNumber"), 0 ).DefaultValue( true );

    m_paramDesc.Parameter<float>( kPlaybackOffset, _T("PlaybackOffset"), 0 )
        .DefaultValue( 0.f )
        .Range( -FLT_MAX, FLT_MAX );

    m_paramDesc.Parameter<bool>( kUsePlaybackRange, _T("UsePlaybackRange"), 0 ).DefaultValue( false );

    m_paramDesc.Parameter<int>( kPlaybackRangeStart, _T("PlaybackRangeStart"), 0 )
        .DefaultValue( 0 )
        .Range( INT_MIN, INT_MAX );

    m_paramDesc.Parameter<int>( kPlaybackRangeEnd, _T("PlaybackRangeEnd"), 0 )
        .DefaultValue( 0 )
        .Range( INT_MIN, INT_MAX );

    m_paramDesc.Parameter<int>( kPlaybackRangeStartORT, _T("PlaybackRangeStartORT"), 0 )
        .DefaultValue( ort_type::blank )
        .Range( 0, ort_type::COUNT );

    m_paramDesc.Parameter<int>( kPlaybackRangeEndORT, _T("PlaybackRangeEndORT"), 0 )
        .DefaultValue( ort_type::hold )
        .Range( 0, ort_type::COUNT );

    m_paramDesc.Parameter<bool>( kInWorldSpace, _T("InWorldSpace"), 0 ).DefaultValue( false );
}

ClassDesc2* GetEmberLoaderDesc() {
    static EmberLoaderDesc theEmberLoaderDesc;
    return &theEmberLoaderDesc;
}

class EmberLoader : public EmberObjectBase {
  public:
    EmberLoader( BOOL loading );

    virtual ~EmberLoader();

    // From Animatable
    virtual IOResult Load( ILoad* iload );

    // From ReferenceMaker
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

// From BaseObject
#if MAX_VERSION_MAJOR < 24
    virtual TYPE_STRING_TYPE GetObjectName();
#else
    virtual TYPE_STRING_TYPE GetObjectName( bool localized ) const;
#endif

    virtual CreateMouseCallBack* GetCreateMouseCallBack();

    // From Object
    virtual void InitNodeName( MSTR& s );

  protected:
    virtual void EvalField( EmberPipeObject& objToPopulate, TimeValue t );

    virtual void SetIconScale( float scale );

    virtual ClassDesc2* GetClassDesc();

  protected:
    float get_icon_size() const { return frantic::max3d::get<float>( m_pblock, kIconSize ); }
    const MCHAR* get_filename_pattern() const {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kFilenamePattern );
    }
    int get_cache_capacity() const { return ( 1 << 20 ) * frantic::max3d::get<int>( m_pblock, kCacheCapacityMB ); }
    bool get_use_playback_time() const { return frantic::max3d::get<bool>( m_pblock, kUsePlaybackTime ); }
    float get_playback_time( TimeValue t ) const { return frantic::max3d::get<float>( m_pblock, kPlaybackTime, t ); }
    int get_viewport_reduce() const { return std::max( 0, frantic::max3d::get<int>( m_pblock, kViewportReduce ) ); }
    bool get_substitute_sequence_number() const {
        return frantic::max3d::get<bool>( m_pblock, kSubstituteSequenceNumber );
    }
    float get_playback_offset() const { return frantic::max3d::get<float>( m_pblock, kPlaybackOffset ); }
    bool get_use_playback_range() const { return frantic::max3d::get<bool>( m_pblock, kUsePlaybackRange ); }
    int get_playback_range_start() const { return frantic::max3d::get<int>( m_pblock, kPlaybackRangeStart ); }
    int get_playback_range_end() const { return frantic::max3d::get<int>( m_pblock, kPlaybackRangeEnd ); }
    ort_type::option get_playback_range_start_ORT() const {
        return static_cast<ort_type::option>( std::max(
            0, std::min<int>( ort_type::COUNT - 1, frantic::max3d::get<int>( m_pblock, kPlaybackRangeStartORT ) ) ) );
    }
    ort_type::option get_playback_range_end_ORT() const {
        return static_cast<ort_type::option>( std::max(
            0, std::min<int>( ort_type::COUNT - 1, frantic::max3d::get<int>( m_pblock, kPlaybackRangeEndORT ) ) ) );
    }
    bool get_in_world_space() const { return frantic::max3d::get<bool>( m_pblock, kInWorldSpace ); }

    bool get_viewport_enabled() const { return frantic::max3d::get<bool>( m_pblock, kViewportEnabled ); }
    const MCHAR* get_viewport_mask_channel() const {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kViewportMaskChannel );
    }
    const MCHAR* get_viewport_vector_channel() const {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kViewportVectorChannel );
    }
    const MCHAR* get_viewport_color_channel() const {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kViewportColorChannel );
    }
    float get_viewport_vector_scale( TimeValue t ) const {
        return frantic::max3d::get<float>( m_pblock, kViewportVectorScale, t );
    }
    bool get_viewport_vector_normalize() const {
        return frantic::max3d::get<bool>( m_pblock, kViewportVectorNormalize );
    }
    bool get_viewport_bounds_enabled() const { return frantic::max3d::get<bool>( m_pblock, kViewportBoundsEnabled ); }

  private:
    friend class FilenamePatternAccessor;
    friend class EmberLoaderAssetAccessor;
    class EmberLoaderPLC;

  private:
    struct deserialize_data {
        // boost::shared_future< boost::shared_ptr<frantic::volumetrics::field_interface> > futureField;
        future_field_base::ptr_type futureField;
        frantic::channels::channel_map fieldMap;
        Box3 bounds;
        float spacing;

        std::size_t memoryUsage;
    };

    boost::shared_ptr<deserialize_data> deserialize( const frantic::tstring& path ) const;

    void on_filename_pattern_changed( const MCHAR* szPath );
    void on_cache_capacity_changed( std::size_t cacheSize );

    void EnumAuxFiles( AssetEnumCallback& nameEnum, DWORD flags );

  private:
    struct serializer {
        EmberLoader* m_owner;

        serializer( EmberLoader* owner )
            : m_owner( owner ) {}

        void serialize( const frantic::tstring& /*path*/, const boost::shared_ptr<deserialize_data>& /*val*/ ) const {
            assert( false ); // This should never be called.
        }

        boost::shared_ptr<deserialize_data> deserialize( const frantic::tstring& path ) const {
            return m_owner->deserialize( path );
        }
    };

    struct size_estimator {
        std::size_t operator()( const boost::shared_ptr<deserialize_data>& val ) const { return val->memoryUsage; }
    };

    typedef frantic::files::sequence_cache<boost::shared_ptr<deserialize_data>, serializer, size_estimator> cache_type;

    cache_type m_dataCache;

    struct file_type {
        enum option {
            kUnknown,
            kFumeFX,
            kOpenVDB,
            kField3D,
        };
    };

    // If in single-frame mode (ie. not loading a sequence) then we will store our cached result here. We use a boolean
    // flag to indicate validity since there may not be a valid field for the specified path (ie. m_singleFrameData is
    // NULL).
    bool m_singleFrameValid;
    boost::shared_ptr<deserialize_data> m_singleFrameData;

    file_type::option m_fileType;
};

void* EmberLoaderDesc::Create( BOOL loading ) { return new EmberLoader( loading ); }

void FilenamePatternAccessor::Set( PB2Value& v, ReferenceMaker* owner, ParamID id, int /*tabIndex*/, TimeValue /*t*/ ) {
    if( !owner )
        return;

    if( id == kFilenamePattern )
        static_cast<EmberLoader*>( owner )->on_filename_pattern_changed(
            static_cast<EmberLoader*>( owner )->m_pblock->GetStr( kFilenamePattern ) );
    else if( id == kCacheCapacityMB )
        static_cast<EmberLoader*>( owner )->on_cache_capacity_changed( static_cast<std::size_t>( 1 << 20 ) *
                                                                       static_cast<std::size_t>( std::max( 0, v.i ) ) );
}

#pragma warning( push )
#pragma warning(                                                                                                       \
    disable : 4355 ) // Disable "'this' : used in base member initializer list" warning since its safe here.
EmberLoader::EmberLoader( BOOL /*loading*/ )
    : m_dataCache( 512 * ( 1 << 20 ), serializer( this ), size_estimator() )
    , m_fileType( file_type::kUnknown )
    , m_singleFrameValid( false ) {
    GetEmberLoaderDesc()->MakeAutoParamBlocks( this );
}
#pragma warning( pop )

EmberLoader::~EmberLoader() {}

// From Animatable
class EmberLoader::EmberLoaderPLC : public PostLoadCallback {
    EmberLoader* m_pOwner;

  public:
    explicit EmberLoaderPLC( EmberLoader* pOwner )
        : m_pOwner( pOwner ) {}

    virtual void proc( ILoad* /*iload*/ ) {
        if( m_pOwner && m_pOwner->m_pblock ) {
            m_pOwner->on_cache_capacity_changed(
                static_cast<std::size_t>( 1 << 20 ) *
                static_cast<std::size_t>( std::max( 0, m_pOwner->m_pblock->GetInt( kCacheCapacityMB ) ) ) );
            m_pOwner->on_filename_pattern_changed( m_pOwner->m_pblock->GetStr( kFilenamePattern ) );
        }

        delete this;
    }

    void PostLoadCallback( ILoad* iload ) {
        IParamBlock2PostLoadInfo* postLoadInfo =
            (IParamBlock2PostLoadInfo*)m_pOwner->m_pblock->GetInterface( IPARAMBLOCK2POSTLOADINFO_ID );
        if( postLoadInfo != NULL && postLoadInfo->GetVersion() == 1 ) {
            const MCHAR* filenamePattern = m_pOwner->m_pblock->GetStr( kFilenamePattern_old );
            m_pOwner->m_pblock->SetValue( kFilenamePattern, 0, filenamePattern );
            Control* filenameControl = m_pOwner->m_pblock->GetControllerByIndex( kFilenamePattern_old );
            if( filenameControl )
                m_pOwner->m_pblock->SetControllerByIndex( m_pOwner->m_pblock->IDtoIndex( kFilenamePattern ), 0,
                                                          filenameControl );
        }
    }
};

IOResult EmberLoader::Load( ILoad* iload ) {
    IOResult result = GeomObject::Load( iload );

    if( result == IO_OK )
        iload->RegisterPostLoadCallback( new EmberLoaderPLC( this ) );

    return result;
}

// From ReferenceMaker
RefResult EmberLoader::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& /*partID*/,
                                         RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock && message == REFMSG_CHANGE ) {
        this->InvalidateCache();

        int index = -1;
        ParamID id = m_pblock->LastNotifyParamID( index );

        if( id == kSubstituteSequenceNumber ) {
            // If we are disabling the sequence, clear it now.
            if( !this->get_substitute_sequence_number() ) {
                m_dataCache.clear();
            } else {
                // Update the cache to re-enable it.
                this->on_filename_pattern_changed( this->get_filename_pattern() );
            }
        }
    }
    return REF_SUCCEED;
}

// From BaseObject
#if MAX_VERSION_MAJOR < 24
TYPE_STRING_TYPE EmberLoader::GetObjectName() { return _T( EmberLoader_DISPLAYNAME ); }
#else
TYPE_STRING_TYPE EmberLoader::GetObjectName( bool localized ) const { return _T( EmberLoader_DISPLAYNAME ); }
#endif

CreateMouseCallBack* EmberLoader::GetCreateMouseCallBack() {
    static frantic::max3d::ClickAndDragCreateCallBack theCallback;
    return &theCallback;
}

// From Object
void EmberLoader::InitNodeName( MSTR& s ) { s = _T( EmberLoader_DISPLAYNAME ); }

ClassDesc2* EmberLoader::GetClassDesc() { return GetEmberLoaderDesc(); }

void EmberLoader::EvalField( EmberPipeObject& objToPopulate, TimeValue t ) {
    Interval valid = FOREVER;

    float iconScale = this->get_icon_size();
    if( iconScale > 0.f ) {
        // NOTE: A static Mesh object caused intermittent access violations in Max 2015 (and 2014?)
#if MAX_VERSION_MAJOR >= 25
        static MaxSDK::SharedMeshPtr theIcon;
#else
        static Mesh* theIcon = NULL;
#endif
        static MaxSDK::Graphics::RenderItemHandleArray theIconMeshes;

        if( !theIcon ) {
#if MAX_VERSION_MAJOR >= 25
            theIcon = new MaxSDK::SharedMesh();
            BuildMesh_Text_EMBER_Loader( theIcon->GetMesh() );
#else
            theIcon = CreateNewMesh();
            BuildMesh_Text_EMBER_Loader( *theIcon );
#endif
        }

        Matrix3 iconTM;
        iconTM.SetScale( Point3( iconScale, iconScale, iconScale ) );

        objToPopulate.SetViewportIcon( theIconMeshes, theIcon, iconTM );
    }

    BOOL usePlayback = FALSE;
    m_pblock->GetValue( kUsePlaybackTime, t, usePlayback, valid );

    boost::shared_ptr<deserialize_data> data;

    try {
        if( this->get_substitute_sequence_number() ) {
            if( !m_dataCache.empty() ) {
                bool usePlaybackTime = this->get_use_playback_time();

                double frame = frantic::max3d::to_frames<double>( t );
                double frameOffset = static_cast<double>( this->get_playback_offset() );

                if( usePlaybackTime ) {
                    float floatFrame;
                    m_pblock->GetValue( kPlaybackTime, t, floatFrame, valid );

                    frame = static_cast<double>( floatFrame );
                } else {
                    valid.SetInstant( t ); // We can make this more accurate (maybe by using the bracketing frames?).
                }

                frame += frameOffset;

                double rangeStart, rangeEnd;

                boost::tie( rangeStart, rangeEnd ) = m_dataCache.get_key_range();

                if( this->get_use_playback_range() ) {
                    double customStart = static_cast<double>( this->get_playback_range_start() );
                    double customEnd = static_cast<double>( this->get_playback_range_end() );

                    if( customStart > rangeStart )
                        rangeStart = customStart;
                    if( customEnd < rangeEnd )
                        rangeEnd = customEnd;
                    if( rangeEnd < rangeStart )
                        rangeEnd = rangeStart;
                }

                if( frame < rangeStart ) {
                    ort_type::option startOrt = this->get_playback_range_start_ORT();
                    if( startOrt == ort_type::hold )
                        data = m_dataCache.find( rangeStart );

                    // If we aren't using the playback graph, we can optimize the validity interval. If we are using the
                    // graph, we can't make any assumptions.
                    if( !usePlaybackTime )
                        valid.Set( TIME_NegInfinity, static_cast<TimeValue>( static_cast<double>( GetTicksPerFrame() ) *
                                                                             ( rangeStart - frameOffset ) ) -
                                                         1 );
                } else if( frame > rangeEnd ) {
                    ort_type::option endOrt = this->get_playback_range_end_ORT();
                    if( endOrt == ort_type::hold )
                        data = m_dataCache.find( rangeEnd );

                    // If we aren't using the playback graph, we can optimize the validity interval.
                    if( !usePlaybackTime )
                        valid.Set( static_cast<TimeValue>( static_cast<double>( GetTicksPerFrame() ) *
                                                           ( rangeEnd - frameOffset ) ) +
                                       1,
                                   TIME_PosInfinity );
                } else {
                    data = m_dataCache.find( m_dataCache.find_nearest_key( frame ) );
                }
            }
        } else { // !get_substitute_sequence_number()
            boost::filesystem::path filePath( this->get_filename_pattern() );

            if( !m_singleFrameValid ) {
                m_singleFrameData.reset();
                m_singleFrameValid = true;

                if( boost::filesystem::exists( filePath ) )
                    m_singleFrameData = this->deserialize( filePath.string<frantic::tstring>() );
            }

            data = m_singleFrameData;

            valid.SetInfinite();
        }
    } catch( const std::exception& e ) {
        FF_LOG( error ) << frantic::strings::to_tstring( e.what() ) << std::endl;

        // TODO: Update the error status of this object.

        valid.SetInstant( t ); // TODO: On an error, should we be valid until something changes, or do we want a
                               // time-change to try the evaluation again?
    }

    if( !data ) {
        // TODO: Use appropriate channels from any sequence entry (if one exists).
        objToPopulate.SetEmpty();
    } else {
        int vpReduce = this->get_viewport_reduce();
        bool inWorldSpace = this->get_in_world_space();

        objToPopulate.Set( data->futureField, data->fieldMap, data->bounds, data->spacing, inWorldSpace );
        objToPopulate.SetViewportSpacingNatural( vpReduce );

        if( this->get_viewport_enabled() ) {
            const MCHAR* szMaskChannel = this->get_viewport_mask_channel();
            if( !szMaskChannel )
                szMaskChannel = _T("");

            const MCHAR* szVelocityChannel = this->get_viewport_vector_channel();
            if( !szVelocityChannel )
                szVelocityChannel = _T("");

            const MCHAR* szColorChannel = this->get_viewport_color_channel();
            if( !szColorChannel )
                szColorChannel = _T("");

            objToPopulate.SetViewportScalarChannel( szMaskChannel );
            objToPopulate.SetViewportVectorChannel( szVelocityChannel );
            objToPopulate.SetViewportColorChannel( szColorChannel );

            if( !szVelocityChannel || szVelocityChannel[0] == _T( '\0' ) ) {
                objToPopulate.SetViewportDisplayDots( 0.f, std::numeric_limits<float>::infinity() );
            } else {
                bool normalize = this->get_viewport_vector_normalize();
                float scale = this->get_viewport_vector_scale( t );

                objToPopulate.SetViewportDisplayLines( normalize, scale );
            }
        } else {
            objToPopulate.SetViewportDisplayNone();
        }

        objToPopulate.SetViewportDisplayBounds( this->get_viewport_bounds_enabled() );
    }

    objToPopulate.SetChannelValidity( GEOM_CHAN_NUM, valid );
    objToPopulate.SetChannelValidity( DISP_ATTRIB_CHAN_NUM, FOREVER );
}

void EmberLoader::SetIconScale( float scale ) { frantic::max3d::set<float>( m_pblock, kIconSize, 0, scale ); }

boost::shared_ptr<EmberLoader::deserialize_data> EmberLoader::deserialize( const frantic::tstring& path ) const {
    boost::shared_ptr<deserialize_data> result( new deserialize_data );

    boost::filesystem::path deserializePath( path );

    if( deserializePath.extension() == _T(".vdb") ) {
        openvdb_meta vdbMeta;

        result->futureField =
            ember::create_field_interface_from_file( deserializePath.string<frantic::tstring>(), vdbMeta );

        result->spacing = vdbMeta.spacing;
        result->bounds = frantic::max3d::to_max_t( vdbMeta.bounds );
        result->fieldMap.swap( vdbMeta.map );
        result->memoryUsage = vdbMeta.memoryUsage;
#if defined( FIELD3D_AVAILABLE )
    } else if( deserializePath.extension() == _T(".f3d") ) {
        field3d_meta meta;

        // TODO: This should support asynchronous loading!
        boost::shared_ptr<frantic::volumetrics::field_interface> pField =
            load_fields_from_file( deserializePath.string<frantic::tstring>(), meta );

        result->futureField = ember::create_field_task_from_future( boost::make_shared_future( pField ) );

        result->spacing = meta.spacing;
        result->bounds = frantic::max3d::to_max_t( meta.bounds );
        result->fieldMap.swap( meta.map );
        result->memoryUsage = meta.memoryUsage;
#endif
#if defined( FUMEFX_SDK_AVAILABLE )
    } else if( deserializePath.extension() == _T(".fxd") ) {
        frantic::max3d::volumetrics::fumefx_fxd_metadata fxdMeta;

        result->futureField =
            create_field_task_from_future( frantic::max3d::volumetrics::get_fumefx_field_async( path, 0, fxdMeta ) );

        result->spacing = fxdMeta.dx;
        result->bounds.pmin.Set( fxdMeta.dataBounds[0], fxdMeta.dataBounds[1], fxdMeta.dataBounds[2] );
        result->bounds.pmax.Set( fxdMeta.dataBounds[3], fxdMeta.dataBounds[4], fxdMeta.dataBounds[5] );
        result->fieldMap.swap( fxdMeta.fileChannels );
        result->memoryUsage = fxdMeta.memUsage;
#endif
    } else {
        assert( false );
    }

    return result;
}

void EmberLoader::on_filename_pattern_changed( const MCHAR* szPath ) {
    if( szPath ) {
        boost::filesystem::path thePatternPath( szPath );
        if( thePatternPath.extension() == _T(".vdb") ) {
            m_fileType = file_type::kOpenVDB;
        } else if( thePatternPath.extension() == _T(".f3d") ) {
            m_fileType = file_type::kField3D;
        } else if( thePatternPath.extension() == _T(".fxd") ) {
            m_fileType = file_type::kFumeFX;
        } else {
            m_fileType = file_type::kUnknown;
        }
    } else {
        m_fileType = file_type::kUnknown;
    }

    // If we had cached a single frame, it is no longer valid.
    m_singleFrameValid = false;
    m_singleFrameData.reset();
    if( this->get_substitute_sequence_number() ) {
        if( szPath ) {
            m_dataCache.set_disk_path( szPath, cache_type::synchronize );
        } else {
            m_dataCache.set_disk_path( _T(""), cache_type::synchronize );
        }
    }
}

void EmberLoader::on_cache_capacity_changed( std::size_t cacheSize ) { m_dataCache.set_capacity( cacheSize ); }

class EmberLoaderAssetAccessor : public IAssetAccessor {
  protected:
    EmberLoader* m_EmberLoader;
    ParamID m_parameterId;

  public:
    EmberLoaderAssetAccessor( EmberLoader* emberLoader, ParamID parameterId )
        : m_EmberLoader( emberLoader )
        , m_parameterId( parameterId ) {}

    MaxSDK::AssetManagement::AssetUser GetAsset() const {
        return m_EmberLoader->m_pblock->GetAssetUser( m_parameterId );
    }

    bool SetAsset( const MaxSDK::AssetManagement::AssetUser& aNewAssetUser ) {
        m_EmberLoader->m_pblock->SetValue( m_parameterId, 0, aNewAssetUser );
        return true;
    }

    MaxSDK::AssetManagement::AssetType GetAssetType() const { return MaxSDK::AssetManagement::kAnimationAsset; }
};

void EmberLoader::EnumAuxFiles( AssetEnumCallback& nameEnum, DWORD flags ) {
    if( ( flags & FILE_ENUM_CHECK_AWORK1 ) && TestAFlag( A_WORK1 ) )
        return;

    if( flags & FILE_ENUM_ACCESSOR_INTERFACE ) {
        EmberLoaderAssetAccessor renderAccessor( this, kFilenamePattern );

        if( renderAccessor.GetAsset().GetId() != MaxSDK::AssetManagement::kInvalidId ) {
            const frantic::tstring path = renderAccessor.GetAsset().GetFullFilePath().data();

            if( frantic::files::file_exists( path ) ) {
                IEnumAuxAssetsCallback* callback = static_cast<IEnumAuxAssetsCallback*>( &nameEnum );
                callback->DeclareAsset( renderAccessor );
            }
        }
    } else {
        if( m_pblock->GetAssetUser( kFilenamePattern ).GetId() != MaxSDK::AssetManagement::kInvalidId ) {
            const frantic::tstring path = m_pblock->GetAssetUser( kFilenamePattern ).GetFullFilePath().data();
            if( frantic::files::file_exists( path ) ) {
                IPathConfigMgr::GetPathConfigMgr()->RecordInputAsset( m_pblock->GetAssetUser( kFilenamePattern ),
                                                                      nameEnum, flags );
            }
        }
        GeomObject::EnumAuxFiles( nameEnum, flags );
    }
}
} // namespace max3d
} // namespace ember

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource.h"

#include <stoke/max3d/FieldSim/SimEmberBase.hpp>
#include <stoke/max3d/FieldSim/SimEmberBaseParamBlock.hpp>

#include <frantic/files/filename_sequence.hpp>
#include <frantic/logging/logging_level.hpp>

#include <iparamb2.h>
#include <plugapi.h>

#include <AssetManagement\iassetmanager.h>
#include <IPathConfigMgr.h>

namespace ember {
namespace max3d {

class SimEmberBaseSequencePathValidator : public PBValidator {
    SimEmberBaseSequencePathValidator();

  public:
    static SimEmberBaseSequencePathValidator& GetInstance();

    virtual BOOL Validate( PB2Value& v );
};

SimEmberBaseSequencePathValidator::SimEmberBaseSequencePathValidator() {}

SimEmberBaseSequencePathValidator& SimEmberBaseSequencePathValidator::GetInstance() {
    static SimEmberBaseSequencePathValidator theSimEmberBaseSequencePathValidator;
    return theSimEmberBaseSequencePathValidator;
}

BOOL SimEmberBaseSequencePathValidator::Validate( PB2Value& v ) {
    try {
        // Try to construct a filename_pattern in order to see if the provided path is invalid.
        frantic::files::filename_pattern tempPattern( v.s );

        // If we didn't get an exception, it's all good.
        return TRUE;
    } catch( const std::exception& e ) {
#ifdef UNICODE
        FF_LOG( error ) << MSTR::FromACP( e.what() ).data() << std::endl;
#else
        FF_LOG( error ) << e.what() << std::endl;
#endif
    }

    return FALSE;
}

class SimEmberBaseSequencePathAccessor : public PBAccessor {
    SimEmberBaseSequencePathAccessor();

  public:
    static SimEmberBaseSequencePathAccessor& GetInstance();

    virtual void Get( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t, Interval& valid );

    virtual void Set( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t );
};

SimEmberBaseSequencePathAccessor::SimEmberBaseSequencePathAccessor() {}

SimEmberBaseSequencePathAccessor& SimEmberBaseSequencePathAccessor::GetInstance() {
    static SimEmberBaseSequencePathAccessor theSimEmberBaseSequencePathAccessor;
    return theSimEmberBaseSequencePathAccessor;
}

void SimEmberBaseSequencePathAccessor::Get( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t,
                                            Interval& valid ) {
    if( !owner )
        return;

    if( id == kSequenceCacheUsage ) {
        std::size_t cacheSize = static_cast<SimEmberBase*>( owner )->m_dataCache.get_usage();

        cacheSize += ( ( 1ui64 << 20 ) - 1 ); // Add 1 less than a MB so we round up.
        cacheSize /= ( 1ui64 << 20 );         // Convert to MB.

        v.i = static_cast<int>( cacheSize );
    } else if( id == kSerializeQueueUsage ) {
        std::size_t cacheSize = static_cast<SimEmberBase*>( owner )->m_dataCache.get_pending_usage();

        cacheSize += ( ( 1ui64 << 20 ) - 1 ); // Add 1 less than a MB so we round up.
        cacheSize /= ( 1ui64 << 20 );         // Convert to MB.

        v.i = static_cast<int>( cacheSize );
    } else {
        return PBAccessor::Get( v, owner, id, tabIndex, t, valid );
    }
}

void SimEmberBaseSequencePathAccessor::Set( PB2Value& v, ReferenceMaker* owner_, ParamID id, int /*tabIndex*/,
                                            TimeValue /*t*/ ) {
    if( !owner_ )
        return;

    SimEmberBase* pOwner = static_cast<SimEmberBase*>( owner_ );

    if( id == kSequencePath ) {
        try {
            if( pOwner->m_pblock->GetInt( kUseDiskCache ) != FALSE )
                pOwner->m_dataCache.set_disk_path( v.s, SimEmberBase::cache_type::keep_existing );
        } catch( const std::exception& e ) {
            FF_LOG( error ) << e.what() << std::endl;
        }
    } else if( id == kSequenceCacheCapacity ) {
        try {
            v.i = std::max( 0, v.i );
            pOwner->m_dataCache.set_capacity( static_cast<std::size_t>( v.i ) * static_cast<std::size_t>( 1 << 20 ) );
        } catch( const std::exception& e ) {
            FF_LOG( error ) << e.what() << std::endl;
        }
    } else if( id == kUseDiskCache ) {
        try {
            if( v.i == FALSE )
                pOwner->m_dataCache.set_disk_path( _T(""), SimEmberBase::cache_type::keep_existing );
            else
                pOwner->m_dataCache.set_disk_path( pOwner->m_pblock->GetStr( kSequencePath ),
                                                   SimEmberBase::cache_type::keep_existing );
        } catch( const std::exception& e ) {
            FF_LOG( error ) << e.what() << std::endl;
        }
    } else if( id == kSerializeQueueCapacity ) {
        try {
            v.i = std::max( 0, v.i );
            pOwner->m_dataCache.set_pending_capacity( static_cast<std::size_t>( v.i ) *
                                                      static_cast<std::size_t>( 1 << 20 ) );
        } catch( const std::exception& e ) {
            FF_LOG( error ) << e.what() << std::endl;
        }
    }
}

void InitParamBlockDesc( frantic::max3d::ParamBlockBuilder& pbBuilder ) {
    pbBuilder.Parameter<float>( kIconSize, _T("IconSize") )
        .EnableAnimation( true, IDS_ICONSIZE )
        .ExtraFlags( P_RESET_DEFAULT )
        .DefaultValue( 30.f )
        .Range( 0.f, FLT_MAX );

    pbBuilder.Parameter<bool>( kViewportEnabled, _T("ViewportEnabled"), 0 ).DefaultValue( true );

    // pbBuilder.Parameter<float>( kViewportPercentage, _T("ViewportPercentage"), 0, false, P_RESET_DEFAULT )
    //	.DefaultValue( 0.1f )
    //	.Range( 0.f, 1.f );

    // pbBuilder.Parameter<float>( kViewportLimit, _T("ViewportLimit"), 0 )
    //	.DefaultValue( 10000.f )
    //	.Range( 0.f, FLT_MAX );

    // pbBuilder.Parameter<bool>( kUseViewportLimit, _T("UseViewportLimit"), 0 )
    //	.DefaultValue( false );

    pbBuilder.Parameter<TYPE_STRING_TYPE>( kViewportMaskChannel, _T("ViewportMaskChannel"), 0 )
        .DefaultValue( _T("Density") );

    pbBuilder.Parameter<TYPE_STRING_TYPE>( kViewportVectorChannel, _T("ViewportVectorChannel"), 0 )
        .DefaultValue( _T("PRTViewportVector") );

    pbBuilder.Parameter<TYPE_STRING_TYPE>( kViewportColorChannel, _T("ViewportColorChannel"), 0 )
        .DefaultValue( _T("PRTViewportColor") );

    pbBuilder.Parameter<float>( kViewportVectorScale, _T("ViewportVectorScale"), 0 )
        .DefaultValue( 1.f )
        .Range( 0.f, FLT_MAX );

    pbBuilder.Parameter<bool>( kViewportVectorNormalize, _T("ViewportVectorNormalize"), 0 ).DefaultValue( false );

    // Trying to guess automatically.
    // pbBuilder.Parameter<bool>( kViewportVectorEnabled, _T("ViewportVectorEnabled"), 0 )
    //	.DefaultValue( false );

    pbBuilder.Parameter<int>( kViewportReduce, _T("ViewportReduce"), 0 ).DefaultValue( 0 ).Range( 0, INT_MAX );

    pbBuilder.Parameter<bool>( kViewportBoundsEnabled, _T("ViewportBoundsEnabled"), 0 ).DefaultValue( false );

    pbBuilder.Parameter<float>( kPlaybackTime, _T("PlaybackTime") )
        .EnableAnimation( true, IDS_PLAYBACKTIME )
        .DefaultValue( 0.f )
        .Range( -FLT_MAX, FLT_MAX );

    pbBuilder.Parameter<bool>( kUsePlaybackTime, _T("UsePlaybackTime"), 0 ).DefaultValue( false );

    pbBuilder.Parameter<bool>( kUsePlaybackInterpolation, _T("UsePlaybackInterpolation"), 0 ).DefaultValue( false );

    pbBuilder.Parameter<TYPE_STRING_TYPE>( kSequencePath, _T("SequencePath"), 0 )
        .DefaultValue( _T("") )
        .Validator( &SimEmberBaseSequencePathValidator::GetInstance() )
        .Accessor( &SimEmberBaseSequencePathAccessor::GetInstance() );

    pbBuilder.Parameter<int>( kSequenceCacheCapacity, _T("SequenceCacheCapacityMB"), 0 )
        .DefaultValue( DEFAULT_CACHE_SIZE_MB )
        .Range( 0, INT_MAX )
        .Accessor( &SimEmberBaseSequencePathAccessor::GetInstance() );

    pbBuilder.Parameter<int>( kSequenceCacheUsage, _T("SequenceCacheUsageMB"), 0, false, P_READ_ONLY | P_TRANSIENT )
        .Accessor( &SimEmberBaseSequencePathAccessor::GetInstance() );

    pbBuilder.Parameter<bool>( kUseDiskCache, _T("UseDiskCache"), 0 )
        .DefaultValue( false )
        .Accessor( &SimEmberBaseSequencePathAccessor::GetInstance() );

    pbBuilder.Parameter<int>( kSerializeQueueCapacity, _T("SerializeQueueCapacityMB"), 0 )
        .DefaultValue( DEFAULT_CACHE_SIZE_MB / 2 )
        .Range( 0, INT_MAX )
        .Accessor( &SimEmberBaseSequencePathAccessor::GetInstance() );

    pbBuilder.Parameter<int>( kSerializeQueueUsage, _T("SerializeQueueUsageMB"), 0, false, P_READ_ONLY | P_TRANSIENT )
        .Accessor( &SimEmberBaseSequencePathAccessor::GetInstance() );
}

/*
class SimEmberBaseAssetAccessor : public IAssetAccessor
{
protected:
        frantic::tstring m_filename;
public:
        SimEmberBaseAssetAccessor( frantic::tstring filename )
                : m_filename( filename ) {

        }
        MaxSDK::AssetManagement::AssetUser GetAsset() const {
                return MaxSDK::AssetManagement::IAssetManager::GetInstance()->GetAsset( m_filename.c_str(),
MaxSDK::AssetManagement::kAnimationAsset, true );
        }

        bool SetAsset( const MaxSDK::AssetManagement::AssetUser& aNewAssetUser ) {
                return true;
        }

        MaxSDK::AssetManagement::AssetType GetAssetType() const {
                return MaxSDK::AssetManagement::kAnimationAsset;
        }

};


void SimEmberBase::EnumAuxFiles( AssetEnumCallback& nameEnum, DWORD flags ) {

        bool useCache = m_pblock->GetInt( kUseDiskCache );
        const frantic::tstring file = m_dataCache.get_disk_path_pattern();

        double key = 0;
        if( !m_dataCache.empty() ) {
                key = *( m_dataCache.key_begin() );
        }

        frantic::files::filename_pattern filename( file );

        if( useCache && !file.empty() ) {

                if( (flags & FILE_ENUM_CHECK_AWORK1) && TestAFlag( A_WORK1 ) )
                        return;

                if( flags & FILE_ENUM_ACCESSOR_INTERFACE ) {
                        SimEmberBaseAssetAccessor cacheAccessor( filename[key] );

                        if( cacheAccessor.GetAsset().GetId() != MaxSDK::AssetManagement::kInvalidId ) {
                                const frantic::tstring path = cacheAccessor.GetAsset().GetFullFilePath().data();

                                IEnumAuxAssetsCallback* callback = static_cast<IEnumAuxAssetsCallback*>(&nameEnum);
                                callback->DeclareAsset( cacheAccessor );

                        } else {
                                if( MaxSDK::AssetManagement::IAssetManager::GetInstance()->GetAsset(
filename[key].c_str(), MaxSDK::AssetManagement::kAnimationAsset, true ).GetId() != MaxSDK::AssetManagement::kInvalidId )
{

                                        IPathConfigMgr::GetPathConfigMgr()->RecordOutputAsset(
MaxSDK::AssetManagement::IAssetManager::GetInstance()->GetAsset( filename[key].c_str(),
MaxSDK::AssetManagement::kAnimationAsset, true ), nameEnum, flags ); GeomObject::EnumAuxFiles( nameEnum, flags );
                                }
                        }
                }
        }
}
*/

} // namespace max3d
} // namespace ember

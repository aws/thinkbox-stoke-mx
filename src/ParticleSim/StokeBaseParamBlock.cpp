// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource.h"

#include <stoke/max3d/ParticleSim/StokeBase.hpp>
#include <stoke/max3d/ParticleSim/StokeBaseParamBlock.hpp>

#include <frantic/files/filename_sequence.hpp>
#include <frantic/logging/logging_level.hpp>

#include <iparamb2.h>
#include <plugapi.h>

#include <AssetManagement\iassetmanager.h>
#include <IPathConfigMgr.h>

namespace stoke {
namespace max3d {

class StokeBaseSequencePathValidator : public PBValidator {
    StokeBaseSequencePathValidator();

  public:
    static StokeBaseSequencePathValidator& GetInstance();

    virtual BOOL Validate( PB2Value& v );
};

StokeBaseSequencePathValidator::StokeBaseSequencePathValidator() {}

StokeBaseSequencePathValidator& StokeBaseSequencePathValidator::GetInstance() {
    static StokeBaseSequencePathValidator theStokeBaseSequencePathValidator;
    return theStokeBaseSequencePathValidator;
}

BOOL StokeBaseSequencePathValidator::Validate( PB2Value& v ) {
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

class StokeBaseSequencePathAccessor : public PBAccessor {
    StokeBaseSequencePathAccessor();

  public:
    static StokeBaseSequencePathAccessor& GetInstance();

    virtual void Get( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t, Interval& valid );

    virtual void Set( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t );
};

StokeBaseSequencePathAccessor::StokeBaseSequencePathAccessor() {}

StokeBaseSequencePathAccessor& StokeBaseSequencePathAccessor::GetInstance() {
    static StokeBaseSequencePathAccessor theStokeBaseSequencePathAccessor;
    return theStokeBaseSequencePathAccessor;
}

void StokeBaseSequencePathAccessor::Get( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t,
                                         Interval& valid ) {
    if( !owner )
        return;

    if( id == kSequenceCacheUsage ) {
        std::size_t cacheSize = static_cast<StokeBase*>( owner )->m_particleCache.get_usage();

        cacheSize += ( ( 1ui64 << 20 ) - 1 ); // Add 1 less than a MB so we round up.
        cacheSize /= ( 1ui64 << 20 );         // Convert to MB.

        v.i = static_cast<int>( cacheSize );
    } else if( id == kSerializeQueueUsage ) {
        std::size_t cacheSize = static_cast<StokeBase*>( owner )->m_particleCache.get_pending_usage();

        cacheSize += ( ( 1ui64 << 20 ) - 1 ); // Add 1 less than a MB so we round up.
        cacheSize /= ( 1ui64 << 20 );         // Convert to MB.

        v.i = static_cast<int>( cacheSize );
    } else {
        return PBAccessor::Get( v, owner, id, tabIndex, t, valid );
    }
}

void StokeBaseSequencePathAccessor::Set( PB2Value& v, ReferenceMaker* owner_, ParamID id, int tabIndex, TimeValue t ) {
    if( !owner_ )
        return;

    StokeBase* pOwner = static_cast<StokeBase*>( owner_ );

    if( id == kSequencePath ) {
        try {
            if( pOwner->m_pblock->GetInt( kUseDiskCache ) != FALSE )
                pOwner->m_particleCache.set_disk_path( v.s, StokeBase::cache_type::keep_existing );
        } catch( const std::exception& e ) {
            FF_LOG( error ) << e.what() << std::endl;
        }
    } else if( id == kSequenceCacheCapacity ) {
        try {
            v.i = std::max( 0, v.i );
            pOwner->m_particleCache.set_capacity( static_cast<std::size_t>( v.i ) *
                                                  static_cast<std::size_t>( 1 << 20 ) );
        } catch( const std::exception& e ) {
            FF_LOG( error ) << e.what() << std::endl;
        }
    } else if( id == kUseDiskCache ) {
        try {
            if( v.i == FALSE )
                pOwner->m_particleCache.set_disk_path( _T(""), StokeBase::cache_type::keep_existing );
            else
                pOwner->m_particleCache.set_disk_path( pOwner->m_pblock->GetStr( kSequencePath ),
                                                       StokeBase::cache_type::keep_existing );
        } catch( const std::exception& e ) {
            FF_LOG( error ) << e.what() << std::endl;
        }
    } else if( id == kSerializeQueueCapacity ) {
        try {
            v.i = std::max( 0, v.i );
            pOwner->m_particleCache.set_pending_capacity( static_cast<std::size_t>( v.i ) *
                                                          static_cast<std::size_t>( 1 << 20 ) );
        } catch( const std::exception& e ) {
            FF_LOG( error ) << e.what() << std::endl;
        }
    }
}

void InitParamBlockDesc( ParamBlockDesc2& inoutDesc ) {
    inoutDesc.AddParam( kIconSize, _T("IconSize"), TYPE_FLOAT, P_RESET_DEFAULT, 0, p_end );
    inoutDesc.ParamOption( kIconSize, p_default, 30.f );
    inoutDesc.ParamOption( kIconSize, p_range, 0.f, FLT_MAX );

    inoutDesc.AddParam( kViewportEnabled, _T("ViewportEnabled"), TYPE_BOOL, 0, 0, p_end );
    inoutDesc.ParamOption( kViewportEnabled, p_default, TRUE );

    inoutDesc.AddParam( kViewportPercentage, _T("ViewportPercentage"), TYPE_PCNT_FRAC, P_RESET_DEFAULT, 0, p_end );
    inoutDesc.ParamOption( kViewportPercentage, p_default, 1.f );
    inoutDesc.ParamOption( kViewportPercentage, p_range, 0.f, 100.f );

    inoutDesc.AddParam( kViewportLimit, _T("ViewportLimit"), TYPE_FLOAT, 0, 0, p_end );
    inoutDesc.ParamOption( kViewportLimit, p_default, 10000.f );
    inoutDesc.ParamOption( kViewportLimit, p_range, 0.f, FLT_MAX );

    inoutDesc.AddParam( kUseViewportLimit, _T("UseViewportLimit"), TYPE_BOOL, 0, 0, p_end );
    inoutDesc.ParamOption( kUseViewportLimit, p_default, FALSE );

    inoutDesc.AddParam( kViewportVectorChannel, _T("ViewportVectorChannel"), TYPE_STRING, 0, 0, p_end );
    inoutDesc.ParamOption( kViewportVectorChannel, p_default, _T("PRTViewportVector") );

    inoutDesc.AddParam( kViewportColorChannel, _T("ViewportColorChannel"), TYPE_STRING, 0, 0, p_end );
    inoutDesc.ParamOption( kViewportColorChannel, p_default, _T("PRTViewportColor") );

    inoutDesc.AddParam( kViewportVectorScale, _T("ViewportVectorScale"), TYPE_FLOAT, 0, 0, p_end );
    inoutDesc.ParamOption( kViewportVectorScale, p_default, 1.f );
    inoutDesc.ParamOption( kViewportVectorScale, p_range, 0.f, FLT_MAX );

    inoutDesc.AddParam( kViewportVectorNormalize, _T("ViewportVectorNormalize"), TYPE_BOOL, 0, 0, p_end );
    inoutDesc.ParamOption( kViewportVectorNormalize, p_default, FALSE );

    inoutDesc.AddParam( kPlaybackTime, _T("PlaybackTime"), TYPE_FLOAT, P_ANIMATABLE, IDS_PLAYBACKTIME, p_end );
    inoutDesc.ParamOption( kPlaybackTime, p_default, 0.f );
    inoutDesc.ParamOption( kPlaybackTime, p_range, -FLT_MAX, FLT_MAX );

    inoutDesc.AddParam( kUsePlaybackTime, _T("UsePlaybackTime"), TYPE_BOOL, 0, 0, p_end );
    inoutDesc.ParamOption( kUsePlaybackTime, p_default, FALSE );

    inoutDesc.AddParam( kUsePlaybackInterpolation, _T("UsePlaybackInterpolation"), TYPE_BOOL, 0, 0, p_end );
    inoutDesc.ParamOption( kUsePlaybackInterpolation, p_default, TRUE );

    inoutDesc.AddParam( kSequencePath, _T("SequencePath"), TYPE_STRING, P_RESET_DEFAULT, 0, p_end );
    inoutDesc.ParamOption( kSequencePath, p_default, _T("") );
    inoutDesc.ParamOption( kSequencePath, p_validator, &StokeBaseSequencePathValidator::GetInstance() );
    inoutDesc.ParamOption( kSequencePath, p_accessor, &StokeBaseSequencePathAccessor::GetInstance() );

    inoutDesc.AddParam( kSequenceCacheCapacity, _T("SequenceCacheCapacityMB"), TYPE_INT, 0, 0, p_end );
    inoutDesc.ParamOption( kSequenceCacheCapacity, p_default, DEFAULT_CACHE_SIZE_MB );
    inoutDesc.ParamOption( kSequenceCacheCapacity, p_range, 0, INT_MAX );
    inoutDesc.ParamOption( kSequenceCacheCapacity, p_accessor, &StokeBaseSequencePathAccessor::GetInstance() );

    inoutDesc.AddParam( kSequenceCacheUsage, _T("SequenceCacheUsageMB"), TYPE_INT, P_READ_ONLY | P_TRANSIENT, 0,
                        p_end );
    inoutDesc.ParamOption( kSequenceCacheUsage, p_accessor, &StokeBaseSequencePathAccessor::GetInstance() );

    inoutDesc.AddParam( kUseDiskCache, _T("UseDiskCache"), TYPE_BOOL, 0, 0, p_end );
    inoutDesc.ParamOption( kUseDiskCache, p_default, FALSE );
    inoutDesc.ParamOption( kUseDiskCache, p_accessor, &StokeBaseSequencePathAccessor::GetInstance() );

    inoutDesc.AddParam( kSerializeQueueCapacity, _T("SerializeQueueCapacityMB"), TYPE_INT, 0, 0, p_end );
    inoutDesc.ParamOption( kSerializeQueueCapacity, p_default, DEFAULT_CACHE_SIZE_MB / 2 );
    inoutDesc.ParamOption( kSerializeQueueCapacity, p_range, 0, INT_MAX );
    inoutDesc.ParamOption( kSerializeQueueCapacity, p_accessor, &StokeBaseSequencePathAccessor::GetInstance() );

    inoutDesc.AddParam( kSerializeQueueUsage, _T("SerializeQueueUsageMB"), TYPE_INT, P_READ_ONLY | P_TRANSIENT, 0,
                        p_end );
    inoutDesc.ParamOption( kSerializeQueueUsage, p_accessor, &StokeBaseSequencePathAccessor::GetInstance() );

    inoutDesc.AddParam( kChannelsToSave, _T("ChannelsToSave"), TYPE_STRING_TAB, 0, 0, 0, p_end );

    inoutDesc.AddParam( kSimulationMagmaFlow, _T( "SimulationMagma" ), TYPE_REFTARG, 0, 0, p_end );

    inoutDesc.AddParam( kUseMagma, _T( "UseMagma" ), TYPE_BOOL, 0, 0, p_end );
    inoutDesc.ParamOption( kUseMagma, p_default, TRUE );

    inoutDesc.AddParam( kBirthMagmaFlow, _T( "BirthMagma" ), TYPE_REFTARG, 0, 0, p_end );
}

/*
class StokeBaseAssetAccessor : public IAssetAccessor
{
protected:
        frantic::tstring m_filename;
public:
        StokeBaseAssetAccessor( frantic::tstring filename )
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

void StokeBase::EnumAuxFiles( AssetEnumCallback& nameEnum, DWORD flags ) {
        bool useCache = m_pblock->GetInt( kUseDiskCache );
        const frantic::tstring file = m_particleCache.get_disk_path_pattern();

        double key = 0;
        if( !m_particleCache.empty() ) {
                key = *( m_particleCache.key_begin() );
        }

        frantic::files::filename_pattern filename( file );

        if( useCache && !file.empty() ) {

                if( (flags & FILE_ENUM_CHECK_AWORK1) && TestAFlag( A_WORK1 ) )
                        return;

                if( flags & FILE_ENUM_ACCESSOR_INTERFACE ) {
                        StokeBaseAssetAccessor cacheAccessor( filename[key] );

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
} // namespace stoke

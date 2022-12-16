// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/max3d/ParticleSim/IStokeBase.hpp>

#include <stoke/particle_set_interface.hpp>
#include <stoke/particle_set_serializer.hpp>

#include <krakatoa/max3d/IPRTParticleObjectExt.hpp>
#include <krakatoa/max3d/PRTObject.hpp>

#include <frantic/files/sequence_cache.hpp>

#include <frantic/magma/max3d/IErrorReporter.hpp>

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#define DEFAULT_CACHE_SIZE_MB 1024
#define DEFAULT_CACHE_SIZE ( static_cast<__int64>( DEFAULT_CACHE_SIZE_MB ) * ( 1i64 << 20 ) )

namespace stoke {
namespace max3d {

using frantic::particles::particle_istream_ptr;

class StokeBase : public krakatoa::max3d::PRTObject<StokeBase>,
                  public IStokeBase,
                  public krakatoa::max3d::IPRTParticleObjectExtOwner,
                  public frantic::magma::max3d::ErrorReporter {
  public:
    StokeBase( BOOL loading );

    virtual ~StokeBase();

    virtual FPInterfaceDesc* GetDescByID( Interface_ID id );

    virtual BaseInterface* GetInterface( Interface_ID id );

    virtual IOResult Load( ILoad* iload );

    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

    // From IStokeBase
    virtual void ResetParticleCache();

    virtual void FlushParticleCache();

    virtual void FlushParticleCacheAsync( Value* pFinishCallback );

    virtual void CancelFlushParticleCache();

    virtual void SetChannelsToSave( const Tab<const MCHAR*>* pChannels );

    virtual void InitializeParticleCache( const MCHAR* szSourceSequencePattern );

    virtual void AddParticleSet( IParticleSet* pParticleSet, TimeValue cacheTime );

    virtual void WriteParticleCache( const MCHAR* szDestSequencePattern, bool memoryOnly ) const;

    virtual Tab<TimeValue> GetParticleCacheTimes() const;

    virtual Tab<TimeValue> GetMemoryCacheTimes() const;

    virtual void SetNumSerializerThreads( int numThreads );

    virtual void SetSerializerCallback( Value* pCallback );

    virtual void InitSimMagmaHolder();

    virtual frantic::particles::particle_istream_ptr
    create_particle_stream( INode* pNode, Interval& outValidity,
                            boost::shared_ptr<frantic::max3d::particles::IMaxKrakatoaPRTEvalContext> pEvalContext );

  protected:
    virtual ClassDesc2* GetClassDesc();

    virtual Mesh* GetIconMesh( Matrix3& outTM );
#if MAX_VERSION_MAJOR >= 25
    virtual MaxSDK::SharedMeshPtr GetIconMeshShared( Matrix3& outTM );
#endif

    virtual void SetIconSize( float scale );

    virtual bool InWorldSpace() const;

    virtual particle_istream_ptr
    GetInternalStream( INode* pNode, TimeValue t, Interval& inoutValidity,
                       frantic::max3d::particles::IMaxKrakatoaPRTEvalContextPtr pEvalContext );

  private:
    /**
     * Find the closest particle set and get the distance for doing velocity extrapolation.
     * @param t The time to search for.
     * @param[out] outTimeOffset The time distance to the chosen sample. This is positive if the result is "before" the
     * requested time, or negative if the result is "after".
     * @return A pointer to particle set nearest to the requested time.
     */
    // IParticleSet* GetNearestSample( TimeValue t, TimeValue& outTimeOffset );
    particle_set_interface_ptr GetNearestSample( double frame, double& outTimeOffset );

    /**
     * Find the closest two particle sets and get the distance for doing cubic interpolation.
     * @param t The time to search for. The result sets will be the closest samples before and after.
     * @param[out] outTimeOffset The time distance to the chosen sample. This is positive if the left result is "before"
     * the requested time, or negative if the left result is "after".
     * @param[out] outTimeStep The time distance between the chosen samples. This is positive if the left result is
     * "before" the requested time, or negative if the left result is "after".
     * @return A pair of pointers to the two closest particle sets. The first pair entry (ie. the left one) is the
     * closest to the requested time. Therefore it might be before or after the actual requested time.
     */
    // std::pair<IParticleSet*,IParticleSet*> GetBracketingSamples( TimeValue t, TimeValue& outTimeOffset, TimeValue&
    // outTimeStep );
    std::pair<particle_set_interface_ptr, particle_set_interface_ptr>
    GetBracketingSamples( double frame, double& outTimeOffset, double& outTimeStep );

    /**
     * Creates a new instance of a particle_isteam subclass that represents an empty particle stream. The current and
     * native channel maps will be set appropriately. \return A new empty particle stream instance.
     */
    particle_istream_ptr CreateEmptyStream( const frantic::channels::channel_map& outMap ) const;

    /**
     * Callback that is invoked when a file is finished serializing.
     */
    void SerializeCallbackImpl( const frantic::tstring& filePath ) const;

    class StokeBasePLC;

  private:
    struct particle_set_size_estimator {
        inline std::size_t operator()( const particle_set_interface_ptr& pParticleSet ) const {
            return pParticleSet->get_memory_usage();
        }
    };

    typedef frantic::files::sequence_cache<particle_set_interface_ptr, particle_set_serializer,
                                           particle_set_size_estimator>
        cache_type;

    particle_set_serializer m_serializer; // Must be declared before 'm_particleCache' in order to be initialized first

    cache_type m_particleCache;

    boost::shared_ptr<IParticleObjectExt> m_iparticleobjectext;

    friend class StokeBaseSequencePathAccessor;
    /*
    void EnumAuxFiles( AssetEnumCallback& nameEnum, DWORD flags );
    */
};

} // namespace max3d
} // namespace stoke

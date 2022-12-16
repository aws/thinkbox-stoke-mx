// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <ifnpub.h>

#define StokeBase_INTERFACE Interface_ID( 0x78c724aa, 0x8e77b2f )

namespace stoke {
namespace max3d {

class IParticleSet;

class IStokeBase : public FPMixinInterface {
  public:
    virtual ~IStokeBase();

    virtual FPInterfaceDesc* GetDesc() { return this->GetDescByID( StokeBase_INTERFACE ); }

    /**
     * Drops all the cache data stored in memory without flushing to disk.
     */
    virtual void ResetParticleCache() = 0;

    /**
     * Ensures that all cache data is stored on disk. This is a nop if the disk cache is not enabled.
     */
    virtual void FlushParticleCache() = 0;

    /**
     * Begins an asynchronous flush operation where all items in the cache are copied into the disk cache if they
     * weren't already. Since this is asynchronous, it returns immediately. The callback is invoked once after all
     * the items are flushed.
     * \note Any other call that modifies the cache is invalid while the flush is processing.
     * \param pFinishCallback A MAXScript Function that is invoked when the asynchronous flush completes.
     */
    virtual void FlushParticleCacheAsync( Value* pFinishCallback ) = 0;

    /**
     * Waits for the cache to become empty, then changes the channels that will be saved to the disk cache.
     *
     * HACK: I really hate this. I think it may have been better to specify the subset of channels to save when adding
     * new data via AddParticleSet(), but that's kind of weird too since we really want all data samples to have the
     * same channels and setting it each time would imply it could be different.
     */
    virtual void SetChannelsToSave( const Tab<const MCHAR*>* pChannels ) = 0;

    /**
     * Cancels an asynchronous flush operation. The operations are not immediately cancelled, but will be sometime in
     * the future. The original flush's callback will still be invoked.
     */
    virtual void CancelFlushParticleCache() = 0;

    /**
     * Drops the current cache and synchronizes with the specified sequence.
     */
    virtual void InitializeParticleCache( const MCHAR* szSourceSequencePattern ) = 0;

    /**
     * Adds new particle data to the cache with the specific time. Will replace any existing data at the same time.
     */
    virtual void AddParticleSet( IParticleSet* pParticleSet, TimeValue cacheTime ) = 0;

    /**
     * Copies all data from the cache (memory & disk) to an alternate destination on disk.
     * \param szDestSequencePattern The filename pattern describing the location of the sequence on disk.
     * \param memoryOnly If true, only data current stored in the memory cache is serialized. If false, cache data on
     * disk is copied too.
     */
    virtual void WriteParticleCache( const MCHAR* szDestSequencePattern, bool memoryOnly = false ) const = 0;

    /**
     * Gets a list of all the times for which particle data was stored in the cache.
     */
    virtual Tab<TimeValue> GetParticleCacheTimes() const = 0;

    /**
     * Gets a list of all the times for which particle data is currently stored in memory.
     * \note This is a subset of GetParticleCacheTimes().
     */
    virtual Tab<TimeValue> GetMemoryCacheTimes() const = 0;

    /**
     * Specifies the number of threads to use as background workers when serializing data.
     * \param numThreads The new number of threads to use.
     */
    virtual void SetNumSerializerThreads( int numThreads ) = 0;

    /**
     * Assigns a callback function that is invoked after each item is serialized.
     * \note The callback must be of the form: `fn theCallback filePath = ( ... )` where filePath is a string.
     * \param pCallback A MAXScript function (ie. class Function) that is invoked with the string path of the file
     * serialized.
     */
    virtual void SetSerializerCallback( Value* pCallback ) = 0;

    virtual void InitSimMagmaHolder() = 0;

  private:
    void AddParticleSetMXS( FPInterface* pParticleSet, TimeValue cacheTime );

  public:
    enum {
        kFnResetParticleCache,
        kFnFlushParticleCache,
        kFnAddParticleSet,
        kFnInitializeParticleCache,
        kFnWriteParticleCache,
        kFnGetParticleCacheTimes,
        kFnGetMemoryCacheTimes,
        kFnSetNumSerializerThreads,
        kFnFlushParticleCacheAsync,
        kFnSetSerializerCallback,
        kFnCancelFlushParticleCache,
        kFnSetChannelsToSave,
        kFnInitSimMagmaHolder
    };

#pragma warning( push, 3 )
    BEGIN_FUNCTION_MAP
    VFN_0( kFnResetParticleCache, ResetParticleCache )
    VFN_0( kFnFlushParticleCache, FlushParticleCache )
    VFN_1( kFnFlushParticleCacheAsync, FlushParticleCacheAsync, TYPE_VALUE )
    VFN_0( kFnCancelFlushParticleCache, CancelFlushParticleCache )
    VFN_1( kFnInitializeParticleCache, InitializeParticleCache, TYPE_STRING )
    VFN_2( kFnAddParticleSet, AddParticleSetMXS, TYPE_INTERFACE, TYPE_TIMEVALUE )
    VFN_2( kFnWriteParticleCache, WriteParticleCache, TYPE_STRING, TYPE_bool )
    FN_0( kFnGetParticleCacheTimes, TYPE_TIMEVALUE_TAB_BV, GetParticleCacheTimes )
    FN_0( kFnGetMemoryCacheTimes, TYPE_TIMEVALUE_TAB_BV, GetMemoryCacheTimes )
    VFN_1( kFnSetNumSerializerThreads, SetNumSerializerThreads, TYPE_INT )
    VFN_1( kFnSetSerializerCallback, SetSerializerCallback, TYPE_VALUE )
    VFN_1( kFnSetChannelsToSave, SetChannelsToSave, TYPE_STRING_TAB )
    VFN_0( kFnInitSimMagmaHolder, InitSimMagmaHolder )
    END_FUNCTION_MAP
#pragma warning( pop )

    static void InitInterfaceDesc( FPInterfaceDesc& inoutDesc );
};

} // namespace max3d
} // namespace stoke

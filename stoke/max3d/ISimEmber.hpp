// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <boost/shared_ptr.hpp>

#include <frantic/max3d/fnpublish/MixinInterface.hpp>
#include <frantic/max3d/ifnpub_wrap.hpp>

class INode;

#define SimEmber_INTERFACE Interface_ID( 0x6bb34aa, 0x47b41d41 )
#define SimEmberBase_INTERFACE Interface_ID( 0x2d9c280d, 0x3db17251 )

namespace ember {
namespace max3d {

class ISimEmber : public FPMixinInterface {
  public:
    virtual void reset_simulation() = 0;

    virtual void force_update( INode* node, TimeValue t ) = 0;

    // From FPMixinInterface
    virtual FPInterfaceDesc* GetDesc() { return GetDescByID( SimEmber_INTERFACE ); }

    virtual FPInterfaceDesc* GetDescByID( Interface_ID id ) = 0;

  public:
    enum { kFnResetSimulation };

#pragma warning( push, 3 )
#pragma warning( disable : 4238 )
    BEGIN_FUNCTION_MAP
    VFN_0( kFnResetSimulation, reset_simulation )
    END_FUNCTION_MAP
#pragma warning( pop )

    inline static void init_fpinterface_desc( FPInterfaceDesc& desc ) {
        desc.AppendFunction( kFnResetSimulation, _T("ResetSimulation"), 0, TYPE_VOID, 0, 0, PARAMTYPE_END );
    }
};

inline ISimEmber* GetSimEmberInterface( Animatable* anim ) {
    return anim ? static_cast<ISimEmber*>( anim->GetInterface( SimEmber_INTERFACE ) ) : NULL;
}

class ISimEmberBase : public frantic::max3d::fnpublish::MixinInterface<ISimEmberBase> {
  public:
    virtual ~ISimEmberBase();

    static ThisInterfaceDesc* GetStaticDesc();

    virtual ThisInterfaceDesc* GetDesc();

    virtual void ResetCache() = 0;

    /**
     * Ensures that all cache data is stored on disk. This is a nop if the disk cache is not enabled.
     */
    virtual void FlushCache() = 0;

    /**
     * Begins an asynchronous flush operation where all items in the cache are copied into the disk cache if they
     * weren't already. Since this is asynchronous, it returns immediately. The callback is invoked once after all
     * the items are flushed.
     * \note Any other call that modifies the cache is invalid while the flush is processing.
     * \param pFinishCallback A MAXScript Function that is invoked when the asynchronous flush completes.
     */
    virtual void FlushCacheAsync( Value* pFinishCallback ) = 0;

    /**
     * Cancels an asynchronous flush operation. The operations are not immediately cancelled, but will be sometime in
     * the future. The original flush's callback will still be invoked.
     */
    virtual void CancelFlushCache() = 0;

    /**
     * Clears the existing cached data (losing anything not already serialized) then synchornizes with the existing disk
     * sequence at the specified path. \param szSourceSequencePattern The path to an existing sequence to synchronize
     * the cache with.
     */
    virtual void InitializeCache( const MCHAR* szSourceSequencePattern ) = 0;

    /**
     * Adds new data to the cache with the specific time. Will replace any existing data at the same time.
     */
    virtual void AddData( FPInterface* pDataSet, double cacheFrame ) = 0;

    /**
     * Gets a list of all the times for which data was stored in the cache.
     */
    virtual Tab<double> GetCacheTimes() = 0;

    /**
     * Gets a list of all the times for which data is currently stored in memory.
     * \note This is a subset of GetCacheTimes().
     */
    virtual Tab<double> GetMemoryCacheTimes() = 0;

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
};

} // namespace max3d
} // namespace ember

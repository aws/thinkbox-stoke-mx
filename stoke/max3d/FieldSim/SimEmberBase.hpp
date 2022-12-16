// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/max3d/EmberObjectBase.hpp>
#include <stoke/max3d/ISimEmber.hpp>

#include <frantic/files/sequence_cache.hpp>
#include <frantic/volumetrics/field_interface.hpp>

#include <boost/exception/all.hpp>

#define DEFAULT_CACHE_SIZE_MB 1024
#define DEFAULT_CACHE_SIZE ( static_cast<__int64>( DEFAULT_CACHE_SIZE_MB ) * ( 1i64 << 20 ) )

namespace ember {
namespace max3d {

typedef boost::shared_ptr<frantic::volumetrics::field_interface> field_interface_ptr;

class SimEmberBase : public EmberObjectBase, public ISimEmberBase {
  public:
    SimEmberBase( BOOL loading );

    virtual ~SimEmberBase();

    // From Animatable
    virtual BaseInterface* GetInterface( Interface_ID id );

    // From ReferenceTarget
    virtual IOResult Load( ILoad* iload );

    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

    // From BaseObject
    virtual CreateMouseCallBack* GetCreateMouseCallBack();

    // From ISimEmberBase
    virtual void ResetCache();

    virtual void FlushCache();

    virtual void FlushCacheAsync( Value* pFinishCallback );

    virtual void CancelFlushCache();

    virtual void InitializeCache( const MCHAR* szSourceSequencePattern );

    virtual void AddData( FPInterface* pDataSet, double cacheFrame );

    virtual Tab<double> GetCacheTimes();

    virtual Tab<double> GetMemoryCacheTimes();

    virtual void SetNumSerializerThreads( int numThreads );

    virtual void SetSerializerCallback( Value* pCallback );

  protected:
    // From EmberObjectBase
    virtual void EvalField( EmberPipeObject& objToPopulate, TimeValue t );

    virtual void SetIconScale( float scale );

    virtual ClassDesc2* GetClassDesc();

  private:
    /**
     * Find the closest data set and get the distance for doing velocity extrapolation.
     * @param t The time to search for.
     * @param[out] outTimeOffset The time distance to the chosen sample. This is positive if the result is "before" the
     * requested time, or negative if the result is "after".
     * @return A pointer to data set nearest to the requested time.
     */
    field_interface_ptr GetNearestSample( double frame, double& outTimeOffset );

    /**
     * Find the closest two data sets and get the distance for doing cubic interpolation.
     * @param t The time to search for. The result sets will be the closest samples before and after.
     * @param[out] outTimeOffset The time distance to the chosen sample. This is positive if the left result is "before"
     * the requested time, or negative if the left result is "after".
     * @param[out] outTimeStep The time distance between the chosen samples. This is positive if the left result is
     * "before" the requested time, or negative if the left result is "after".
     * @return A pair of pointers to the two closest data sets. The first pair entry (ie. the left one) is the closest
     * to the requested time. Therefore it might be before or after the actual requested time.
     */
    std::pair<field_interface_ptr, field_interface_ptr> GetBracketingSamples( double frame, double& outTimeOffset,
                                                                              double& outTimeStep );

    /**
     * \return A new instance of an empty data set.
     */
    field_interface_ptr CreateEmptySample( const frantic::channels::channel_map& outMap ) const;

    /**
     * Callback that is invoked when a file is finished serializing.
     */
    void SerializeCallbackImpl( const frantic::tstring& filePath ) const;

    class SimEmberBasePLC;

  private:
    struct serializer_data {
        field_interface_ptr pField;
        frantic::graphics::boundbox3f bounds;
        float spacing;
        std::size_t memoryUsage;
    };

    struct field_size_estimator {
        std::size_t operator()( const serializer_data& pParticleSet ) const;
    };

    struct field_serializer {
        boost::exception_ptr m_storedError;

        void serialize( const frantic::tstring& path, const serializer_data& val ) const /*throw()*/;

        serializer_data deserialize( const frantic::tstring& path ) const;

        void process_errors();
    };

    typedef frantic::files::sequence_cache<serializer_data, field_serializer, field_size_estimator> cache_type;

    field_serializer m_serializer; // Must be declared before 'm_dataCache' in order to be initialized first

    cache_type m_dataCache;

    friend class SimEmberBaseSequencePathAccessor;
    /*
    void EnumAuxFiles( AssetEnumCallback& nameEnum, DWORD flags );
    */
};

} // namespace max3d
} // namespace ember

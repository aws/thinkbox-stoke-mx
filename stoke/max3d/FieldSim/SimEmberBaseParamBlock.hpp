// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>

namespace ember {
namespace max3d {

enum { kMainBlock };

enum {
    kIconSize,
    kViewportPercentage,
    kViewportLimit,
    kUseViewportLimit,
    kViewportMaskChannel,
    kViewportVectorChannel,
    kViewportColorChannel,
    kViewportVectorScale,
    kViewportVectorNormalize,
    kPlaybackTime,
    kUsePlaybackTime,
    kUsePlaybackInterpolation,
    kSequencePath,
    kSequenceCacheCapacity,
    kSequenceCacheUsage,
    kUseDiskCache,
    kSerializeQueueCapacity,
    kSerializeQueueUsage,
    kViewportEnabled,
    kViewportVectorEnabled,
    kViewportBoundsEnabled,
    kViewportReduce
};

void InitParamBlockDesc( frantic::max3d::ParamBlockBuilder& pbBuilder );

inline float get_icon_size( IParamBlock2* pb, TimeValue t ) { return frantic::max3d::get<float>( pb, kIconSize, t ); }

inline float get_viewport_percentage( IParamBlock2* pb ) {
    return frantic::max3d::get<float>( pb, kViewportPercentage );
}

inline boost::int64_t get_viewport_limit( IParamBlock2* pb ) {
    return static_cast<boost::int64_t>( frantic::max3d::get<float>( pb, kViewportLimit ) * 1000.0 );
}

inline const MCHAR* get_viewport_mask_channel( IParamBlock2* pb ) {
    return frantic::max3d::get<const MCHAR*>( pb, kViewportMaskChannel );
}

inline const MCHAR* get_viewport_vector_channel( IParamBlock2* pb ) {
    return frantic::max3d::get<const MCHAR*>( pb, kViewportVectorChannel );
}

inline const MCHAR* get_viewport_color_channel( IParamBlock2* pb ) {
    return frantic::max3d::get<const MCHAR*>( pb, kViewportColorChannel );
}

inline float get_viewport_vector_scale( IParamBlock2* pb, TimeValue t ) {
    return frantic::max3d::get<float>( pb, kViewportVectorScale, t );
}

inline bool get_viewport_vector_normalize( IParamBlock2* pb ) {
    return frantic::max3d::get<bool>( pb, kViewportVectorNormalize );
}

inline float get_playback_time( IParamBlock2* pb, TimeValue t ) {
    return frantic::max3d::get<float>( pb, kPlaybackTime, t );
}

inline bool get_use_playback_time( IParamBlock2* pb ) { return frantic::max3d::get<bool>( pb, kUsePlaybackTime ); }

inline bool get_use_playback_interpolation( IParamBlock2* pb ) {
    return frantic::max3d::get<bool>( pb, kUsePlaybackInterpolation );
}

inline const MCHAR* get_sequence_path( IParamBlock2* pb ) {
    return frantic::max3d::get<const MCHAR*>( pb, kSequencePath );
}

inline boost::int64_t get_sequence_cache_capacity( IParamBlock2* pb ) {
    return static_cast<boost::int64_t>( frantic::max3d::get<int>( pb, kSequenceCacheCapacity ) ) * 1024i64;
}

inline boost::int64_t get_sequence_cache_usage( IParamBlock2* pb ) {
    return static_cast<boost::int64_t>( frantic::max3d::get<int>( pb, kSequenceCacheUsage ) ) * 1024i64;
}

inline bool get_use_disk_cache( IParamBlock2* pb ) { return frantic::max3d::get<bool>( pb, kUseDiskCache ); }

inline boost::int64_t get_serializer_capacity( IParamBlock2* pb ) {
    return static_cast<boost::int64_t>( frantic::max3d::get<int>( pb, kSerializeQueueCapacity ) ) * 1024i64;
}

inline boost::int64_t get_serializer_usage( IParamBlock2* pb ) {
    return static_cast<boost::int64_t>( frantic::max3d::get<int>( pb, kSerializeQueueUsage ) ) * 1024i64;
}

inline bool get_viewport_enabled( IParamBlock2* pb ) { return frantic::max3d::get<bool>( pb, kViewportEnabled ); }

inline bool get_viewport_vector_enabled( IParamBlock2* pb ) {
    return frantic::max3d::get<bool>( pb, kViewportVectorEnabled );
}

inline bool get_viewport_bounds_enabled( IParamBlock2* pb ) {
    return frantic::max3d::get<bool>( pb, kViewportBoundsEnabled );
}

inline int get_viewport_reduce( IParamBlock2* pb ) { return frantic::max3d::get<int>( pb, kViewportReduce ); }

} // namespace max3d
} // namespace ember

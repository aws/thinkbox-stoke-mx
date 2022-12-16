// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace stoke {
namespace max3d {

enum {
    kIconSize,
    kViewportPercentage,
    kViewportLimit,
    kUseViewportLimit,
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
    kChannelsToSave,
    kSimulationMagmaFlow,
    kUseMagma,
    kBirthMagmaFlow
};

void InitParamBlockDesc( ParamBlockDesc2& inoutDesc );

} // namespace max3d
} // namespace stoke

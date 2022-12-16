// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <boost/shared_ptr.hpp>

#define GeometryMagmaHolder_NAME _T("GeometryMagmaHolder")
#define GeometryMagmaHolder_CLASS_ID Class_ID( 0x5c5d40e4, 0x2fe72148 )

namespace GeometryMagmaHolderParams {
enum {
    kMagmaFlow,
    kMagmaInternalFlow,
    kMagmaNote,
    kMagmaTrackID,
    kMagmaTextureMapSources,
    kMagmaCurrentPreset,
    kMagmaCurrentFolder,
    kMagmaAutoUpdate,
    kMagmaInteractiveMode,
    kMagmaAutomaticRenameOFF,
    kMagmaGeometryObjectsList,
    kMagmaNeedCAUpdate,
    kMagmaIsRenderElement,

    kMagmaLastErrorMessage,
    kMagmaLastErrorNode,

    kMagmaNodeMaxDataDEPRECATED, // MAX-specific data, stored as a sparse tab of sorted RefTargets of type
                                 // MagmaNodeMaxData
    kMagmaNodeMaxData
};
}

ReferenceTarget* CreateGenomeMagmaHolder();

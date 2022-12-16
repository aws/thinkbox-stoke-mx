// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <tbb/task_scheduler_init.h>

#include <frantic/max3d/maxscript/maxscript.hpp>

#include <iparamb2.h>
#include <plugapi.h>

#include <vector>

extern void InitializeLogging();

namespace stoke {
namespace max3d {
extern void InitStoke();

ClassDesc2* GetStokeBaseDesc();

// ClassDesc2* GetConcreteTestDesc();
} // namespace max3d
} // namespace stoke

extern ClassDesc2* GetTexmapNodeClassDesc();
extern ClassDesc2* GetEmberAtmosphericDesc();
extern ClassDesc2* GetGenomeMagmaHolderDesc();
extern ClassDesc2* GetMagmaGeometryChannelModifierDesc();
extern ClassDesc* GetFieldMesherClassDesc();

extern void get_legacy_genome_class_descs( std::vector<ClassDesc*>& outDescs );

namespace ember {
namespace max3d {
extern ClassDesc* GetEmberHolderClassDesc();
extern ClassDesc2* GetSimEmberHolderDesc();
extern ClassDesc2* GetFieldNodeDesc();
extern ClassDesc2* GetFloodVelocityLoaderPFOperatorDesc();
extern ClassDesc2* GetFieldTexmapDesc();
extern ClassDesc2* GetEmberForceObjectDesc();
extern ClassDesc2* GetEmberForceModDesc();
extern ClassDesc2* GetPRTEmberBaseDesc();

extern ClassDesc2* GetEmberPipeObjectDesc();
extern ClassDesc2* GetEmberLoaderDesc();
extern ClassDesc2* GetFieldMagmaDesc();
extern ClassDesc2* GetEmberViewportModDesc();
extern ClassDesc2* GetEmberFluidMotionModDesc();
// extern ClassDesc2* GetParticlePipeObjectDesc();
// extern ClassDesc2* GetEmberToParticlesDesc();
extern ClassDesc2* GetEmberFromMeshDesc();
extern ClassDesc2* GetSimEmberBaseDesc();
extern ClassDesc2* GetSimEmberOwnerDesc();

} // namespace max3d
} // namespace ember

namespace frantic {
namespace magma {
namespace nodes {
namespace max3d {
extern void GetClassDescs( std::vector<ClassDesc*>& outClassDescs );
}
} // namespace nodes
} // namespace magma
} // namespace frantic

HINSTANCE hInstance;

namespace {
inline std::vector<ClassDesc*>& GetClassDescs() {
    static std::vector<ClassDesc*> theDescs;
    return theDescs;
}
} // namespace

BOOL WINAPI DllMain( HINSTANCE hinstDLL, ULONG fdwReason, LPVOID /*lpvReserved*/ ) {
    if( fdwReason == DLL_PROCESS_ATTACH ) {
        hInstance = hinstDLL; // Hang on to this DLL's instance handle.
    }

    return TRUE;
}

TCHAR* GetString( int id ) {
    static TCHAR buf[256];

    if( hInstance )
        return LoadString( hInstance, id, buf, sizeof( buf ) / sizeof( TCHAR ) ) ? buf : NULL;
    return NULL;
}

tbb::task_scheduler_init g_taskScheduler( tbb::task_scheduler_init::deferred );

extern "C" {
__declspec( dllexport ) int LibInitialize() {
    g_taskScheduler.initialize();

    stoke::max3d::InitStoke();

    InitializeLogging();

    GetClassDescs().clear();
    GetClassDescs().push_back( stoke::max3d::GetStokeBaseDesc() );

    GetClassDescs().push_back( ember::max3d::GetEmberHolderClassDesc() );
    GetClassDescs().push_back( ember::max3d::GetSimEmberHolderDesc() );
    GetClassDescs().push_back( ember::max3d::GetPRTEmberBaseDesc() );
    GetClassDescs().push_back( ember::max3d::GetFloodVelocityLoaderPFOperatorDesc() );
    GetClassDescs().push_back( ember::max3d::GetFieldTexmapDesc() );
    GetClassDescs().push_back( ember::max3d::GetEmberForceObjectDesc() );
    GetClassDescs().push_back( ember::max3d::GetEmberForceModDesc() );

    GetClassDescs().push_back( ember::max3d::GetEmberPipeObjectDesc() );
    GetClassDescs().push_back( ember::max3d::GetEmberLoaderDesc() );
    GetClassDescs().push_back( ember::max3d::GetFieldMagmaDesc() );
    GetClassDescs().push_back( ember::max3d::GetEmberViewportModDesc() );
    GetClassDescs().push_back( ember::max3d::GetEmberFluidMotionModDesc() );
    // GetClassDescs().push_back( ember::max3d::GetParticlePipeObjectDesc() );
    // GetClassDescs().push_back( ember::max3d::GetEmberToParticlesDesc() );
    GetClassDescs().push_back( ember::max3d::GetEmberFromMeshDesc() );
    GetClassDescs().push_back( ember::max3d::GetSimEmberBaseDesc() );
    GetClassDescs().push_back( ember::max3d::GetSimEmberOwnerDesc() );
    GetClassDescs().push_back( GetEmberAtmosphericDesc() );
    GetClassDescs().push_back( GetFieldMesherClassDesc() );

    GetClassDescs().push_back( GetMagmaGeometryChannelModifierDesc() );
    GetClassDescs().push_back( GetGenomeMagmaHolderDesc() );

    GetClassDescs().push_back( GetTexmapNodeClassDesc() );
    GetClassDescs().push_back( ember::max3d::GetFieldNodeDesc() );
    frantic::magma::nodes::max3d::GetClassDescs( GetClassDescs() );

    get_legacy_genome_class_descs( GetClassDescs() );

    return TRUE;
}

__declspec( dllexport ) int LibShutdown() {
    g_taskScheduler.terminate();

    return TRUE;
}

// This function returns a string that describes the DLL and where the user
// could purchase the DLL if they don't have it.
__declspec( dllexport ) const TCHAR* LibDescription() { return _T("Stoke particle and volume plugin for 3ds Max."); }

// This function returns the number of plug-in classes this DLL
// TODO: Must change this number when adding a new class
__declspec( dllexport ) int LibNumberClasses() { return (int)GetClassDescs().size(); }

// This function returns the number of plug-in classes this DLL
__declspec( dllexport ) ClassDesc* LibClassDesc( int i ) { return GetClassDescs()[i]; }

// This function returns a pre-defined constant indicating the version of
// the system under which it was compiled.  It is used to allow the system
// to catch obsolete DLLs.
__declspec( dllexport ) ULONG LibVersion() { return VERSION_3DSMAX; }

} // extern "C"

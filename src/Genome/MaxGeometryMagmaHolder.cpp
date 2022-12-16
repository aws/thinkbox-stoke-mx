// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/Genome/MaxGeometryMagmaSingleton.hpp>

#include <frantic/logging/logging_level.hpp>
#include <frantic/magma/max3d/MagmaHolder.hpp>

extern HINSTANCE hInstance;

#define GenomeMagmaHolder_CLASS_NAME _M( "GenomeMagmaHolder" )
#define GenomeMagmaHolder_CLASS_ID Class_ID( 0x5c5d40e4, 0x2fe72148 )

class GenomeMagmaHolderClassDesc : public frantic::magma::max3d::MagmaHolderClassDesc {
  public:
    GenomeMagmaHolderClassDesc();

    virtual ~GenomeMagmaHolderClassDesc();

    virtual const MCHAR* ClassName();
    virtual HINSTANCE HInstance();
    virtual Class_ID ClassID();
    virtual void* Create( BOOL loading );
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return ClassName(); }
#endif
};

class GenomeMagmaHolder : public frantic::magma::max3d::MagmaHolder {
  protected:
    virtual ClassDesc2* GetClassDesc();

  public:
    GenomeMagmaHolder( BOOL loading );

    virtual ~GenomeMagmaHolder();

    virtual bool is_read_only() const;
};

ReferenceTarget* CreateGenomeMagmaHolder() { return new GenomeMagmaHolder( FALSE ); }

ClassDesc2* GetGenomeMagmaHolderDesc() {
    static GenomeMagmaHolderClassDesc theGenomeMagmaHolderClassDesc;
    return &theGenomeMagmaHolderClassDesc;
}

GenomeMagmaHolderClassDesc::GenomeMagmaHolderClassDesc() {}

GenomeMagmaHolderClassDesc::~GenomeMagmaHolderClassDesc() {}

const MCHAR* GenomeMagmaHolderClassDesc::ClassName() { return GenomeMagmaHolder_CLASS_NAME; }

HINSTANCE GenomeMagmaHolderClassDesc::HInstance() { return hInstance; }

Class_ID GenomeMagmaHolderClassDesc::ClassID() { return GenomeMagmaHolder_CLASS_ID; }

void* GenomeMagmaHolderClassDesc::Create( BOOL loading ) { return new GenomeMagmaHolder( loading ); }

ClassDesc2* GenomeMagmaHolder::GetClassDesc() { return GetGenomeMagmaHolderDesc(); }

GenomeMagmaHolder::GenomeMagmaHolder( BOOL /*loading*/ )
    : frantic::magma::max3d::MagmaHolder( MaxGeometryMagmaSingleton::get_instance().create_magma_instance() ) {
    // We need our paramblock always because the loading code will touch it (possibly inappropriately). We might want to
    // rework that. if( !loading )
    GetGenomeMagmaHolderDesc()->MakeAutoParamBlocks( this );
}

GenomeMagmaHolder::~GenomeMagmaHolder() {}

bool GenomeMagmaHolder::is_read_only() const {
    bool result = false;

    if( GetCOREInterface()->IsNetworkRenderServer() )
        return true;

    return result || MagmaHolder::is_read_only();
}

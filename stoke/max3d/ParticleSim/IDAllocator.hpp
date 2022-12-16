// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/max3d/ParticleSim/RefCountedInterface.hpp>

#include <boost/shared_ptr.hpp>

#include <ifnpub.h>

#define IDAllocator_INTERFACE Interface_ID( 0x71aa4eb5, 0x7bc169f )

namespace stoke {
class id_generator_interface;
}

namespace stoke {
namespace max3d {

class IDAllocator : public RefCountedInterface<FPMixinInterface> {
  public:
    typedef __int64 id_type;

  public:
    IDAllocator( boost::shared_ptr<id_generator_interface> pImpl );

    virtual ~IDAllocator();

    virtual FPInterfaceDesc* GetDesc();

    virtual boost::shared_ptr<id_generator_interface> GetImpl();

    virtual void Reset();

    virtual id_type AllocateIDs( __int64 numIDs );

  public:
    enum { kFnReset, kFnAllocateIDs };

    BEGIN_FUNCTION_MAP
    VFN_0( kFnReset, Reset )
    FN_1( kFnAllocateIDs, TYPE_INT64, AllocateIDs, TYPE_INT64 )
    END_FUNCTION_MAP

  private:
    boost::shared_ptr<id_generator_interface> m_pImpl;
};

} // namespace max3d
} // namespace stoke

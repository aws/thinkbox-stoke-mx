// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/ParticleSim/IDAllocator.hpp>

#include <stoke/id_generator.hpp>

#include <boost/make_shared.hpp>

namespace stoke {
namespace max3d {

namespace {
FPInterfaceDesc theIDAllocatorDesc( IDAllocator_INTERFACE, _M( "IDAllocator" ), 0, NULL, FP_MIXIN,

                                    // IDAllocator::kFnReset, _T("Reset"), 0, TYPE_VOID, 0, 0,
                                    IDAllocator::kFnAllocateIDs, _T("AllocateIDs"), 0, TYPE_INT64, 0, 1, _T("NumIDs"),
                                    0, TYPE_INT64,

                                    p_end );
}

IDAllocator::IDAllocator( boost::shared_ptr<id_generator_interface> pImpl )
    : m_pImpl( pImpl ) {}

IDAllocator::~IDAllocator() {}

FPInterfaceDesc* IDAllocator::GetDesc() { return &theIDAllocatorDesc; }

boost::shared_ptr<id_generator_interface> IDAllocator::GetImpl() { return m_pImpl; }

void IDAllocator::Reset() {
    // m_pImpl->reset();
}

IDAllocator::id_type IDAllocator::AllocateIDs( __int64 numIDs ) {
    return static_cast<id_type>( m_pImpl->allocate_ids( static_cast<std::size_t>( numIDs ) ) );
}

FPInterface* CreateIDAllocator() { return new IDAllocator( boost::make_shared<id_generator>() ); }

} // namespace max3d
} // namespace stoke

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <ifnpub.h>

namespace stoke {
namespace max3d {

template <class T>
class IObjectPtr {
    T* m_ptr;

  public:
    IObjectPtr()
        : m_ptr( NULL ) {}

    IObjectPtr( T* ptr )
        : m_ptr( ptr ) {
        if( m_ptr != NULL )
            m_ptr->AcquireIObject();
    }

    void reset() {
        if( m_ptr )
            m_ptr->ReleaseIObject();
        m_ptr = NULL;
    }

    IObjectPtr<T>& operator=( const IObjectPtr<T>& rhs ) { *this = rhs.m_ptr; }

    IObjectPtr<T>& operator=( T* ptr ) {
        if( ptr != m_ptr ) {
            if( m_ptr )
                m_ptr->ReleaseIObject();
            if( ptr )
                ptr->AcquireIObject();
            m_ptr = ptr;
        }
    }

    T& operator*() { return *m_ptr; }

    T* operator->() { return m_ptr; }

    T* get() { return m_ptr; }
};

} // namespace max3d
} // namespace stoke

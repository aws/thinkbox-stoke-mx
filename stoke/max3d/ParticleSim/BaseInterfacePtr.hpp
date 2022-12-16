// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <baseinterface.h>

namespace stoke {
namespace max3d {

template <class T>
class BaseInterfacePtr {
    T* m_ptr;

  public:
    BaseInterfacePtr()
        : m_ptr( NULL ) {}

    explicit BaseInterfacePtr( T* ptr )
        : m_ptr( NULL ) {
        if( ptr != NULL )
            ptr->AcquireInterface();

        // Assign the member variable after acquiring the interface in case AcquireInterface throws.
        m_ptr = ptr;
    }

    BaseInterfacePtr( const BaseInterfacePtr& rhs )
        : m_ptr( NULL ) {
        if( rhs.m_ptr != NULL )
            rhs.m_ptr->AcquireInterface();

        // Assign the member variable after acquiring the interface in case AcquireInterface throws.
        m_ptr = rhs.m_ptr;
    }

    ~BaseInterfacePtr() {
        if( m_ptr ) {
            T* pTemp = m_ptr;

            // Clear m_ptr before calling ReleaseInterface. This way we don't double delete if ReleaseInterface throws.
            // We are likely hooped though, if we get an exception in the destructor.
            m_ptr = NULL;

            pTemp->ReleaseInterface();
        }
    }

    void reset( T* ptr = NULL ) {
        if( ptr != m_ptr ) {
            if( m_ptr ) {
                T* pTemp = m_ptr;

                // Clear m_ptr before calling ReleaseInterface. This way we don't double delete if ReleaseInterface
                // throws.
                m_ptr = NULL;

                pTemp->ReleaseInterface();
            }

            if( ptr )
                ptr->AcquireInterface();

            m_ptr = ptr;
        }
    }

    BaseInterfacePtr<T>& operator=( const BaseInterfacePtr<T>& rhs ) {
        reset( rhs.m_ptr );
        return *this;
    }

    BaseInterfacePtr<T>& operator=( T* ptr ) {
        reset( ptr );
        return *this;
    }

    T& operator*() const { return *m_ptr; }

    T* operator->() const { return m_ptr; }

    T* get() const { return m_ptr; }

    operator bool() const { return m_ptr != NULL; }
};

} // namespace max3d
} // namespace stoke

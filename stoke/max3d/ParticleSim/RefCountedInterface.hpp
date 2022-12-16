// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <ifnpub.h>

/**
 * This class will mix in a reference count into a class heirarchy rooted with BaseInterface.
 * @tparam Parent The class to inherit from. Must be a child class of BaseInterface.
 */
template <class Parent>
class RefCountedInterface : public Parent {
  public:
    RefCountedInterface();

    virtual ~RefCountedInterface();

    // From InterfaceServer
    virtual BaseInterface* GetInterface( Interface_ID id );

    // From BaseInterface
    virtual BaseInterface::LifetimeType LifetimeControl();

    virtual BaseInterface* AcquireInterface();

    virtual void ReleaseInterface();

    virtual void DeleteInterface();

  private:
    volatile long m_refCount;
};

template <class Parent>
inline RefCountedInterface<Parent>::RefCountedInterface()
    : m_refCount( 0 ) {}

template <class Parent>
inline RefCountedInterface<Parent>::~RefCountedInterface() {}

template <class Parent>
inline BaseInterface* RefCountedInterface<Parent>::GetInterface( Interface_ID id ) {
    if( Parent::GetID() == id )
        return static_cast<Parent*>( this );
    return Parent::GetInterface( id );
}

template <class Parent>
inline BaseInterface::LifetimeType RefCountedInterface<Parent>::LifetimeControl() {
    return BaseInterface::wantsRelease;
}

template <class Parent>
inline BaseInterface* RefCountedInterface<Parent>::AcquireInterface() {
    InterlockedIncrement( &m_refCount );

    return this;
}

template <class Parent>
inline void RefCountedInterface<Parent>::ReleaseInterface() {
    if( InterlockedDecrement( &m_refCount ) == 0 )
        delete this;
}

template <class Parent>
inline void RefCountedInterface<Parent>::DeleteInterface() {
    // Do nothing
}

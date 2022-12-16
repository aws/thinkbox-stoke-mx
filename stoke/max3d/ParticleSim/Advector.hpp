// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "IAdvector.hpp"
#include "RefCountedInterface.hpp"

namespace stoke {
namespace max3d {

class Advector : public RefCountedInterface<IAdvector> {
  public:
    Advector( advector_interface_ptr pImpl );

    virtual ~Advector();

    virtual advector_interface_ptr GetImpl();

    virtual Point3 AdvectParticle( const Point3& p, const Point3& v, IReflowField* pVelocityField,
                                   float timeStepSeconds );

    virtual Point3 GetOffset( const Point3& p, const Point3& v, IReflowField* pVelocityField, float timeStepSeconds );

  private:
    advector_interface_ptr m_impl;
};

} // namespace max3d
} // namespace stoke

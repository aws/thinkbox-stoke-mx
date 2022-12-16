// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "IReflowField.hpp"
#include "RefCountedInterface.hpp"

#include <frantic/channels/channel_map.hpp>
#include <frantic/volumetrics/field_interface.hpp>

#include <boost/shared_ptr.hpp>

namespace stoke {
namespace max3d {

class ReflowField : public RefCountedInterface<IReflowField> {
  public:
    ReflowField( field_interface_ptr pImpl );

    virtual ~ReflowField();

    // From IReflowField
    virtual field_interface_ptr GetImpl();

    virtual float GetVelocityScale();

    virtual void SetVelocityScale( float newScale );

    // virtual void ResetSimulation();

    // virtual void AdvanceSimulation();

    virtual void Update( TimeValue t );

    virtual Point3 EvaluateVelocity( const Point3& p );

  private:
    field_interface_ptr m_impl;
};

} // namespace max3d
} // namespace stoke

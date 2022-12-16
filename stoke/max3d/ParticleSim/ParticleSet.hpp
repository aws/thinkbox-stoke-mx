// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "IParticleSet.hpp"
#include "RefCountedInterface.hpp"

#include <stoke/particle_set_interface.hpp>

#include <vector>

namespace stoke {
namespace max3d {

FPInterface* CreateParticleSet( const Tab<const MCHAR*>* extraChannels );

class ParticleSet : public RefCountedInterface<IParticleSet> {
  public:
    ParticleSet( particle_set_interface_ptr pImpl );

    virtual ~ParticleSet();

    void reset( particle_set_interface_ptr newImpl );

    // From IParticleSet
    virtual particle_set_interface_ptr GetImpl();

    virtual index_type GetCount();

    virtual Point3 GetPosition( index_type i );

    virtual Point3 GetVelocity( index_type i );

    virtual float GetAge( index_type i );

    virtual float GetLifespan( index_type i );

    virtual id_type GetParticleID( index_type i );

    virtual Point3 GetAdvector( index_type i );

    virtual void SetPosition( index_type i, const Point3& p );

    virtual void SetVelocity( index_type i, const Point3& v );

    virtual void SetAge( index_type i, float age );

    virtual void SetLifespan( index_type i, float lifespan );

    virtual void SetAdvectionOffset( index_type i, const Point3& advectionOffset );

    virtual void DeleteDeadParticles();

    // virtual IParticleSet* Clone();

  private:
    particle_set_interface_ptr m_impl;
};

} // namespace max3d
} // namespace stoke

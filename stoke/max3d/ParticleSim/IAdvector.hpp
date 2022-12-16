// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/advector_interface.hpp>

#include <ifnpub.h>

#define Advector_INTERFACE Interface_ID( 0x4d7062d4, 0x7d912514 )

namespace stoke {
namespace max3d {

class IParticleSet;
class IReflowField;

/**
 * This class encapsulates an advection scheme. This is an application of the strategy pattern such that different
 * advection algorithms can be chosen at runtime.
 */
class IAdvector : public FPMixinInterface {
  public:
    virtual ~IAdvector();

    virtual FPInterfaceDesc* GetDesc();

    virtual advector_interface_ptr GetImpl() = 0;

    /**
     * Solves for the new position of a massless particle that is affected by a velocity field.
     * @param p The position of the particle before advection.
     * @param v The velocity of the particle before advection.
     * @param pVelocityField The velocity field that is affecting the particle.
     * @param timeStepSeconds The amount of time to allow the particle to move through the velocity field.
     * @return The new position of the particle.
     */
    virtual Point3 AdvectParticle( const Point3& p, const Point3& v, IReflowField* pVelocityField,
                                   float timeStepSeconds ) = 0;

    /**
     * Returns the offset the particle would have from it's current position had it been advected.
     * @param p The position of the particle before advection.
     * @param v The velocity of the particle before advection.
     * @param pVelocityField The velocity field that is affecting the particle.
     * @param timeStepSeconds The amount of time to allow the particle to move through the velocity field.
     * @return The offset.
     */
    virtual Point3 GetOffset( const Point3& p, const Point3& v, IReflowField* pVelocityField,
                              float timeStepSeconds ) = 0;

  private:
    virtual Point3 AdvectParticleMXS( const Point3& p, const Point3& v, FPInterface* pVelocityField,
                                      float timeStepSeconds );

  public:
    enum {
        kFnAdvectParticle,
    };

#pragma warning( push, 3 )
#pragma warning( disable : 4238 )
    BEGIN_FUNCTION_MAP
    FN_4( kFnAdvectParticle, TYPE_POINT3_BV, AdvectParticleMXS, TYPE_POINT3, TYPE_POINT3, TYPE_INTERFACE, TYPE_FLOAT )
    END_FUNCTION_MAP
#pragma warning( pop )
};

} // namespace max3d
} // namespace stoke

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "BaseInterfacePtr.hpp"
#include "IParticleGenerator.hpp"
#include "IReflowField.hpp"
#include "RefCountedInterface.hpp"

#include <stoke/particle_generator_interface.hpp>

namespace stoke {
namespace max3d {

class ParticleGenerator : public RefCountedInterface<IParticleGenerator> {
  public:
    ParticleGenerator( particle_generator_interface_ptr pImpl );

    virtual ~ParticleGenerator();

    // From IParticleGenerator
    virtual particle_generator_interface_ptr GetImpl();

    virtual int GetRandomSeed();

    virtual void SetRandomSeed( int seed );

    virtual __int64 GetGeneratorRate();

    virtual void SetGeneratorRate( __int64 numPerFrame );

    virtual Point2 GetInitialLifespan();

    virtual void SetInitialLifespan( const Point2& lifespan );

    virtual float GetDiffusionConstant() const;

    virtual void SetDiffusionConstant( float kDiffusion );

    virtual IReflowField* GetInitialVelocityField();

    virtual void SetInitialVelocityField( IReflowField* pVelocityField );

    virtual IDAllocator* GetIDAllocator();

    virtual void SetIDAllocator( IDAllocator* pAllocator );

    virtual Tab<MSTR*> GetExtraChannels( TimeValue t );

    // virtual void ResetSimulation();

    virtual void Update( TimeValue updateTime );

    virtual void GenerateNextParticles( IParticleSet* pParticleSet, float timeStepSeconds );

    virtual void GenerateNextParticlesWithMagma( IParticleSet* pParticleSet, float timeStepSeconds,
                                                 frantic::magma::max3d::IMagmaHolder* pMagmaHolder,
                                                 frantic::magma::max3d::IErrorReporter2* pErrorReporter, TimeValue t );

  private:
    particle_generator_interface_ptr m_impl;

    BaseInterfacePtr<IReflowField> m_initialVelocityField;
    BaseInterfacePtr<IDAllocator> m_idAllocator;

    std::vector<MSTR> m_cachedStrings;
};

} // namespace max3d
} // namespace stoke

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/particle_generator_interface.hpp>

#include <ifnpub.h>

#include <frantic/magma/max3d/IErrorReporter.hpp>
#include <frantic/magma/max3d/IMagmaHolder.hpp>

#include "IParticleSet.hpp"

#define ParticleGenerator_INTERFACE Interface_ID( 0x76914dad, 0x11c413ec )

namespace stoke {
namespace max3d {

class IReflowField;
class IDAllocator;

class IParticleGenerator : public FPMixinInterface {
  public:
    virtual ~IParticleGenerator();

    virtual FPInterfaceDesc* GetDesc();

    virtual int GetRandomSeed() = 0;

    virtual void SetRandomSeed( int seed ) = 0;

    virtual particle_generator_interface_ptr GetImpl() = 0;

    virtual __int64 GetGeneratorRate() = 0;

    virtual void SetGeneratorRate( __int64 numPerFrame ) = 0;

    virtual Point2 GetInitialLifespan() = 0;

    virtual void SetInitialLifespan( const Point2& lifespan ) = 0;

    virtual float GetDiffusionConstant() const = 0;

    virtual void SetDiffusionConstant( float kDiffusion ) = 0;

    virtual IReflowField* GetInitialVelocityField() = 0;

    virtual void SetInitialVelocityField( IReflowField* pVelocityField ) = 0;

    virtual IDAllocator* GetIDAllocator() = 0;

    virtual void SetIDAllocator( IDAllocator* pAllocator ) = 0;

    virtual Tab<MSTR*> GetExtraChannels( TimeValue t ) = 0;

    // virtual void ResetSimulation() = 0;
    virtual void Update( TimeValue updateTime ) = 0;

    virtual void GenerateNextParticles( IParticleSet* pParticleSet, float timeStepSeconds ) = 0;

    virtual void GenerateNextParticlesWithMagma( IParticleSet* pParticleSet, float timeStepSeconds,
                                                 frantic::magma::max3d::IMagmaHolder* pMagmaHolder,
                                                 frantic::magma::max3d::IErrorReporter2* pErrorReporter,
                                                 TimeValue t ) = 0;

    // virtual void GenerateNextParticles( IParticleSet* pParticleSet ) = 0;

  private:
    FPInterface* GetInitialVelocityFieldMXS();

    void SetInitialVelocityFieldMXS( FPInterface* pVelocityField );

    FPInterface* GetIDAllocatorMXS();

    void SetIDAllocatorMXS( FPInterface* pAllocator );

    void GenerateNextParticlesMXS( FPInterface* pParticleSet, float timeStepSeconds );

    void GenerateNextParticlesWithMagmaMXS( FPInterface* pParticleSet, float timeStepSeconds,
                                            ReferenceTarget* pMagmaHolder, FPInterface* pErrorReporter, TimeValue t );

  public:
    enum {
        // kFnResetSimulation,
        kFnGenerateNextParticles,
        kFnGetRandomSeed,
        kFnSetRandomSeed,
        kFnGetGeneratorRate,
        kFnSetGeneratorRate,
        kFnGetInitialVelocityField,
        kFnSetInitialVelocityField,
        kFnGetIDAllocator,
        kFnSetIDAllocator,
        kFnGetInitialLifespan,
        kFnSetInitialLifespan,
        kFnGetExtraChannels,
        kFnGetDiffusionConstant,
        kFnSetDiffusionConstant,
        kFnUpdate,
        kFnGenerateNextParticlesWithMagma
    };

  protected:
#pragma warning( push, 3 )
#pragma warning( disable : 4238 )
    BEGIN_FUNCTION_MAP
    PROP_FNS( kFnGetRandomSeed, GetRandomSeed, kFnSetRandomSeed, SetRandomSeed, TYPE_INT )
    PROP_FNS( kFnGetGeneratorRate, GetGeneratorRate, kFnSetGeneratorRate, SetGeneratorRate, TYPE_INT64 )
    PROP_FNS( kFnGetInitialVelocityField, GetInitialVelocityFieldMXS, kFnSetInitialVelocityField,
              SetInitialVelocityFieldMXS, TYPE_INTERFACE )
    PROP_FNS( kFnGetIDAllocator, GetIDAllocatorMXS, kFnSetIDAllocator, SetIDAllocatorMXS, TYPE_INTERFACE )
    PROP_FNS( kFnGetInitialLifespan, GetInitialLifespan, kFnSetInitialLifespan, SetInitialLifespan, TYPE_POINT2_BV )
    PROP_FNS( kFnGetDiffusionConstant, GetDiffusionConstant, kFnSetDiffusionConstant, SetDiffusionConstant, TYPE_FLOAT )
    RO_PROP_TFN( kFnGetExtraChannels, GetExtraChannels, TYPE_TSTR_TAB_BV )
    // VFN_0( kFnResetSimulation, ResetSimulation )
    VFN_1( kFnUpdate, Update, TYPE_TIMEVALUE )
    VFN_2( kFnGenerateNextParticles, GenerateNextParticlesMXS, TYPE_INTERFACE, TYPE_FLOAT )
    VFN_5( kFnGenerateNextParticlesWithMagma, GenerateNextParticlesWithMagmaMXS, TYPE_INTERFACE, TYPE_FLOAT,
           TYPE_REFTARG, TYPE_INTERFACE, TYPE_TIMEVALUE )
    END_FUNCTION_MAP
#pragma warning( pop )
};

} // namespace max3d
} // namespace stoke

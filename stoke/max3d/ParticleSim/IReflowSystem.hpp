// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <ifnpub.h>

#include "IAdvector.hpp"
#include "IParticleGenerator.hpp"
#include "IParticleSet.hpp"
#include "IReflowField.hpp"

#define ReflowSystem_INTERFACE Interface_ID( 0x2742626c, 0x7563008c )

namespace stoke {
namespace max3d {

class IReflowSystem : public FPMixinInterface {
  public:
    virtual ~IReflowSystem();

    virtual FPInterfaceDesc* GetDesc();

    virtual void SetAdvector( IAdvector* pAdvector ) = 0;

    virtual void SetParticleGenerator( IParticleGenerator* pGenerator ) = 0;

    virtual void SetReflowField( IReflowField* pField ) = 0;

    virtual IParticleSet* GetResultSet() = 0;

    virtual void ResetSimulation() = 0;

    virtual void AdvanceSimulation() = 0;

    virtual int GetRandomSeed() = 0;

    virtual void SetRandomSeed( int seed ) = 0;

  public:
    virtual void SetAdvectorMXS( FPInterface* pAdvectorObject );
    virtual void SetParticleGeneratorMXS( FPInterface* pGeneratorObject );
    virtual void SetReflowFieldMXS( FPInterface* pGeneratorObject );

  public:
    enum {
        kFnSetAdvector,
        kFnSetParticleGenerator,
        kFnSetReflowField,
        kFnGetResultSet,
        kFnResetSimulation,
        kFnAdvanceSimulation,
        kFnSetRandomSeed,
        kFnGetRandomSeed,
    };

  protected:
#pragma warning( push, 3 )
#pragma warning( disable : 4238 )
    BEGIN_FUNCTION_MAP
    PROP_FNS( kFnGetRandomSeed, GetRandomSeed, kFnSetRandomSeed, SetRandomSeed, TYPE_INT )

    VFN_1( kFnSetAdvector, SetAdvectorMXS, TYPE_INTERFACE )
    VFN_1( kFnSetParticleGenerator, SetParticleGeneratorMXS, TYPE_INTERFACE )
    VFN_1( kFnSetReflowField, SetReflowFieldMXS, TYPE_INTERFACE )
    FN_0( kFnGetResultSet, TYPE_INTERFACE, GetResultSet )
    VFN_0( kFnResetSimulation, ResetSimulation )
    VFN_0( kFnAdvanceSimulation, AdvanceSimulation )
    END_FUNCTION_MAP
#pragma warning( pop )
};

} // namespace max3d
} // namespace stoke

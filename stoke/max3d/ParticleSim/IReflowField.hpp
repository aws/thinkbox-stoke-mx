// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/field_interface.hpp>

#include <ifnpub.h>
#if MAX_VERSION_MAJOR >= 25
#include <geom/point3.h>
#else
#include <point3.h>
#endif

#include <boost/shared_ptr.hpp>

#define ReflowField_INTERFACE Interface_ID( 0x144e29b6, 0x705128f7 )

namespace stoke {
namespace max3d {

class IReflowField : public FPMixinInterface {
  public:
    virtual ~IReflowField();

    virtual FPInterfaceDesc* GetDesc();

    virtual field_interface_ptr GetImpl() = 0;

    virtual float GetVelocityScale() = 0;

    virtual void SetVelocityScale( float newScale ) = 0;

    // virtual void ResetSimulation() = 0;

    // virtual void AdvanceSimulation() = 0;

    virtual void Update( TimeValue t ) = 0;

    virtual Point3 EvaluateVelocity( const Point3& p ) = 0;

  public:
    enum {
        // kFnResetSimulation,
        // kFnAdvanceSimulation,
        kFnUpdate,
        kFnEvaluateVelocity,
        kFnGetVelocityScale,
        kFnSetVelocityScale,
    };

  protected:
#pragma warning( push, 3 )
#pragma warning( disable : 4238 )
    BEGIN_FUNCTION_MAP
    PROP_FNS( kFnGetVelocityScale, GetVelocityScale, kFnSetVelocityScale, SetVelocityScale, TYPE_FLOAT )
    // VFN_0( kFnResetSimulation, ResetSimulation )
    // VFN_0( kFnAdvanceSimulation, AdvanceSimulation )
    VFN_1( kFnUpdate, Update, TYPE_TIMEVALUE )
    FN_1( kFnEvaluateVelocity, TYPE_POINT3_BV, EvaluateVelocity, TYPE_POINT3 )
    END_FUNCTION_MAP
#pragma warning( pop )
};

} // namespace max3d
} // namespace stoke

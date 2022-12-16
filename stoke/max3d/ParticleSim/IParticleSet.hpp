// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/particle_set_interface.hpp>

#include <ifnpub.h>
#if MAX_VERSION_MAJOR >= 25
#include <geom/point3.h>
#else
#include <point3.h>
#endif

#define ParticleSet_INTERFACE Interface_ID( 0x4d402e9a, 0x61c5f51 )

namespace stoke {
namespace max3d {

class IParticleSet;

/**
 * Writes an IParticleSet to disk.
 * @param particleSet The IParticleSet instance that is written to disk
 * @param szFilePath The path to write the particles to.
 */
void WriteParticleSet( IParticleSet* particleSet, const MCHAR* szFilePath );

/**
 * Writes an IParticleSet to disk.
 * @param particleSet The IParticleSet instance, cast as a FPInterface*, that is written to disk
 * @param szFilePath The path to write the particles to.
 */
void WriteParticleSetMXS( FPInterface* particleSet, const MCHAR* szFilePath );

/**
 * Creates a new IParticleSet, loading the particle data from a file.
 * @param szFilePath The path to the particle data.
 * @return A new IParticleSet subclass instance with the file's particle data. Will be NULL if the file doesn't exist,
 * or isn't valid particle data.
 */
IParticleSet* ReadParticleSet( const MCHAR* szFilePath );

class IParticleSet : public FPMixinInterface {
  public:
    typedef __int64 index_type;
    typedef __int64 id_type;

  public:
    virtual ~IParticleSet();

    virtual void reset( particle_set_interface_ptr newImpl ) = 0;

    virtual FPInterfaceDesc* GetDesc();

    virtual particle_set_interface_ptr GetImpl() = 0;

    virtual index_type GetCount() = 0;

    virtual Point3 GetPosition( index_type i ) = 0;

    virtual Point3 GetVelocity( index_type i ) = 0;

    virtual float GetAge( index_type i ) = 0;

    virtual float GetLifespan( index_type i ) = 0;

    // Can't use GetID because that conflicts with FPMixinInterface::GetID()
    virtual id_type GetParticleID( index_type i ) = 0;

    virtual Point3 GetAdvector( index_type i ) = 0;

    virtual void SetPosition( index_type i, const Point3& p ) = 0;

    virtual void SetVelocity( index_type i, const Point3& v ) = 0;

    virtual void SetAge( index_type i, float age ) = 0;

    virtual void SetLifespan( index_type i, float lifespan ) = 0;

    virtual void DeleteDeadParticles() = 0;

    virtual void SetAdvectionOffset( index_type i, const Point3& advectionOffset ) = 0;

    // virtual IParticleSet* Clone() = 0;

  public:
    enum {
        kFnGetCount,
        kFnGetPosition,
        kFnGetVelocity,
        kFnGetAge,
        kFnGetLifespan,
        kFnGetID,
        kFnSetPosition,
        kFnSetVelocity,
        kFnSetAge,
        kFnSetLifespan,
        kFnDeleteDeadParticles,
        // kFnClone,
    };

  protected:
#pragma warning( push, 3 )
#pragma warning( disable : 4238 )
    BEGIN_FUNCTION_MAP
    RO_PROP_FN( kFnGetCount, GetCount, TYPE_INT64 )
    FN_1( kFnGetPosition, TYPE_POINT3_BV, GetPosition, TYPE_INT64 )
    FN_1( kFnGetVelocity, TYPE_POINT3_BV, GetVelocity, TYPE_INT64 )
    FN_1( kFnGetAge, TYPE_FLOAT, GetAge, TYPE_INT64 )
    FN_1( kFnGetLifespan, TYPE_FLOAT, GetLifespan, TYPE_INT64 )
    FN_1( kFnGetID, TYPE_INT64, GetParticleID, TYPE_INT64 )
    VFN_2( kFnSetPosition, SetPosition, TYPE_INT64, TYPE_POINT3 )
    VFN_2( kFnSetVelocity, SetVelocity, TYPE_INT64, TYPE_POINT3 )
    VFN_2( kFnSetAge, SetAge, TYPE_INT64, TYPE_FLOAT )
    VFN_2( kFnSetLifespan, SetLifespan, TYPE_INT64, TYPE_FLOAT )
    VFN_0( kFnDeleteDeadParticles, DeleteDeadParticles )
    // FN_0( kFnClone, TYPE_INTERFACE, Clone )
    END_FUNCTION_MAP
#pragma warning( pop )
};

} // namespace max3d
} // namespace stoke

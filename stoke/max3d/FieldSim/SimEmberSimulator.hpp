// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/max3d/fnpublish/StandaloneInterface.hpp>

#include <ember/concatenated_field.hpp>
#include <ember/staggered_grid.hpp>

#include <frantic/diagnostics/profiling_section.hpp>

#undef min
#undef max
#undef X_AXIS
#undef Y_AXIS
#undef Z_AXIS

#pragma warning( push, 3 )
#pragma warning( disable : 4800 4244 4146 4267 4355 4503 )
#include <openvdb/math/Coord.h>
#include <openvdb/openvdb.h>
#pragma warning( pop )

#if MAX_VERSION_MAJOR >= 25
#include <geom/box3.h>
#else
#include <box3.h>
#endif

#undef min
#undef max
#undef X_AXIS
#undef Y_AXIS
#undef Z_AXIS

#define SimEmberSimulator_INTERFACE Interface_ID( 0x53373ddf, 0x274237ba )

namespace ember {
namespace max3d {

class advector_interface {
  public:
    virtual ~advector_interface() {}

    virtual openvdb::GridBase::Ptr advect( const frantic::volumetrics::field_interface& field,
                                           const frantic::volumetrics::field_interface& velocity, float timeStep,
                                           const frantic::graphics::boundbox3f& wsBounds, float spacing ) = 0;

    virtual boost::shared_ptr<frantic::volumetrics::field_interface>
    apply_advection_decorator( const boost::shared_ptr<frantic::volumetrics::field_interface>& pField,
                               const boost::shared_ptr<frantic::volumetrics::field_interface>& pVelocity,
                               float timeStep ) = 0;
};

class solver_interface {
  public:
    virtual ~solver_interface() {}

    virtual boost::shared_ptr<frantic::volumetrics::field_interface>
    solve( const frantic::volumetrics::field_interface& pVelocity, float timeStep,
           const frantic::graphics::boundbox3f& wsBounds, float spacing ) = 0;
};

// Encapsulates a collection of existing fields (ie. the current state).
class SimEmberSimulator : public frantic::max3d::fnpublish::StandaloneInterface<SimEmberSimulator> {
  public:
    SimEmberSimulator();

    virtual ~SimEmberSimulator();

    void SetSimulationRange( int startFrame, int endFrame, int numSteps = 1 );

    void SetInitMagmaExpression( ReferenceTarget* expr );

    void SetUpdateMagmaExpression( ReferenceTarget* expr );

    void SetGridParameters( const Point3& pmin, const Point3& pmax, float spacing );

    void SetContextValue( const MCHAR* szKeyName, const FPValue& val );

    void SetSolverEnabled( bool enabled = true );

    void SetErrorCallback( Value* callbackFn );

    frantic::max3d::fnpublish::TimeParameter GetTime();

    Interval GetSimulationInterval();

    Tab<const MCHAR*> GetCurrentFieldNames();

    int GetTotalAdvectTime();
    int GetTotalUpdateTime();
    int GetTotalDiscretizeTime();
    int GetTotalSolverTime();

    // Returns false if interrupted by the callback.
    bool Simulate( Value* callback = NULL );

    std::size_t get_field_count();

    openvdb::GridBase::ConstPtr get_grid( std::size_t i );

    openvdb::GridBase::ConstPtr get_grid( const std::string& name );

    boost::shared_ptr<staggered_discretized_field> get_velocity();

    boost::shared_ptr<concatenated_field> get_field_collection();

    Box3 get_worldspace_bounds() const;
    float get_spacing() const;

  protected:
    virtual ThisInterfaceDesc* GetDesc();

  private:
    template <class T>
    void set_field_constant( const std::string& name, const T& val );

    void set_field_constant( const std::string& name, frantic::channels::data_type_t, std::size_t arity,
                             const void* val );

    void do_step( TimeValue currentTime, double timeStepSeconds );

    void do_init();

  private:
    openvdb::GridPtrVec m_fields;
    std::map<std::string, std::size_t> m_fieldMap;
    std::map<frantic::tstring, FPValue> m_contextData;

    boost::shared_ptr<staggered_discretized_field>
        m_velocityField; // Separate because we are discretizing to a dense, staggered grid for velocity.

    boost::shared_ptr<advector_interface> m_advector;
    boost::shared_ptr<solver_interface> m_solver;

    int m_startFrame, m_endFrame;
    int m_simTime;
    int m_numSubsteps;

    frantic::diagnostics::profiling_section m_psAdvect, m_psUpdate, m_psDiscretize, m_psSolver;

    openvdb::CoordBBox m_simBounds;
    openvdb::math::Transform::Ptr m_transform;

    AnimHandle m_initMagma, m_updateMagma;

    std::vector<MSTR> m_cachedFieldNames;

    boost::shared_ptr<Value> m_errorCallback;
};

inline Box3 SimEmberSimulator::get_worldspace_bounds() const {
    Box3 result;

    if( m_transform ) {
        openvdb::Vec3d pmin = m_transform->indexToWorld( m_simBounds.min() );
        openvdb::Vec3d pmax = m_transform->indexToWorld( m_simBounds.max() );

        result.pmin.Set( static_cast<float>( pmin.x() ), static_cast<float>( pmin.y() ),
                         static_cast<float>( pmin.z() ) );
        result.pmax.Set( static_cast<float>( pmax.x() ), static_cast<float>( pmax.y() ),
                         static_cast<float>( pmax.z() ) );
    }

    return result;
}

inline float SimEmberSimulator::get_spacing() const {
    return m_transform ? static_cast<float>( m_transform->voxelSize()[0] ) : 0.f;
}

inline SimEmberSimulator* GetSimEmberSimulatorInterface( InterfaceServer* server ) {
    return static_cast<SimEmberSimulator*>( server->GetInterface( SimEmberSimulator_INTERFACE ) );
}

} // namespace max3d
} // namespace ember

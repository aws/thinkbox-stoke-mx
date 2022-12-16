// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

//#include <stoke/max3d/MXSField.hpp>
#include <stoke/max3d/FieldSim/SimEmberHolder.hpp>
#include <stoke/max3d/FieldSim/SimEmberSimulator.hpp>
#include <stoke/max3d/Magma/SimInfoNode.hpp>

#include <ember/advection.hpp>
#include <ember/concatenated_field.hpp>
#include <ember/data_types.hpp>
#include <ember/ember_compiler.hpp>
#include <ember/openvdb.hpp>

#pragma warning( disable : 4503 ) // "decorated name length exceeded, name was truncated" casued by OpenVDB template
                                  // instantiations.

#include <frantic/magma/max3d/IErrorReporter.hpp>
#include <frantic/magma/max3d/IMagmaHolder.hpp>

#include <frantic/max3d/GenericReferenceTarget.hpp>
#include <frantic/max3d/fnpublish/MixinInterface.hpp>
#include <frantic/max3d/fnpublish/StandaloneInterface.hpp>
#include <frantic/max3d/fnpublish/StaticInterface.hpp>
#include <frantic/max3d/paramblock_builder.hpp>
#include <frantic/max3d/rendering/renderplugin_utils.hpp>
#include <frantic/max3d/time.hpp>

#include <frantic/magma/magma_interface.hpp>
#include <frantic/magma/nodes/magma_output_node.hpp>

#include <frantic/strings/tstring.hpp>

#include <boost/intrusive_ptr.hpp>

#undef NDEBUG
#include <cassert>

#define SimEmberOwner_CLASSID Class_ID( 0x40414375, 0x3f130a79 )
#define SimEmberOwner_NAME "SimEmberOwner"
#define SimEmberOwner_VERSION 1

#define EmberHolder_CLASS Class_ID( 0x2b493510, 0x5134620d )

#define SimEmberContext_INTERFACE Interface_ID( 0x31027386, 0x6d8d31ec )
#define SimEmberOwner_INTERFACE Interface_ID( 0x44007f5c, 0x7f635129 )
#define SimEmberFactory_INTERFACE Interface_ID( 0x565c1373, 0x4db901f8 )
#define SimEmberAdvector_INTERFACE Interface_ID( 0x4c970557, 0x5c92011e )
#define SimEmberSolver_INTERFACE Interface_ID( 0x79353a37, 0x2a1f7ecf )

inline void intrusive_ptr_add_ref( BaseInterface* i ) {
    if( i )
        i->AcquireInterface();
}

inline void intrusive_ptr_release( BaseInterface* i ) {
    if( i )
        i->ReleaseInterface();
}

#define SimEmberHolder_CLASSID Class_ID( 0x1e12881, 0x546d3634 )
#define SimEmberHolder_NAME "SimEmberHolder"

namespace ember {
namespace max3d {

class SimEmberHolderDesc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL loading ) { return new SimEmberHolder( loading ); }
    const TCHAR* ClassName() { return _T( SimEmberHolder_NAME ); }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return SimEmberHolder_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() {
        return _T( SimEmberHolder_NAME );
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( SimEmberHolder_NAME ); }
#endif
};

ClassDesc2* GetSimEmberHolderDesc() {
    static SimEmberHolderDesc theSimEmberHolderDesc;
    return &theSimEmberHolderDesc;
}

SimEmberHolder::SimEmberHolderMagmaSingleton::SimEmberHolderMagmaSingleton() {
    this->define_node_type<ember::max3d::SimInfoNode>();
}

frantic::magma::magma_singleton& SimEmberHolder::SimEmberHolderMagmaSingleton::get_instance() {
    static SimEmberHolderMagmaSingleton theEmberHolderMagmaSingleton;
    return theEmberHolderMagmaSingleton;
}

SimEmberHolder::SimEmberHolder( BOOL /*loading*/ )
    : EmberHolder( EMBER_SIM_NODE_SET ) {
    GetSimEmberHolderDesc()->MakeAutoParamBlocks( this );
}

ClassDesc2* SimEmberHolder::GetClassDesc() { return GetSimEmberHolderDesc(); }

class SimEmberHolderMagmaSingleton : public EmberHolderMagmaSingleton {
  public:
    static frantic::magma::magma_singleton& get_instance() {
        static SimEmberHolderMagmaSingleton theSimEmberHolderMagmaSingleton;
        return theSimEmberHolderMagmaSingleton;
    }

    SimEmberHolderMagmaSingleton() { this->define_node_type<ember::max3d::SimInfoNode>(); }
};

std::unique_ptr<frantic::magma::magma_interface> CreateSimEmberMagmaInterface() {
    return SimEmberHolderMagmaSingleton::get_instance().create_magma_instance();
}

class SimEmberMXSContext : public frantic::max3d::fnpublish::StandaloneInterface<SimEmberMXSContext> {
  public:
    SimEmberMXSContext() {}

    virtual ~SimEmberMXSContext() {}

    void SetValue( const MCHAR* szName, const FPValue& value );

    const FPValue& GetValue( const MCHAR* szName );

    bool HasValue( const MCHAR* szName );

    template <class T>
    T get( const frantic::tstring& name );

    virtual ThisInterfaceDesc* GetDesc();

  private:
    std::map<frantic::tstring, FPValue> m_values;
};

void SimEmberMXSContext::SetValue( const MCHAR* szName, const FPValue& value ) { m_values[szName] = value; }

const FPValue& SimEmberMXSContext::GetValue( const MCHAR* szName ) {
    std::map<frantic::tstring, FPValue>::const_iterator it = m_values.find( szName );
    if( it != m_values.end() )
        return it->second;

    static FPValue emptyResult;
    return emptyResult;
}

bool SimEmberMXSContext::HasValue( const MCHAR* szName ) { return m_values.find( szName ) != m_values.end(); }

namespace {
template <class T>
struct FPValueTraits;

#define FPValueTraitsDef( type, typeCode )                                                                             \
    template <>                                                                                                        \
    struct FPValueTraits<type> {                                                                                       \
        inline static type get( const FPValue& v ) { return typeCode##_FIELD( v ); }                                   \
    }
#define FPValueTraitsPtrDef( type, typeCode )                                                                          \
    template <>                                                                                                        \
    struct FPValueTraits<type> {                                                                                       \
        inline static const type& get( const FPValue& v ) { return typeCode##_FIELD( v ); }                            \
    }

FPValueTraitsDef( int, TYPE_INT );
FPValueTraitsDef( float, TYPE_FLOAT );
FPValueTraitsDef( __int64, TYPE_INT64 );
FPValueTraitsDef( double, TYPE_DOUBLE );
FPValueTraitsDef( bool, TYPE_bool );
FPValueTraitsDef( TYPE_STRING_TYPE, TYPE_STRING );
FPValueTraitsDef( ReferenceTarget*, TYPE_REFTARG );
FPValueTraitsDef( INode*, TYPE_INODE );
FPValueTraitsDef( FPInterface*, TYPE_INTERFACE );
FPValueTraitsDef( IObject*, TYPE_IOBJECT );
FPValueTraitsDef( Value*, TYPE_VALUE );

FPValueTraitsPtrDef( Point2, TYPE_POINT2 );
FPValueTraitsPtrDef( Point3, TYPE_POINT3 );
FPValueTraitsPtrDef( Point4, TYPE_POINT4 );
FPValueTraitsPtrDef( Quat, TYPE_QUAT );
FPValueTraitsPtrDef( MSTR, TYPE_TSTR );

#undef FPValueTraitsPtrDef
#undef FPValueTraitsDef
} // namespace

template <class T>
T SimEmberMXSContext::get( const frantic::tstring& name ) {
    std::map<frantic::tstring, FPValue>::const_iterator it = m_values.find( name );
    if( it != m_values.end() )
        return FPValueTraits<T>::get( it->second );

    throw std::out_of_range( "Property not found" );
}

SimEmberMXSContext::ThisInterfaceDesc* SimEmberMXSContext::GetDesc() {
    static ThisInterfaceDesc theDesc( SimEmberContext_INTERFACE, _T("SimEmberContext"), 0 );

    if( theDesc.empty() ) {
        theDesc.function( _T("SetValue"), &SimEmberMXSContext::SetValue ).param( _T("Name") ).param( _T("Value") );

        theDesc.function( _T("GetValue"), &SimEmberMXSContext::GetValue ).param( _T("Name") );

        theDesc.function( _T("HasValue"), &SimEmberMXSContext::HasValue ).param( _T("Name") );
    }

    return &theDesc;
}

inline SimEmberMXSContext* GetSimEmberContextInterface( InterfaceServer* pServer ) {
    return pServer ? static_cast<SimEmberMXSContext*>( pServer->GetInterface( SimEmberContext_INTERFACE ) ) : NULL;
}

class rk2_advector : public advector_interface {
  public:
    virtual ~rk2_advector() {}

    template <class GridType>
    void advect_impl( GridType& outGrid, const frantic::volumetrics::field_interface& field,
                      const frantic::volumetrics::field_interface& velocity, float timeStep,
                      const openvdb::CoordBBox& bbox ) {
        char* buffer = static_cast<char*>( alloca( field.get_channel_map().structure_size() ) );

        typename GridType::Accessor accessor = outGrid.getAccessor();

        openvdb::Coord ijk;
        for( ; ijk.x() <= bbox.max().x(); ++ijk.x() ) {
            ijk.y() = bbox.min().y();
            for( ; ijk.y() <= bbox.max().y(); ++ijk.y() ) {
                ijk.z() = bbox.min().z();
                for( ; ijk.z() <= bbox.max().z(); ++ijk.z() ) {
                    openvdb::math::Vec3d p = outGrid.transform().indexToWorld( ijk );
                    openvdb::math::Vec3s v1, v2;

                    // This is an RK2 advection step.
                    velocity.evaluate_field(
                        v1.asPointer(),
                        vec3( static_cast<float>( p.x() ), static_cast<float>( p.y() ), static_cast<float>( p.z() ) ) );

                    openvdb::math::Vec3d p1 = p - timeStep * v1;

                    velocity.evaluate_field( v2.asPointer(),
                                             vec3( static_cast<float>( p1.x() ), static_cast<float>( p1.y() ),
                                                   static_cast<float>( p1.z() ) ) );

                    openvdb::math::Vec3d p2 = p - 0.5f * timeStep * ( v1 + v2 );

                    if( field.evaluate_field( buffer, vec3( static_cast<float>( p2.x() ), static_cast<float>( p2.y() ),
                                                            static_cast<float>( p2.z() ) ) ) )
                        accessor.setValue(
                            ijk, static_cast<typename GridType::ValueType*>( static_cast<void*>( buffer ) )[0] );
                }
            }
        }
    }

    virtual boost::shared_ptr<frantic::volumetrics::field_interface>
    apply_advection_decorator( const boost::shared_ptr<frantic::volumetrics::field_interface>& pField,
                               const boost::shared_ptr<frantic::volumetrics::field_interface>& pVelocity,
                               float timeStep ) {
        return boost::make_shared<advection_decorator>( pField, pVelocity, timeStep );
    }

    virtual openvdb::GridBase::Ptr advect( const frantic::volumetrics::field_interface& field,
                                           const frantic::volumetrics::field_interface& velocity, float timeStep,
                                           const frantic::graphics::boundbox3f& wsBounds, float spacing ) {
        // If all inputs are grids, we can optimize this process by bounding the maximum timestep and using the existing
        // topology of the grids.
        openvdb::CoordBBox bbox = ember::get_ijk_inner_bounds( wsBounds.minimum(), wsBounds.maximum(), spacing );
        openvdb::math::Transform::Ptr xform = openvdb::math::Transform::createLinearTransform( spacing );

        xform->postTranslate( openvdb::Vec3d( spacing / 2.f ) );

        openvdb::GridBase::Ptr pResult;

        if( field.get_channel_map()[0].data_type() == frantic::channels::data_type_float32 ) {
            if( field.get_channel_map()[0].arity() == 1 ) {
                openvdb::FloatGrid::Ptr pGrid = openvdb::FloatGrid::create( 0.f );

                pGrid->setTransform( xform );
                pGrid->setName( frantic::strings::to_string( field.get_channel_map()[0].name() ) );

                this->advect_impl( *pGrid, field, velocity, timeStep, bbox );

                pResult = pGrid;
            } else if( field.get_channel_map()[0].arity() == 3 ) {
                openvdb::Vec3fGrid::Ptr pGrid = openvdb::Vec3fGrid::create( openvdb::Vec3f( 0.f ) );

                pGrid->setTransform( xform );
                pGrid->setName( frantic::strings::to_string( field.get_channel_map()[0].name() ) );

                this->advect_impl( *pGrid, field, velocity, timeStep, bbox );

                pResult = pGrid;
            }
        }

        // Remove regions of empty values.
        if( pResult )
            pResult->pruneGrid();

        return pResult;
    }
};

class SimEmberAdvector : public frantic::max3d::fnpublish::StandaloneInterface<SimEmberAdvector> {
  public:
    SimEmberAdvector();

    virtual ~SimEmberAdvector();

    void SetImpl( const boost::shared_ptr<advector_interface>& pImpl );

    FPInterface* AdvectField( FPInterface* pTarget, FPInterface* pVelocity, float timeStep, bool toGrid = true );

    // From StandaloneInterface
    virtual ThisInterfaceDesc* GetDesc();

  private:
    boost::shared_ptr<advector_interface> m_pImpl;
};

SimEmberAdvector::SimEmberAdvector() {}

SimEmberAdvector::~SimEmberAdvector() {}

void SimEmberAdvector::SetImpl( const boost::shared_ptr<advector_interface>& pImpl ) { m_pImpl = pImpl; }

FPInterface* SimEmberAdvector::AdvectField( FPInterface* pTarget, FPInterface* pVelocity, float timeStep,
                                            bool toGrid ) {
    // MXSField* pTargetField = GetMXSFieldInterface( pTarget );
    // MXSField* pVelocityField = GetMXSFieldInterface( pVelocity );

    /*if( !m_pImpl || !pTargetField )
            return NULL;
    if( !pVelocityField )
            return pTargetField;

    if( toGrid ){
            frantic::graphics::boundbox3f wsBounds( frantic::max3d::from_max_t( pTargetField->GetBoundsMin() ),
    frantic::max3d::from_max_t( pTargetField->GetBoundsMax() ) ); float spacing = pTargetField->GetSpacing();

            openvdb::GridBase::Ptr pResultGrid = m_pImpl->advect( *pTargetField->get_field(),
    *pVelocityField->get_field(), timeStep, wsBounds, spacing );

            openvdb::CoordBBox activeBounds = pResultGrid->evalActiveVoxelBoundingBox();
            openvdb::Vec3d boundsMin = pResultGrid->transform().indexToWorld( activeBounds.min() );
            openvdb::Vec3d boundsMax = pResultGrid->transform().indexToWorld( activeBounds.max() );

            spacing = static_cast<float>( pResultGrid->voxelSize()[0] );
            wsBounds.set(
                    vec3( static_cast<float>( boundsMin.x() ), static_cast<float>( boundsMin.y() ), static_cast<float>(
    boundsMin.z() ) ), vec3( static_cast<float>( boundsMax.x() ), static_cast<float>( boundsMax.y() ),
    static_cast<float>( boundsMax.z() ) )
            );

            boost::shared_ptr<frantic::volumetrics::field_interface> field( ember::create_field_interface( pResultGrid
    ).release() );

            std::unique_ptr<MXSField> result( new MXSField );
            result->SetField( field, Box3( frantic::max3d::to_max_t( wsBounds.minimum() ), frantic::max3d::to_max_t(
    wsBounds.maximum() ) ), spacing ); return result.release(); }else{
            boost::shared_ptr<frantic::volumetrics::field_interface> pFieldImpl = pTargetField->get_field(),
    pVelocityFieldImpl = pVelocityField->get_field();

            if( pVelocityFieldImpl->get_channel_map().channel_count() != 1 ||
    pVelocityFieldImpl->get_channel_map()[0].data_type() != frantic::channels::data_type_float32 ||
    pVelocityFieldImpl->get_channel_map()[0].arity() != 3 ) return NULL;

            std::unique_ptr<MXSField> result( new MXSField );
            result->SetField( m_pImpl->apply_advection_decorator( pFieldImpl, pVelocityFieldImpl, timeStep ), Box3(
    pTargetField->GetBoundsMin(), pTargetField->GetBoundsMax() ), pTargetField->GetSpacing() ); return result.release();
    }*/

    return NULL;
}

SimEmberAdvector::ThisInterfaceDesc* SimEmberAdvector::GetDesc() {
    static ThisInterfaceDesc theDesc( SimEmberAdvector_INTERFACE, _T("SimEmberAdvector"), 0 );

    if( theDesc.empty() ) {
        theDesc.function( _T("AdvectField"), &SimEmberAdvector::AdvectField )
            .param( _T("TargetField") )
            .param( _T("VelocityField") )
            .param( _T("TimeStepSeconds") )
            .param( _T("ToGrid"), false );
    }

    return &theDesc;
}

class poisson_solver : public solver_interface {
  public:
    virtual ~poisson_solver() {}

    virtual boost::shared_ptr<frantic::volumetrics::field_interface>
    solve( const frantic::volumetrics::field_interface& velocity, float timeStep,
           const frantic::graphics::boundbox3f& wsBounds, float spacing ) {
        openvdb::CoordBBox bbox = ember::get_ijk_inner_bounds( wsBounds.minimum(), wsBounds.maximum(), spacing );

        int bounds[6];
        bbox.min().asXYZ( bounds[0], bounds[2], bounds[4] );
        bbox.max().asXYZ( bounds[1], bounds[3], bounds[5] );

        ++bounds[1];
        ++bounds[3];
        ++bounds[5];

        boost::shared_ptr<ember::staggered_discretized_field> result =
            boost::make_shared<ember::staggered_discretized_field>( bounds, spacing,
                                                                    velocity.get_channel_map()[0].name() );

        frantic::logging::null_progress_logger nullLogger;

        result->assign( velocity, velocity.get_channel_map()[0].name(), &nullLogger );

        ember::do_poisson_solve( result->get_grid(), timeStep, "DDDDDD", &nullLogger );

        return result;

        // openvdb::CoordBBox bbox = ember::get_ijk_inner_bounds( wsBounds.minimum(), wsBounds.maximum(), spacing );
        // openvdb::math::Transform::Ptr xform = openvdb::math::Transform::createLinearTransform( spacing );

        // xform->postTranslate( openvdb::Vec3d( spacing/2.f ) );

        // frantic::logging::null_progress_logger nullLogger;

        // int bounds[6];
        // bbox.min().asXYZ( bounds[0], bounds[2], bounds[4] );
        // bbox.max().asXYZ( bounds[1], bounds[3], bounds[5] );

        // ember::staggered_grid grid( bounds, spacing );
        // ember::assign( grid, pVelocity, pVelocity.get_channel_map()[0].name(), nullLogger );

        //// TODO: Update our solver to work on OpenVDB grids in order to save the copy to-and-from a staggered_grid.
        // ember::do_poisson_solve( grid, timeStep, "DDDDDD", NULL );

        // openvdb::Vec3fGrid::Ptr pResult = ember::copy_from_dense( grid );
        // pResult->setGridClass( openvdb::GRID_STAGGERED );
        // pResult->setVectorType( openvdb::VEC_CONTRAVARIANT_RELATIVE );
        // pResult->setName( frantic::strings::to_string( pVelocity.get_channel_map()[0].name() ) );

        // return pResult;
    }
};

class SimEmberSolver : public frantic::max3d::fnpublish::StandaloneInterface<SimEmberSolver> {
  public:
    SimEmberSolver();

    virtual ~SimEmberSolver();

    void SetImpl( const boost::shared_ptr<solver_interface>& pImpl );

    FPInterface* SolveVelocityField( FPInterface* pField, float timeStep );

    // From StandaloneInterface
    virtual ThisInterfaceDesc* GetDesc();

  private:
    boost::shared_ptr<solver_interface> m_pImpl;
};

SimEmberSolver::SimEmberSolver() {}

SimEmberSolver::~SimEmberSolver() {}

void SimEmberSolver::SetImpl( const boost::shared_ptr<solver_interface>& pImpl ) { m_pImpl = pImpl; }

FPInterface* SimEmberSolver::SolveVelocityField( FPInterface* pField, float timeStep ) {
    // MXSField* pMXSField = GetMXSFieldInterface( pField );

    // if( !pMXSField || !pMXSField->get_field() || !m_pImpl )
    //	return NULL;

    // std::unique_ptr<MXSField> result( new MXSField );
    //
    // frantic::graphics::boundbox3f wsBounds( frantic::max3d::from_max_t( pMXSField->GetBoundsMin() ),
    // frantic::max3d::from_max_t( pMXSField->GetBoundsMax() ) ); float spacing = pMXSField->GetSpacing();
    // boost::shared_ptr<frantic::volumetrics::field_interface> pFieldImpl = pMXSField->get_field();

    // if( wsBounds.is_empty() || !(spacing >= 1e-5f) || !pFieldImpl )
    //	return NULL;

    ////openvdb::Vec3fGrid::Ptr pResultField = m_pImpl->solve( *pFieldImpl, timeStep, wsBounds, spacing );

    ////result->SetField( ember::create_field_interface( pResultField ), Box3( pMXSField->GetBoundsMin(),
    ///pMXSField->GetBoundsMax() ), pMXSField->GetSpacing() );

    // boost::shared_ptr<frantic::volumetrics::field_interface> resultField = m_pImpl->solve( *pFieldImpl, timeStep,
    // wsBounds, spacing );

    // result->SetField( resultField, Box3( pMXSField->GetBoundsMin(), pMXSField->GetBoundsMax() ),
    // pMXSField->GetSpacing() );

    // return result.release();

    return NULL;
}

SimEmberSolver::ThisInterfaceDesc* SimEmberSolver::GetDesc() {
    static ThisInterfaceDesc theDesc( SimEmberSolver_INTERFACE, _T("SimEmberSolver"), 0 );

    if( theDesc.empty() ) {
        theDesc.function( _T("SolveVelocityField"), &SimEmberSolver::SolveVelocityField )
            .param( _T("VelocityField") )
            .param( _T("TimeStep") );
    }

    return &theDesc;
}

class ISimEmberOwner : public frantic::max3d::fnpublish::MixinInterface<ISimEmberOwner> {
  public:
    virtual ~ISimEmberOwner() {}

    /**
     * Retrieves the field object for the initial state of the simulator.
     * \param szFieldName Name of the field to evaluate its initial state.
     * \param pContext Client supplied instance of 'SimEmberMXSContext' that offers extra context information.
     * \param t The time to evaluate at.
     */
    virtual FPInterface* GetInitialField( const MCHAR* szFieldName, FPInterface* pContext,
                                          frantic::max3d::fnpublish::TimeWrapper t ) = 0;

    /**
     * Updates the specified field (ex. applies body forces to a velocity, or inserts new density values, etc.)
     * \param pFieldCollection The existing collection of fields prior to the update.
     * \param szFieldName The name of the field to update
     * \param pContext Client supplied instance of 'SimEmberMXSContext' that offers extra context information.
     * \param timeStep The time step of the update (in seconds).
     * \param t The time to evaluate the update at.
     */
    virtual FPInterface* GetUpdateField( FPInterface* pFieldCollection, const MCHAR* szFieldName, FPInterface* pContext,
                                         float timeStep, frantic::max3d::fnpublish::TimeWrapper t ) = 0;

    static ThisInterfaceDesc* GetStaticDesc();

    virtual ThisInterfaceDesc* GetDesc() { return ISimEmberOwner::GetStaticDesc(); }
};

ISimEmberOwner::ThisInterfaceDesc* ISimEmberOwner::GetStaticDesc() {
    static ThisInterfaceDesc theDesc( SimEmberOwner_INTERFACE, _T("SimEmberOwner"), 0 );

    if( theDesc.empty() ) {
        theDesc.function( _T("GetInitialField"), &ISimEmberOwner::GetInitialField )
            .param( _T("FieldName") )
            .param( _T("Context") );

        theDesc.function( _T("GetUpdateField"), &ISimEmberOwner::GetUpdateField )
            .param( _T("InputFields") )
            .param( _T("FieldName") )
            .param( _T("Context") )
            .param( _T("TimeStep") );
    }

    return &theDesc;
}

inline ISimEmberOwner* GetSimEmberOwnerInterface( InterfaceServer* pServer ) {
    return pServer ? static_cast<ISimEmberOwner*>( pServer->GetInterface( SimEmberOwner_INTERFACE ) ) : NULL;
}

class SimEmberOwnerDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_pbBuilder;

  public:
    SimEmberOwnerDesc();

    int IsPublic() { return FALSE; }
    void* Create( BOOL loading );
    const TCHAR* ClassName() { return _T( SimEmberOwner_NAME ); }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return SimEmberOwner_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() {
        return _T( SimEmberOwner_NAME );
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( SimEmberOwner_NAME ); }
#endif
};

enum { kInitHolder, kUpdateHolder };

SimEmberOwnerDesc::SimEmberOwnerDesc()
    : m_pbBuilder( 0, _T("Parameters"), 0, SimEmberOwner_VERSION ) {
    m_pbBuilder.OwnerClassDesc( this );
    m_pbBuilder.OwnerRefNum( 0 );

    m_pbBuilder.Parameter<ReferenceTarget*>( kInitHolder, _T("initMagmaHolder"), 0, false, P_SUBANIM );
    m_pbBuilder.Parameter<ReferenceTarget*>( kUpdateHolder, _T("magmaHolder"), 0, false, P_SUBANIM );

    ISimEmberOwner::GetStaticDesc()->SetClassDesc( this );
}

ClassDesc2* GetSimEmberOwnerDesc() {
    static SimEmberOwnerDesc theSimEmberOwnerDesc;
    return &theSimEmberOwnerDesc;
}

class SimEmberOwner : public frantic::max3d::GenericReferenceTarget<ReferenceTarget, SimEmberOwner>,
                      public ISimEmberOwner {
  public:
    SimEmberOwner( BOOL loading );

    virtual ~SimEmberOwner();

    virtual BaseInterface* GetInterface( Interface_ID id );

    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

    virtual FPInterface* GetInitialField( const MCHAR* szFieldName, FPInterface* pContext,
                                          frantic::max3d::fnpublish::TimeWrapper t );

    virtual FPInterface* GetUpdateField( FPInterface* pFieldCollection, const MCHAR* szFieldName, FPInterface* pContext,
                                         float timeStep, frantic::max3d::fnpublish::TimeWrapper t );

    // TODO: Error handling?

  protected:
    virtual ClassDesc2* GetClassDesc();
};

void* SimEmberOwnerDesc::Create( BOOL loading ) { return new SimEmberOwner( loading ); }

SimEmberOwner::SimEmberOwner( BOOL loading ) {
    if( !loading ) {
        GetSimEmberOwnerDesc()->MakeAutoParamBlocks( this );

        std::unique_ptr<EmberHolder> initHolder( new EmberHolder( EMBER_SIM_NODE_SET ) );
        std::unique_ptr<EmberHolder> updateHolder( new EmberHolder( EMBER_SIM_NODE_SET ) );

        m_pblock->SetValue( kInitHolder, 0, initHolder.release() );
        m_pblock->SetValue( kUpdateHolder, 0, updateHolder.release() );

        // m_pblock->SetValue( kInitHolder, 0, static_cast<ReferenceTarget*>( CreateInstance( REF_TARGET_CLASS_ID,
        // EmberHolder_CLASS ) ) ); m_pblock->SetValue( kUpdateHolder, 0, static_cast<ReferenceTarget*>( CreateInstance(
        // REF_TARGET_CLASS_ID, EmberHolder_CLASS ) ) );
    }
}

SimEmberOwner::~SimEmberOwner() {}

BaseInterface* SimEmberOwner::GetInterface( Interface_ID id ) {
    if( BaseInterface* result = ISimEmberOwner::GetInterface( id ) )
        return result;
    return frantic::max3d::GenericReferenceTarget<ReferenceTarget, SimEmberOwner>::GetInterface( id );
}

RefResult SimEmberOwner::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& /*partID*/,
                                           RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        if( message == REFMSG_CHANGE )
            return REF_SUCCEED;
    }
    return REF_DONTCARE;
}

namespace {
typedef frantic::volumetrics::field_interface field;
typedef boost::shared_ptr<field> field_ptr;

template <class T>
class constant_field : public field {
    frantic::channels::channel_map m_channelMap;
    T m_value;

  public:
    constant_field( const T& value, const frantic::tstring& valueName = _T("Value") )
        : m_value( value ) {
        m_channelMap.define_channel<T>( valueName );
        m_channelMap.end_channel_definition();
    }

    virtual const frantic::channels::channel_map& get_channel_map() const { return m_channelMap; }

    virtual bool evaluate_field( void* dest, const frantic::graphics::vector3f& ) const {
        *reinterpret_cast<T*>( dest ) = m_value;
        return true;
    }

    const T& get_constant() const { return m_value; }
};

inline bool is_constant_zero( field& theField ) {
    return ( typeid( theField ) == typeid( constant_field<vec3> ) )
               ? static_cast<constant_field<vec3>&>( theField ).get_constant() == vec3( 0, 0, 0 )
               : false;
}
} // namespace

namespace test {
class sim_compiler : public ember_compiler {
    std::map<frantic::tstring, field_ptr> m_channels;

  public:
    sim_compiler() {}

    sim_compiler& operator=( const movable<sim_compiler>& rhs ) {
        this->ember_compiler::operator=( boost::move( static_cast<ember_compiler&>( rhs.get() ) ) );
        this->m_channels.swap( rhs.get().m_channels );
        return *this;
    }

    void clear_channel_values();

    void set_channel_value( const frantic::tstring& channelName, field_ptr field );

    field_ptr get_channel_value( const frantic::tstring& channelName );

    virtual std::unique_ptr<expression_field> create_empty_expression_field();

    // Override these nodes to handle how we want evaluation to work.
    virtual void compile( frantic::magma::nodes::magma_input_channel_node* );

  private:
    class expression_field_impl;
};

class sim_compiler::expression_field_impl : public expression_field {
    sim_compiler m_expression;

  public:
    expression_field_impl( sim_compiler& parentExpression ) {
        m_expression.init_as_child_of( parentExpression );
        m_expression.m_channels = parentExpression.m_channels;
    }

    virtual ~expression_field_impl() {}

    virtual void add_output( frantic::magma::magma_interface::magma_id exprID, int outputIndex,
                             const frantic::tstring& outputName = _T("Value") ) {
        m_expression.build_subtree( exprID );

        const base_compiler::temporary_meta& meta = m_expression.get_value( exprID, outputIndex );

        m_expression.append_output_channel( outputName, meta.first.m_elementCount, meta.first.m_elementType );
        m_expression.compile_output( -42, std::make_pair( exprID, outputIndex ), outputName, meta.first );
        m_expression.clear_channel_values();
    }

    virtual const frantic::channels::channel_map& get_channel_map() const { return m_expression.get_output_map(); }

    virtual bool evaluate_field( void* dest, const frantic::graphics::vector3f& pos ) const {
        m_expression.eval( static_cast<char*>( dest ), pos );
        return true;
    }
};

void sim_compiler::clear_channel_values() { m_channels.clear(); }

void sim_compiler::set_channel_value( const frantic::tstring& channelName, field_ptr field ) {
    m_channels[channelName] = field;
}

sim_compiler::field_ptr sim_compiler::get_channel_value( const frantic::tstring& channelName ) {
    return m_channels[channelName];
}

std::unique_ptr<sim_compiler::expression_field> sim_compiler::create_empty_expression_field() {
    return std::unique_ptr<expression_field>( new expression_field_impl( *this ) );
}

void sim_compiler::compile( frantic::magma::nodes::magma_input_channel_node* node ) {
    if( node->get_channelName() == _T("Position") ) {
        ember_compiler::compile( node );
    } else {
        std::map<frantic::tstring, field_ptr>::iterator it = m_channels.find( node->get_channelName() );
        if( it == m_channels.end() ) {
            field_ptr defaultField;

            if( node->get_channelType() == *frantic::magma::magma_singleton::get_named_data_type( _T("Float") ) )
                defaultField.reset( new constant_field<float>( 0.f ) );
            else if( node->get_channelType() == *frantic::magma::magma_singleton::get_named_data_type( _T("Vec3") ) )
                defaultField.reset( new constant_field<vec3>( vec3() ) );
            else
                defaultField.reset( new constant_field<float>( 0.f ) );

            it = m_channels.insert( std::make_pair( node->get_channelName(), defaultField ) ).first;
        }

        this->compile_field( node->get_id(), it->second, std::make_pair( this->get_position_expression_id(), 0 ) );
    }
}
} // namespace test

namespace {
class EmberSimObjectContext : public frantic::magma::magma_compiler_interface::context_base {
  public:
    frantic::graphics::boundbox3f bounds;
    float spacing;

    RenderGlobalContext globContext;
    INode* node;

    TimeValue m_simStart;
    int m_frameRate;

    float timeStepSeconds;

  public:
    EmberSimObjectContext( TimeValue t, INode* _node = NULL, int frameRate = GetFrameRate() )
        : node( _node )
        , m_simStart( t )
        , m_frameRate( frameRate ) {
        frantic::max3d::rendering::initialize_renderglobalcontext( globContext );

        globContext.time = t;
        globContext.camToWorld.IdentityMatrix();
        globContext.worldToCam.IdentityMatrix();
    }

    virtual ~EmberSimObjectContext() {}

    virtual frantic::tstring get_name() const { return _T("EmberContext"); }

    virtual frantic::graphics::transform4f get_world_transform( bool inverse = false ) const {
        return node ? frantic::max3d::from_max_t( inverse ? Inverse( node->GetNodeTM( globContext.time ) )
                                                          : node->GetNodeTM( globContext.time ) )
                    : frantic::graphics::transform4f::identity();
    }

    virtual frantic::graphics::transform4f get_camera_transform( bool inverse = false ) const {
        return frantic::max3d::from_max_t( inverse ? globContext.camToWorld : globContext.camToWorld );
    }

    virtual boost::any get_property( const frantic::tstring& name ) const {
        if( name == _T("Time") )
            return boost::any( globContext.time );
        else if( name == _T("MaxRenderGlobalContext") )
            return boost::any(
                static_cast<const RenderGlobalContext*>( &globContext ) ); // Expected to be a const pointer.
        else if( name == _T("CurrentINode") )
            return boost::any( node );
        else if( name == _T("InWorldSpace") )
            return boost::any( node == NULL );
        else if( name == _T("Bounds") )
            return boost::any( bounds );
        else if( name == _T("Spacing") )
            return boost::any( spacing );
        else if( name == _T("TimeStep") )
            return boost::any( timeStepSeconds );
        return frantic::magma::magma_compiler_interface::context_base::get_property( name );
    }
};
} // namespace

enum { kInitialExpression, kPerStepExpression };

FPInterface* SimEmberOwner::GetInitialField( const MCHAR* szFieldName, FPInterface* pContext,
                                             frantic::max3d::fnpublish::TimeWrapper t ) {
    /*if( frantic::magma::max3d::IMagmaHolder* holder = frantic::magma::max3d::GetMagmaHolderInterface(
    m_pblock->GetReferenceTarget( kInitHolder ) ) ){ boost::shared_ptr<frantic::magma::magma_interface> magma =
    holder->get_magma_interface(); if( !magma ) return NULL;

            magma->set_expression_id( kInitialExpression );

            for( int i = 0, iEnd = magma->get_num_outputs( frantic::magma::magma_interface::TOPMOST_LEVEL ); i < iEnd;
    ++i ){ frantic::magma::nodes::magma_output_node* outNode = dynamic_cast<frantic::magma::nodes::magma_output_node*>(
    magma->get_node( magma->get_output( frantic::magma::magma_interface::TOPMOST_LEVEL, i ).first ) ); if( outNode &&
    outNode->get_enabled() && outNode->get_channelName() == szFieldName ){ boost::shared_ptr<EmberSimObjectContext> ctx(
    new EmberSimObjectContext(t) );

                            SimEmberMXSContext* pMXSContext = GetSimEmberContextInterface( pContext );

                            if( pMXSContext && pMXSContext->HasValue( _T("BoundsMin") ) && pMXSContext->HasValue(
    _T("BoundsMax") ) ){ ctx->bounds.set( frantic::max3d::from_max_t( pMXSContext->get<Point3>( _T("BoundsMin") ) ),
                                            frantic::max3d::from_max_t( pMXSContext->get<Point3>( _T("BoundsMax") ) )
                                    );
                            }

                            if( pMXSContext && pMXSContext->HasValue( _T("Spacing") ) ){
                                    ctx->spacing = pMXSContext->get<float>( _T("Spacing") );
                            }

                            ctx->timeStepSeconds = 0.f;

                            test::sim_compiler compiler;
                            compiler.set_abstract_syntax_tree( magma );
                            compiler.set_compilation_context( ctx );

                            std::unique_ptr<MXSField> result( new MXSField );

                            result->SetField( compiler.build_output_as_field( *outNode ), Box3(
    frantic::max3d::to_max_t( ctx->bounds.minimum() ), frantic::max3d::to_max_t( ctx->bounds.maximum() ) ), ctx->spacing
    );

                            return result.release();
                    }
            }
    }*/

    return NULL;
}

FPInterface* SimEmberOwner::GetUpdateField( FPInterface* /*pFieldCollection*/, const MCHAR* szFieldName,
                                            FPInterface* pContext, float timeStep,
                                            frantic::max3d::fnpublish::TimeWrapper t ) {
    // if( frantic::magma::max3d::IMagmaHolder* holder = frantic::magma::max3d::GetMagmaHolderInterface(
    // m_pblock->GetReferenceTarget( kUpdateHolder ) ) ){ 	boost::shared_ptr<frantic::magma::magma_interface> magma =
    //holder->get_magma_interface(); 	if( !magma ) 		return NULL;

    //	magma->set_expression_id( kPerStepExpression );

    //	for( int i = 0, iEnd = magma->get_num_outputs( frantic::magma::magma_interface::TOPMOST_LEVEL ); i < iEnd; ++i
    //){ 		frantic::magma::nodes::magma_output_node* outNode = dynamic_cast<frantic::magma::nodes::magma_output_node*>(
    //magma->get_node( magma->get_output( frantic::magma::magma_interface::TOPMOST_LEVEL, i ).first ) ); 		if( outNode &&
    //outNode->get_enabled() && outNode->get_channelName() == szFieldName ){ 			boost::shared_ptr<EmberSimObjectContext>
    //ctx( new EmberSimObjectContext(t) );

    //			SimEmberMXSContext* pMXSContext = GetSimEmberContextInterface( pContext );

    //			if( pMXSContext && pMXSContext->HasValue( _T("BoundsMin") ) && pMXSContext->HasValue( _T("BoundsMax") )
    //){ 				ctx->bounds.set( 					frantic::max3d::from_max_t( pMXSContext->get<Point3>( _T("BoundsMin") ) ),
    //					frantic::max3d::from_max_t( pMXSContext->get<Point3>( _T("BoundsMax") ) )
    //				);
    //			}

    //			if( pMXSContext && pMXSContext->HasValue( _T("Spacing") ) ){
    //				ctx->spacing = pMXSContext->get<float>( _T("Spacing") );
    //			}

    //			ctx->timeStepSeconds = timeStep;
    //
    //			test::sim_compiler compiler;
    //			compiler.set_abstract_syntax_tree( magma );
    //			compiler.set_compilation_context( ctx );
    //
    //			/*if( SimEmberSimulator* pMXSFieldCollection = GetSimEmberSimulatorInterface( pFieldCollection )
    //){ 				for( int i = 1, iEnd = pMXSFieldCollection->FieldCount(); i <= iEnd; ++i ){ 					MSTR name =
    //pMXSFieldCollection->GetFieldNameByIndex( i ); 					MXSField* pField = static_cast<MXSField*>(
    //pMXSFieldCollection->GetFieldByIndex( i ) );

    //					compiler.set_channel_value( name.data(), pField->get_field() );
    //				}
    //			}*/

    //			std::unique_ptr<MXSField> result( new MXSField );

    //			result->SetField( compiler.build_output_as_field( *outNode ), Box3( frantic::max3d::to_max_t(
    //ctx->bounds.minimum() ), frantic::max3d::to_max_t( ctx->bounds.maximum() ) ), ctx->spacing );

    //			return result.release();
    //		}
    //	}
    //}

    return NULL;
}

ClassDesc2* SimEmberOwner::GetClassDesc() { return GetSimEmberOwnerDesc(); }

SimEmberSimulator::SimEmberSimulator()
    : m_simTime( INT_MIN )
    , m_initMagma( 0 )
    , m_updateMagma( 0 )
    , m_startFrame( 0 )
    , m_endFrame( 100 )
    , m_numSubsteps( 1 )
    , m_psAdvect( _T("Advection") )
    , m_psUpdate( _T("Update") )
    , m_psDiscretize( _T("Discretize") )
    , m_psSolver( _T("Solver") ) {
    m_advector = boost::make_shared<rk2_advector>();
    m_solver = boost::make_shared<poisson_solver>();
}

SimEmberSimulator::~SimEmberSimulator() {}

void SimEmberSimulator::SetSimulationRange( int startFrame, int endFrame, int numSteps ) {
    m_startFrame = startFrame;
    m_endFrame = endFrame;
    if( m_endFrame < m_startFrame )
        m_endFrame = m_startFrame;
    m_numSubsteps = numSteps;
}

void SimEmberSimulator::SetInitMagmaExpression( ReferenceTarget* expr ) {
    m_initMagma = Animatable::GetHandleByAnim( expr );
}

void SimEmberSimulator::SetUpdateMagmaExpression( ReferenceTarget* expr ) {
    m_updateMagma = Animatable::GetHandleByAnim( expr );
}

void SimEmberSimulator::SetGridParameters( const Point3& pmin, const Point3& pmax, float spacing ) {
    m_contextData[_T("BoundsMin")].LoadPtr( TYPE_POINT3_BV, TYPE_POINT3_BV_RSLT pmin );
    m_contextData[_T("BoundsMax")].LoadPtr( TYPE_POINT3_BV, TYPE_POINT3_BV_RSLT pmax );
    m_contextData[_T("Spacing")].LoadPtr( TYPE_FLOAT, TYPE_FLOAT_RSLT spacing );

    m_simBounds = ember::get_ijk_inner_bounds( pmin, pmax, spacing );
    m_transform = openvdb::math::Transform::createLinearTransform( spacing );
    m_transform->postTranslate( openvdb::Vec3d( spacing / 2.f ) );
}

void SimEmberSimulator::SetContextValue( const MCHAR* szKeyName, const FPValue& val ) {
    m_contextData[szKeyName] = val;
}

void SimEmberSimulator::SetSolverEnabled( bool enabled ) {
    m_solver.reset();

    if( enabled )
        m_solver = boost::make_shared<poisson_solver>();
}

void SimEmberSimulator::SetErrorCallback( Value* callbackFn ) {
    if( !callbackFn || !is_function( callbackFn ) ) {
        m_errorCallback.reset();
    } else {
        m_errorCallback = frantic::max3d::mxs::make_shared_value( callbackFn );
    }
}

frantic::max3d::fnpublish::TimeParameter SimEmberSimulator::GetTime() {
    return frantic::max3d::fnpublish::TimeParameter( GetTicksPerFrame() * m_simTime );
}

Interval SimEmberSimulator::GetSimulationInterval() {
    return Interval( GetTicksPerFrame() * m_startFrame, GetTicksPerFrame() * m_endFrame );
}

Tab<const MCHAR*> SimEmberSimulator::GetCurrentFieldNames() {
    m_cachedFieldNames.clear();
    m_cachedFieldNames.reserve( m_fields.size() );
    for( openvdb::GridPtrVec::const_iterator it = m_fields.begin(), itEnd = m_fields.end(); it != itEnd; ++it )
#if MAX_VERSION_MAJOR >= 15
        m_cachedFieldNames.push_back( MSTR::FromACP( ( *it )->getName().c_str() ) );
#else
        m_cachedFieldNames.push_back( ( *it )->getName().c_str() );
#endif

    if( m_velocityField )
        m_cachedFieldNames.push_back( MSTR( _T("Velocity") ) );

    Tab<const MCHAR*> result;

    result.SetCount( static_cast<int>( m_cachedFieldNames.size() ) );

    std::size_t i = 0;
    for( std::vector<MSTR>::const_iterator it = m_cachedFieldNames.begin(), itEnd = m_cachedFieldNames.end();
         it != itEnd; ++it, ++i )
        result[i] = it->data();

    return result;
}

int SimEmberSimulator::GetTotalAdvectTime() { return static_cast<int>( m_psAdvect.total_time() ); }

int SimEmberSimulator::GetTotalUpdateTime() { return static_cast<int>( m_psUpdate.total_time() ); }

int SimEmberSimulator::GetTotalDiscretizeTime() { return static_cast<int>( m_psDiscretize.total_time() ); }

int SimEmberSimulator::GetTotalSolverTime() { return static_cast<int>( m_psSolver.total_time() ); }

// Returns false if interrupted by the callback.
bool SimEmberSimulator::Simulate( Value* callback ) {
    m_psAdvect.reset();
    m_psUpdate.reset();
    m_psDiscretize.reset();
    m_psSolver.reset();

    try {
        if( m_simTime < m_startFrame )
            this->do_init();

        assert( m_simTime >= m_startFrame );

        double timeStepSeconds = static_cast<double>( GetTicksPerFrame() ) / static_cast<double>( TIME_TICKSPERSEC );

        timeStepSeconds /= static_cast<double>( m_numSubsteps );

        if( callback && is_function( callback ) ) {
            bool shouldContinue = callback->apply( NULL, 0 )->to_bool() != FALSE;
            if( !shouldContinue )
                return false;
        }

        for( ; m_simTime <= m_endFrame; ) {
            for( int i = 0; i < m_numSubsteps; ++i ) {
                TimeValue actualTime = ( GetTicksPerFrame() * m_simTime ) + ( GetTicksPerFrame() * i ) / m_numSubsteps;

                this->do_step( actualTime, timeStepSeconds );
            }

            ++m_simTime;

            if( callback && is_function( callback ) ) {
                bool shouldContinue = callback->apply( NULL, 0 )->to_bool() != FALSE;
                if( !shouldContinue )
                    return false;
            }
        }

        if( m_errorCallback )
            frantic::magma::max3d::ClearError( m_errorCallback.get() );
    } catch( const frantic::magma::magma_exception& e ) {
        if( m_errorCallback )
            frantic::magma::max3d::ReportError( m_errorCallback.get(), e );

        // Return false to interrupt the simulation.
        return false;
    } // catch(...){ /*propogate all non-magma exceptions to MXS*/ }

    return true;
}

std::size_t SimEmberSimulator::get_field_count() { return m_fields.size(); }

openvdb::GridBase::ConstPtr SimEmberSimulator::get_grid( std::size_t i ) {
    if( i < m_fields.size() )
        return m_fields[i];
    return openvdb::GridBase::ConstPtr();
}

openvdb::GridBase::ConstPtr SimEmberSimulator::get_grid( const std::string& name ) {
    std::map<std::string, std::size_t>::const_iterator it = m_fieldMap.find( name );
    if( it != m_fieldMap.end() )
        return m_fields[it->second];
    return openvdb::GridBase::ConstPtr();
}

boost::shared_ptr<staggered_discretized_field> SimEmberSimulator::get_velocity() { return m_velocityField; }

boost::shared_ptr<concatenated_field> SimEmberSimulator::get_field_collection() {
    boost::shared_ptr<concatenated_field> result = boost::make_shared<concatenated_field>();

    for( openvdb::GridPtrVec::const_iterator it = m_fields.begin(), itEnd = m_fields.end(); it != itEnd; ++it )
        result->add_field( boost::shared_ptr<frantic::volumetrics::field_interface>(
            ember::create_field_interface( *it ).release() ) );

    if( m_velocityField )
        result->add_field( m_velocityField );

    return result;
}

template <class T>
void SimEmberSimulator::set_field_constant( const std::string& name, const T& val ) {}

template <>
void SimEmberSimulator::set_field_constant<float>( const std::string& name, const float& val ) {
    std::map<std::string, std::size_t>::const_iterator it = m_fieldMap.find( name );
    if( it == m_fieldMap.end() ) {
        m_fieldMap[name] = m_fields.size();
        m_fields.push_back( openvdb::FloatGrid::create( val ) );
        m_fields.back()->setName( name );
        if( name == "Density" )
            m_fields.back()->setGridClass( openvdb::GRID_FOG_VOLUME );
    } else {
        openvdb::GridBase::Ptr field = openvdb::FloatGrid::create( val );
        field->setName( name );
        if( name == "Density" )
            field->setGridClass( openvdb::GRID_FOG_VOLUME );
        m_fields[it->second] = field;
    }
}

template <>
void SimEmberSimulator::set_field_constant<openvdb::Vec3f>( const std::string& name, const openvdb::Vec3f& val ) {
    std::map<std::string, std::size_t>::const_iterator it = m_fieldMap.find( name );
    if( it == m_fieldMap.end() ) {
        m_fieldMap[name] = m_fields.size();
        m_fields.push_back( openvdb::Vec3fGrid::create( val ) );
        m_fields.back()->setName( name );
        if( name == "Normal" || name == "Tangent" )
            m_fields.back()->setVectorType( openvdb::VEC_COVARIANT_NORMALIZE );
    } else {
        openvdb::GridBase::Ptr field = openvdb::Vec3fGrid::create( val );
        field->setName( name );
        if( name == "Normal" || name == "Tangent" )
            field->setVectorType( openvdb::VEC_COVARIANT_NORMALIZE );
        m_fields[it->second] = field;
    }
}

void SimEmberSimulator::set_field_constant( const std::string& name, frantic::channels::data_type_t dt,
                                            std::size_t arity, const void* val ) {
    switch( dt ) {
    case frantic::channels::data_type_float32:
        if( arity == 1 ) {
            this->set_field_constant( name, reinterpret_cast<const float*>( val )[0] );
        } else if( arity == 3 ) {
            this->set_field_constant( name, openvdb::Vec3f( reinterpret_cast<const float*>( val ) ) );
        }
        break;
    default:
        break;
    }
}

void SimEmberSimulator::do_step( TimeValue currentTime, double timeStepSeconds ) {
    FF_LOG( progress ) << _T("Simulating frame: ") << frantic::max3d::to_frames<double>( currentTime ) << std::endl;

    boost::shared_ptr<frantic::volumetrics::field_interface> nextVelocity;
    std::map<frantic::tstring, boost::shared_ptr<frantic::volumetrics::field_interface>> nextFields;

    {
        frantic::diagnostics::scoped_profile spsAdvect( m_psAdvect );

        // TODO: Make an advection decorator that hardcodes the openvdb grid & staggered velocity grid.
        if( m_velocityField ) {
            for( openvdb::GridPtrVec::iterator it = m_fields.begin(), itEnd = m_fields.end(); it != itEnd; ++it ) {
                boost::shared_ptr<frantic::volumetrics::field_interface> field(
                    ember::create_field_interface( *it ).release() );

                nextFields[frantic::strings::to_tstring( ( *it )->getName() )] = m_advector->apply_advection_decorator(
                    field, m_velocityField, static_cast<float>( timeStepSeconds ) );
            }

            nextVelocity = m_advector->apply_advection_decorator( m_velocityField, m_velocityField,
                                                                  static_cast<float>( timeStepSeconds ) );
        } else {
            for( openvdb::GridPtrVec::iterator it = m_fields.begin(), itEnd = m_fields.end(); it != itEnd; ++it )
                nextFields[frantic::strings::to_tstring( ( *it )->getName() )] = ember::create_field_interface( *it );
        }
    }

    {
        frantic::diagnostics::scoped_profile spsUpdate( m_psUpdate );
        if( m_updateMagma ) {
            if( frantic::magma::max3d::IMagmaHolder* holder =
                    frantic::magma::max3d::GetMagmaHolderInterface( Animatable::GetAnimByHandle( m_updateMagma ) ) ) {
                boost::shared_ptr<frantic::magma::magma_interface> magma = holder->get_magma_interface();
                if( magma ) {
                    magma->set_expression_id( kPerStepExpression );

                    // TODO: We don't need to evaluate the channels separately.
                    for( int i = 0, iEnd = magma->get_num_outputs( frantic::magma::magma_interface::TOPMOST_LEVEL );
                         i < iEnd; ++i ) {
                        frantic::magma::nodes::magma_output_node* outNode =
                            dynamic_cast<frantic::magma::nodes::magma_output_node*>( magma->get_node(
                                magma->get_output( frantic::magma::magma_interface::TOPMOST_LEVEL, i ).first ) );
                        if( outNode && outNode->get_enabled() &&
                            outNode->get_input( 0 ).first != frantic::magma::magma_interface::INVALID_ID ) {
                            boost::shared_ptr<EmberSimObjectContext> ctx( new EmberSimObjectContext( currentTime ) );

                            std::map<frantic::tstring, FPValue>::const_iterator itBoundsMin =
                                m_contextData.find( _T("BoundsMin") );
                            std::map<frantic::tstring, FPValue>::const_iterator itBoundsMax =
                                m_contextData.find( _T("BoundsMax") );
                            std::map<frantic::tstring, FPValue>::const_iterator itSpacing =
                                m_contextData.find( _T("Spacing") );

                            if( itBoundsMin != m_contextData.end() && itBoundsMax != m_contextData.end() &&
                                ( itBoundsMin->second.type & ( ~TYPE_BY_VAL ) ) == TYPE_POINT3 &&
                                ( itBoundsMax->second.type & ( ~TYPE_BY_VAL ) ) == TYPE_POINT3 ) {
                                ctx->bounds.set(
                                    frantic::max3d::from_max_t( TYPE_POINT3_FIELD( itBoundsMin->second ) ),
                                    frantic::max3d::from_max_t( TYPE_POINT3_FIELD( itBoundsMax->second ) ) );
                            }

                            if( itSpacing != m_contextData.end() && itSpacing->second.type == TYPE_FLOAT )
                                ctx->spacing = TYPE_FLOAT_FIELD( itSpacing->second );

                            ctx->timeStepSeconds = static_cast<float>( timeStepSeconds );

                            test::sim_compiler compiler;
                            compiler.set_abstract_syntax_tree( magma );
                            compiler.set_compilation_context( ctx );

                            for( std::map<frantic::tstring,
                                          boost::shared_ptr<frantic::volumetrics::field_interface>>::iterator
                                     it = nextFields.begin(),
                                     itEnd = nextFields.end();
                                 it != itEnd; ++it )
                                compiler.set_channel_value( it->first, it->second );

                            if( nextVelocity )
                                compiler.set_channel_value( _T("Velocity"), nextVelocity );

                            boost::shared_ptr<frantic::volumetrics::field_interface> field;

                            try {
                                field = compiler.build_output_as_field( *outNode );
                            } catch( const frantic::magma::magma_exception& /*e*/ ) {
                                throw;
                            } catch( const std::exception& e ) {
                                throw frantic::magma::magma_exception() << frantic::magma::magma_exception::error_name(
                                    frantic::strings::to_tstring( e.what() ) );
                            }

                            if( outNode->get_channelName() == _T("Velocity") ) {
                                nextVelocity = field;
                            } else {
                                nextFields[outNode->get_channelName()] = field;
                            }
                        }
                    }
                }
            }
        }
    }

    {
        frantic::diagnostics::scoped_profile spsDiscretize( m_psDiscretize );

        for( std::map<frantic::tstring, boost::shared_ptr<frantic::volumetrics::field_interface>>::iterator
                 it = nextFields.begin(),
                 itEnd = nextFields.end();
             it != itEnd; ++it ) {
            assert( it->second->get_channel_map().channel_count() == 1 );

            openvdb::GridPtrVec grids;
            ember::create_grids( grids, it->second, m_transform, m_simBounds );

            if( !grids.empty() ) {
                if( frantic::logging::is_logging_debug() ) {
                    frantic::logging::debug << _T("At time: ") << ( frantic::max3d::to_frames<double>( currentTime ) )
                                            << std::endl;
                    frantic::logging::debug << _T("\tGrid: ")
                                            << frantic::strings::to_tstring( grids.front()->getName() ) << std::endl;

                    openvdb::CoordBBox bbox = grids.front()->evalActiveVoxelBoundingBox();
                    std::stringstream ss;
                    ss << bbox << std::endl;

                    frantic::logging::debug << _T("\t\tBounds: ") << frantic::strings::to_tstring( ss.str() )
                                            << std::endl;
                    frantic::logging::debug << _T("\t\tSpacing: ") << grids.front()->transform().voxelSize()[0]
                                            << std::endl;
                }

                m_fields[m_fieldMap[grids.front()->getName()]] = grids.front();
            }
        }

        nextFields.clear();
    }

    {
        frantic::diagnostics::scoped_profile spsDiscretize( m_psDiscretize );

        if( nextVelocity ) {
            int voxelBounds[6];
            m_simBounds.min().asXYZ( voxelBounds[0], voxelBounds[2], voxelBounds[4] );
            m_simBounds.max().offsetBy( 1 ).asXYZ( voxelBounds[1], voxelBounds[3], voxelBounds[5] );

            // TODO: If this doesn't depend on the existing velocity, we can sample in-place. It's a fully dense grid so
            // we can't miss anything. I think we can determine this is the case by checking if our shared_ptr to the
            // velocity is the only one. Otherwise 'nextVelocity' will have additional references.
            if( !m_velocityField.unique() )
                m_velocityField = boost::make_shared<ember::staggered_discretized_field>(
                    voxelBounds, static_cast<float>( m_transform->voxelSize()[0] ), _T("Velocity") );
            m_velocityField->assign( *nextVelocity );

            if( m_solver )
                ember::do_poisson_solve( m_velocityField->get_grid(), static_cast<float>( timeStepSeconds ) );
        }

        nextVelocity.reset();
    }
}

void SimEmberSimulator::do_init() {
    FF_LOG( progress ) << _T("Initializing frame: ") << m_startFrame << std::endl;

    m_simTime = m_startFrame;
    m_fields.clear();
    m_fieldMap.clear();
    m_velocityField.reset();

    TimeValue simTimeTicks = GetTicksPerFrame() * m_simTime;

    if( m_initMagma != 0 ) {
        // Sample the initial values to the grid. TODO: We don't need to evaluate the channels separately.
        if( frantic::magma::max3d::IMagmaHolder* holder =
                frantic::magma::max3d::GetMagmaHolderInterface( Animatable::GetAnimByHandle( m_initMagma ) ) ) {
            boost::shared_ptr<frantic::magma::magma_interface> magma = holder->get_magma_interface();
            if( magma ) {
                magma->set_expression_id( kInitialExpression );

                for( int i = 0, iEnd = magma->get_num_outputs( frantic::magma::magma_interface::TOPMOST_LEVEL );
                     i < iEnd; ++i ) {
                    frantic::magma::nodes::magma_output_node* outNode =
                        dynamic_cast<frantic::magma::nodes::magma_output_node*>( magma->get_node(
                            magma->get_output( frantic::magma::magma_interface::TOPMOST_LEVEL, i ).first ) );
                    if( outNode && outNode->get_enabled() &&
                        outNode->get_input( 0 ).first != frantic::magma::magma_interface::INVALID_ID ) {
                        boost::shared_ptr<EmberSimObjectContext> ctx( new EmberSimObjectContext( simTimeTicks ) );

                        std::map<frantic::tstring, FPValue>::const_iterator itBoundsMin =
                            m_contextData.find( _T("BoundsMin") );
                        std::map<frantic::tstring, FPValue>::const_iterator itBoundsMax =
                            m_contextData.find( _T("BoundsMax") );
                        std::map<frantic::tstring, FPValue>::const_iterator itSpacing =
                            m_contextData.find( _T("Spacing") );

                        if( itBoundsMin != m_contextData.end() && itBoundsMax != m_contextData.end() &&
                            ( itBoundsMin->second.type & ( ~TYPE_BY_VAL ) ) == TYPE_POINT3 &&
                            ( itBoundsMax->second.type & ( ~TYPE_BY_VAL ) ) == TYPE_POINT3 ) {
                            ctx->bounds.set( frantic::max3d::from_max_t( TYPE_POINT3_FIELD( itBoundsMin->second ) ),
                                             frantic::max3d::from_max_t( TYPE_POINT3_FIELD( itBoundsMax->second ) ) );
                        }

                        if( itSpacing != m_contextData.end() && itSpacing->second.type == TYPE_FLOAT )
                            ctx->spacing = TYPE_FLOAT_FIELD( itSpacing->second );

                        ctx->timeStepSeconds = 0.f;

                        test::sim_compiler compiler;
                        compiler.set_abstract_syntax_tree( magma );
                        compiler.set_compilation_context( ctx );

                        boost::shared_ptr<frantic::volumetrics::field_interface> field;

                        try {
                            field = compiler.build_output_as_field( *outNode );
                        } catch( const frantic::magma::magma_exception& /*e*/ ) {
                            throw;
                        } catch( const std::exception& e ) {
                            throw frantic::magma::magma_exception() << frantic::magma::magma_exception::error_name(
                                frantic::strings::to_tstring( e.what() ) );
                        }

                        if( outNode->get_channelName() == _T("Velocity") ) {
                            // Sample to a staggered grid.
                            int voxelBounds[6];
                            m_simBounds.min().asXYZ( voxelBounds[0], voxelBounds[2], voxelBounds[4] );
                            m_simBounds.max().offsetBy( 1 ).asXYZ( voxelBounds[1], voxelBounds[3], voxelBounds[5] );

                            m_velocityField = boost::make_shared<ember::staggered_discretized_field>(
                                voxelBounds, static_cast<float>( m_transform->voxelSize()[0] ), _T("Velocity") );
                            m_velocityField->assign( *field );
                        } else {
                            // TODO: It would be easy to set a constant value if we could determine if an expression was
                            // space-constant (ie. not dependent on Position).
                            assert( field->get_channel_map().channel_count() == 1 );

                            // openvdb::GridBase::Ptr grid = ember::create_grid( field, m_transform, m_simBounds );
                            openvdb::GridPtrVec grids;
                            ember::create_grids( grids, field, m_transform, m_simBounds );

                            if( !grids.empty() ) {
                                m_fieldMap[grids.front()->getName()] = m_fields.size();
                                m_fields.push_back( grids.front() );
                            }
                        }
                    }
                }
            }
        }
    }

    if( m_updateMagma != 0 ) {
        // Add a default value for any channel that wasn't set in the init expression.
        if( frantic::magma::max3d::IMagmaHolder* holder =
                frantic::magma::max3d::GetMagmaHolderInterface( Animatable::GetAnimByHandle( m_updateMagma ) ) ) {
            boost::shared_ptr<frantic::magma::magma_interface> magma = holder->get_magma_interface();
            if( magma ) {
                magma->set_expression_id( kPerStepExpression );

                for( int i = 0, iEnd = magma->get_num_outputs( frantic::magma::magma_interface::TOPMOST_LEVEL );
                     i < iEnd; ++i ) {
                    frantic::magma::nodes::magma_output_node* outNode =
                        dynamic_cast<frantic::magma::nodes::magma_output_node*>( magma->get_node(
                            magma->get_output( frantic::magma::magma_interface::TOPMOST_LEVEL, i ).first ) );
                    if( !outNode || !outNode->get_enabled() ||
                        outNode->get_input( 0 ).first == frantic::magma::magma_interface::INVALID_ID )
                        continue;

                    if( outNode->get_channelName() == _T("Velocity") ) {
                        if( !m_velocityField ) {
                            // Sample to a staggered grid.
                            int voxelBounds[6];
                            m_simBounds.min().asXYZ( voxelBounds[0], voxelBounds[2], voxelBounds[4] );
                            m_simBounds.max().offsetBy( 1 ).asXYZ( voxelBounds[1], voxelBounds[3], voxelBounds[5] );

                            m_velocityField = boost::make_shared<ember::staggered_discretized_field>(
                                voxelBounds, static_cast<float>( m_transform->voxelSize()[0] ), _T("Velocity") );
                            // m_velocityField->get_grid().fill( vec3(0,0,0) );
                        }
                    } else if( m_fieldMap.find( frantic::strings::to_string( outNode->get_channelName() ) ) ==
                               m_fieldMap.end() ) {
                        const frantic::magma::magma_data_type& dt = outNode->get_channelType();
                        if( frantic::channels::is_channel_data_type_float( dt.m_elementType ) ) {
                            if( dt.m_elementCount == 1 )
                                this->set_field_constant( frantic::strings::to_string( outNode->get_channelName() ),
                                                          0.f );
                            else if( dt.m_elementCount == 3 )
                                this->set_field_constant( frantic::strings::to_string( outNode->get_channelName() ),
                                                          openvdb::Vec3f( 0.f ) );
                        }
                    }
                }
            }
        }
    }

    assert( m_fieldMap.find( "Velocity" ) == m_fieldMap.end() );
}

SimEmberSimulator::ThisInterfaceDesc* SimEmberSimulator::GetDesc() {
    static ThisInterfaceDesc theDesc( SimEmberSimulator_INTERFACE, _T("SimEmberFieldCollection"), 0 );

    if( theDesc.empty() ) {
        theDesc.function( _T("SetSimulationRange"), &SimEmberSimulator::SetSimulationRange )
            .param( _T("StartFrame") )
            .param( _T("EndFrame") )
            .param( _T("NumSubSteps"), 1 );

        theDesc.function( _T("SetInitMagmaExpression"), &SimEmberSimulator::SetInitMagmaExpression )
            .param( _T("MagmaHolder") );

        theDesc.function( _T("SetUpdateMagmaExpression"), &SimEmberSimulator::SetUpdateMagmaExpression )
            .param( _T("MagmaHolder") );

        theDesc.function( _T("SetGridParameters"), &SimEmberSimulator::SetGridParameters )
            .param( _T("BoundsMin") )
            .param( _T("BoundsMax") )
            .param( _T("Spacing") );

        theDesc.function( _T("SetContextValue"), &SimEmberSimulator::SetContextValue )
            .param( _T("KeyName") )
            .param( _T("Value") );

        theDesc.function( _T("SetSolverEnabled"), &SimEmberSimulator::SetSolverEnabled ).param( _T("Enabled") );

        theDesc.function( _T("SetErrorCallback"), &SimEmberSimulator::SetErrorCallback )
            .param( _T("CallbackFunction") );

        theDesc.function( _T("GetCurrentTime"), &SimEmberSimulator::GetTime );

        theDesc.function( _T("GetSimulationInterval"), &SimEmberSimulator::GetSimulationInterval );

        theDesc.function( _T("GetCurrentFieldNames"), &SimEmberSimulator::GetCurrentFieldNames );

        theDesc.read_only_property( _T("TotalAdvectTime"), &SimEmberSimulator::GetTotalAdvectTime );
        theDesc.read_only_property( _T("TotalUpdateTime"), &SimEmberSimulator::GetTotalUpdateTime );
        theDesc.read_only_property( _T("TotalDiscretizeTime"), &SimEmberSimulator::GetTotalDiscretizeTime );
        theDesc.read_only_property( _T("TotalSolverTime"), &SimEmberSimulator::GetTotalSolverTime );

        theDesc.function( _T("Simulate"), &SimEmberSimulator::Simulate )
            .param( _T("FrameCallback"), static_cast<Value*>( &undefined ) );
    }

    return &theDesc;
}

// inline SimEmberSimulator* GetSimEmberSimulatorInterface( InterfaceServer* pServer ){ return pServer ?
// static_cast<SimEmberSimulator*>( pServer->GetInterface( SimEmberSimulator_INTERFACE ) ) : NULL; }

class SimEmberFactory : public frantic::max3d::fnpublish::StaticInterface<SimEmberFactory> {
    static SimEmberFactory s_instance;

  public:
    SimEmberFactory();

    virtual ~SimEmberFactory();

    FPInterface* CreateFieldCollection() { return new SimEmberSimulator; }
    FPInterface* CreateSimEmberContext() { return new SimEmberMXSContext; }
    ReferenceTarget* CreateSimEmberOwner() { return new SimEmberOwner( FALSE ); }

    FPInterface* CreateSimEmberAdvector() {
        std::unique_ptr<SimEmberAdvector> result( new SimEmberAdvector );
        result->SetImpl( boost::make_shared<rk2_advector>() );
        return result.release();
    }

    FPInterface* CreateSimEmberSolver() {
        std::unique_ptr<SimEmberSolver> result( new SimEmberSolver );
        result->SetImpl( boost::make_shared<poisson_solver>() );
        return result.release();
    }

    FPInterface* CreateConstantField( const FPValue& value, const Point3& pmin, const Point3& pmax, float spacing,
                                      const MCHAR* szName ) {
        /*std::unique_ptr<MXSField> result( new MXSField );
        if( (value.type & (~TYPE_BY_VAL)) == TYPE_FLOAT ){
                result->SetField( boost::make_shared< constant_field<float> >( TYPE_FLOAT_FIELD(value), szName ),
        Box3(pmin, pmax), spacing ); }else if( (value.type & (~TYPE_BY_VAL)) == TYPE_POINT3 ){ result->SetField(
        boost::make_shared< constant_field<ember::vec3> >( frantic::max3d::from_max_t( TYPE_POINT3_FIELD(value) ),
        szName ), Box3(pmin, pmax), spacing );
        }
        return result.release();*/
        return NULL;
    }
};

SimEmberFactory SimEmberFactory::s_instance;

SimEmberFactory::SimEmberFactory()
    : frantic::max3d::fnpublish::StaticInterface<SimEmberFactory>( SimEmberFactory_INTERFACE, _T("SimEmberFactory"),
                                                                   0 ) {
    this->function( _T("CreateFieldCollection"), &SimEmberFactory::CreateFieldCollection );
    this->function( _T("CreateSimEmberContext"), &SimEmberFactory::CreateSimEmberContext );
    this->function( _T("CreateSimEmberHolder"), &SimEmberFactory::CreateSimEmberOwner );
    this->function( _T("CreateSimEmberAdvector"), &SimEmberFactory::CreateSimEmberAdvector );
    this->function( _T("CreateSimEmberSolver"), &SimEmberFactory::CreateSimEmberSolver );
    this->function( _T("CreateConstantField"), &SimEmberFactory::CreateConstantField )
        .param( _T("Value") )
        .param( _T("BoundsMin") )
        .param( _T("BoundsMax") )
        .param( _T("Spacing") )
        .param( _T("Name"), _T("Value") );
}

SimEmberFactory::~SimEmberFactory() {}

} // namespace max3d
} // namespace ember

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/ParticleSim/Advector.hpp>
#include <stoke/max3d/ParticleSim/IReflowField.hpp>

#include <stoke/dummy_advector.hpp>
#include <stoke/rk2_advector.hpp>
#include <stoke/rk3_advector.hpp>
#include <stoke/rk4_advector.hpp>

#include <frantic/max3d/convert.hpp>

#if MAX_VERSION_MAJOR >= 25
#include <geom/point3.h>
#else
#include <point3.h>
#endif

#include <boost/make_shared.hpp>

namespace stoke {
namespace max3d {

namespace {
FPInterfaceDesc theIAdvectorDesc( Advector_INTERFACE, _M( "Advector" ), 0, NULL, FP_MIXIN,

                                  IAdvector::kFnAdvectParticle, _T("AdvectParticle"), 0, TYPE_POINT3, 0, 4,
                                  _T("ParticlePosition"), 0, TYPE_POINT3, _T("ParticleVelocity"), 0, TYPE_POINT3,
                                  _T("VelocityField"), 0, TYPE_INTERFACE, _T("TimeStepSeconds"), 0, TYPE_FLOAT,

                                  p_end );
}

IAdvector::~IAdvector() {}

FPInterfaceDesc* IAdvector::GetDesc() { return &theIAdvectorDesc; }

Point3 IAdvector::AdvectParticleMXS( const Point3& p, const Point3& v, FPInterface* pVelocityField,
                                     float timeStepSeconds ) {
    IReflowField* pField = static_cast<IReflowField*>( pVelocityField->GetInterface( ReflowField_INTERFACE ) );

    if( !pField )
        throw MAXException( _T("Parameter must be a ReflowField"), 0 );

    return this->AdvectParticle( p, v, pField, timeStepSeconds );
}

FPInterface* CreateRK2Advector() {
    advector_interface_ptr impl;

    impl = boost::make_shared<rk2_advector>();

    return new Advector( impl );
}

FPInterface* CreateRK3Advector() {
    advector_interface_ptr impl;

    impl = boost::make_shared<rk3_advector>();

    return new Advector( impl );
}

FPInterface* CreateRK4Advector() {
    advector_interface_ptr impl;

    impl = boost::make_shared<rk4_advector>();

    return new Advector( impl );
}

Advector::Advector( advector_interface_ptr pImpl )
    : m_impl( pImpl ) {}

Advector::~Advector() {}

advector_interface_ptr Advector::GetImpl() { return m_impl; }

Point3 Advector::AdvectParticle( const Point3& p, const Point3& v, IReflowField* pVelocityField,
                                 float timeStepSeconds ) {
    return frantic::max3d::to_max_t( m_impl->advect_particle( frantic::max3d::from_max_t( p ),
                                                              frantic::max3d::from_max_t( v ),
                                                              *pVelocityField->GetImpl(), timeStepSeconds ) );
}

Point3 Advector::GetOffset( const Point3& p, const Point3& v, IReflowField* pVelocityField, float timeStepSeconds ) {
    return frantic::max3d::to_max_t( m_impl->get_offset( frantic::max3d::from_max_t( p ),
                                                         frantic::max3d::from_max_t( v ), *pVelocityField->GetImpl(),
                                                         timeStepSeconds ) );
}

} // namespace max3d
} // namespace stoke

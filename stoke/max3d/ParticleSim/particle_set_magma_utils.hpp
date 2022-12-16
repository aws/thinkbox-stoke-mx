// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <boost\make_shared.hpp>

#include <frantic\channels\channel_map_const_iterator.hpp>
#include <frantic\particles\streams\particle_array_particle_istream.hpp>

#include <frantic\magma\simple_compiler\simple_particle_compiler.hpp>

#include <frantic\magma\max3d\DebugInformation.hpp>
#include <frantic\magma\max3d\IErrorReporter.hpp>
#include <frantic\magma\max3d\IMagmaHolder.hpp>

#include <stoke\particle_set.hpp>
#include <stoke\particle_set_istream.hpp>

#include <stoke\max3d\ParticleSim\IParticleSet.hpp>
#include <stoke\max3d\ParticleSim\ParticleReflowContext.hpp>

namespace stoke {

enum reflow_expression_id { kPerStepExpression, kBirthExpression };

}

namespace stoke {
namespace max3d {

stoke::particle_set_interface_ptr apply_magma_to_particle_set( frantic::magma::max3d::IMagmaHolder* pMagmaHolder,
                                                               stoke::particle_set_interface_ptr pParticleSet,
                                                               TimeValue t,
                                                               frantic::magma::max3d::IErrorReporter2* pErrorReporter,
                                                               reflow_expression_id expressionId );

}
} // namespace stoke
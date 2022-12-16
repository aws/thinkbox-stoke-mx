// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <baseinterface.h>
#include <imtl.h> //For RenderGlobalContext

#include <map>

#undef base_type

#define EmberHolder_INTERFACE Interface_ID( 0x73420aed, 0x7357494e )

namespace ember {
class ember_compiler;
}

namespace ember {
namespace max3d {

class IEmberHolder : public BaseInterface {
  public:
    virtual ~IEmberHolder() {}

    virtual void build_expression( INode* node, const RenderGlobalContext& globContext, ember_compiler& outExpr,
                                   Interval& outValidity ) = 0;
};

inline IEmberHolder* GetEmberHolderInterface( Animatable* anim ) {
    return anim ? static_cast<IEmberHolder*>( anim->GetInterface( EmberHolder_INTERFACE ) ) : NULL;
}

} // namespace max3d
} // namespace ember

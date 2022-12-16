// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/max3d/EmberHolder.hpp>

namespace ember {
namespace max3d {

class SimEmberHolder : public EmberHolder {
  public:
    SimEmberHolder( BOOL loading );

    virtual ~SimEmberHolder() {}

  protected:
    class SimEmberHolderMagmaSingleton : public EmberHolderMagmaSingleton {
      public:
        static frantic::magma::magma_singleton& get_instance();

        SimEmberHolderMagmaSingleton();

        virtual ~SimEmberHolderMagmaSingleton() {}
    };

  protected:
    virtual ClassDesc2* GetClassDesc();
};

} // namespace max3d
} // namespace ember

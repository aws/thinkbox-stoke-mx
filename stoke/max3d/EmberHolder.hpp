// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/max3d/IEmberHolder.hpp>

#include <frantic/magma/max3d/MagmaHolder.hpp>

#include <frantic/magma/magma_singleton.hpp>

namespace ember {
namespace max3d {

class EmberHolder : public frantic::magma::max3d::MagmaHolder, public IEmberHolder {
  private:
    class EmberHolderClassDesc : public frantic::magma::max3d::MagmaHolderClassDesc {
      public:
        EmberHolderClassDesc();
        virtual ~EmberHolderClassDesc();

        virtual const MCHAR* ClassName();
        virtual HINSTANCE HInstance();
        virtual Class_ID ClassID();
        virtual void* Create( BOOL loading );
#if MAX_VERSION_MAJOR >= 24
        const MCHAR* NonLocalizedClassName() { return ClassName(); }
#endif
    };

  protected:
    class EmberHolderMagmaSingleton : public frantic::magma::magma_singleton {
      public:
        static frantic::magma::magma_singleton& get_instance();

        EmberHolderMagmaSingleton();

        virtual ~EmberHolderMagmaSingleton() {}
    };

  protected:
    virtual ClassDesc2* GetClassDesc();

  public:
    EmberHolder( BOOL loading );

    EmberHolder( boost::int64_t nodeSetID );

    virtual ~EmberHolder() {}

    static ClassDesc2& GetClassDescInstance();

    // From IEmberHolder
    virtual void build_expression( INode* node, const RenderGlobalContext& globContext, ember_compiler& outExpr,
                                   Interval& outValidity );

    // From Animatable
    virtual BaseInterface* GetInterface( Interface_ID id );

    // From ReferenceMaker
    virtual IOResult Load( ILoad* iload );

    virtual IOResult Save( ISave* isave );

  private:
    static std::unique_ptr<frantic::magma::magma_interface> create_magma_interface( boost::int64_t nodeSetID );

  private:
    boost::int64_t m_nodeSetID;
};

#define EMBER_BASIC_NODE_SET 0x1e3446d42dab636fLL
#define EMBER_SIM_NODE_SET 0x2c555d8f236d1c5fLL
#define STOKE_SIM_NODE_SET 0x007691d7086b027513

class EmberHolderMagmaSingleton : public frantic::magma::magma_singleton {
  public:
    static frantic::magma::magma_singleton& get_instance();

    EmberHolderMagmaSingleton();

    virtual ~EmberHolderMagmaSingleton() {}
};

class StokeHolderMagmaSingleton : public frantic::magma::magma_singleton {
  public:
    static frantic::magma::magma_singleton& get_instance();

    StokeHolderMagmaSingleton();

    virtual ~StokeHolderMagmaSingleton() {}
};

} // namespace max3d
} // namespace ember

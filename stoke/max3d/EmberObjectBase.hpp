// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/max3d/EmberPipeObject.hpp>

#include <frantic/max3d/particles/IMaxKrakatoaPRTObject.hpp>

namespace ember {
namespace max3d {

class EmberPipeObject;

class EmberObjectBase
    : public frantic::max3d::GenericReferenceTarget<GeomObject, EmberObjectBase>,
      public frantic::max3d::volumetrics::IEmberField,
      public frantic::max3d::particles::IMaxKrakatoaPRTObject /*, public MaxSDK::Graphics::IAddRenderItemsHelper*/ {
  public:
    EmberObjectBase();

    virtual ~EmberObjectBase();

    // From IEmberField
    virtual boost::shared_ptr<frantic::volumetrics::field_interface> create_field( INode* pNode, TimeValue t,
                                                                                   Interval& valid );

    // From IMaxKrakatoaPRTObject (Implemented to work around the fact that Krakatoa doesn't evaluate the world state of
    // an object, only the base object).
    virtual frantic::particles::particle_istream_ptr
    CreateStream( INode* pNode, Interval& outValidity,
                  boost::shared_ptr<frantic::max3d::particles::IMaxKrakatoaPRTEvalContext> pEvalContext );
    virtual void GetStreamNativeChannels( INode* pNode, TimeValue t, frantic::channels::channel_map& outChannelMap );

    // From IAddRenderItemsHelper
    /*virtual bool AddRenderItems(
            const MaxSDK::Graphics::MaxContext& maxContext,
            MaxSDK::Graphics::RenderNodeHandle& hTargetNode,
            MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer);*/

    // From Animatable
    virtual BaseInterface* GetInterface( Interface_ID id );

    // From BaseObject
    virtual bool RequiresSupportForLegacyDisplayMode() const;

    /*virtual bool UpdateDisplay(
            const MaxSDK::Graphics::MaxContext& maxContext,
            const MaxSDK::Graphics::UpdateDisplayContext& displayContext);*/

    virtual void GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* vp, Box3& box );

    virtual void GetLocalBoundBox( TimeValue t, INode* inode, ViewExp* vp, Box3& box );

    virtual int Display( TimeValue t, INode* inode, ViewExp* vpt, int flags );

    virtual int HitTest( TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt );

    // From Object
    virtual ObjectState Eval( TimeValue t );

    virtual Interval ObjectValidity( TimeValue t );

    virtual int CanConvertToType( Class_ID obtype );

    virtual Object* ConvertToType( TimeValue t, Class_ID obtype );

    // From GeomObject
    virtual Mesh* GetRenderMesh( TimeValue t, INode* inode, View& view, BOOL& needDelete );

  protected:
    virtual void EvalField( EmberPipeObject& objToPopulate, TimeValue t ) = 0;

    virtual void SetIconScale( float scale ) = 0;

    void InvalidateCache();

    // TODO: Replace this access with a more specific way to access the display properties. Either duplicate the
    // viewport maniuplation functions or expose it as a separate interface. Viewport display settings can be
    // manipulate, but the object's data must not be changed here. Invalidate the object to request a re-evaluation.
    EmberPipeObject& GetCurrentCache();

  private:
    ObjectState m_evalCache;
    Interval m_evalCacheValid;
};

} // namespace max3d
} // namespace ember

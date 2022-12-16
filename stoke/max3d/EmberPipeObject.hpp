// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <ember/async_field_task.hpp>

#include <frantic/max3d/GenericReferenceTarget.hpp>
#include <frantic/max3d/volumetrics/IEmberField.hpp>

#include <frantic/channels/channel_map.hpp>

#pragma warning( push, 3 )
#pragma warning( disable : 4913 )
#include <boost/shared_ptr.hpp>
#include <boost/thread/future.hpp>
#pragma warning( pop )

#pragma warning( push, 3 )
#include <maxversion.h>

#include <Graphics/CustomRenderItemHandle.h>
#include <frantic/max3d/viewport/ParticleRenderItem.hpp>

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#pragma warning( pop )

namespace ember {
namespace max3d {

ClassDesc2* GetEmberPipeObjectDesc();

class EmberToParticles;
class StokeRenderCallback;

void SetMaxMarkerCount( std::size_t count );
std::size_t GetMaxMarkerCount();

// We want this type of object to flow up the pipeline. It should never be saved into the scene.
class EmberPipeObject : public frantic::max3d::GenericReferenceTarget<GeomObject, EmberPipeObject>,
                        public frantic::max3d::volumetrics::IEmberField {
  public:
    struct display_mode {
        enum option { kDisplayNone, kDisplayDots, kDisplayLines };
    };

    struct spacing_mode {
        enum option { kSpacingConstant, kSpacingDynamic, kSpacingNatural };
    };

  public:
    EmberPipeObject();

    virtual ~EmberPipeObject();

    void SetEmpty();

    void Set( future_field_base::ptr_type futureField, const frantic::channels::channel_map& fieldMap,
              const Box3& bounds, float spacing, bool inWorldSpace = true );

    // Sets the icon mesh that is displayed alongside the field. Must be valid for the complete lifetime of the object
    // (ie. use a global mesh).
#if MAX_VERSION_MAJOR >= 25
    void EmberPipeObject::SetViewportIcon( const MaxSDK::Graphics::RenderItemHandleArray& hIconMeshes,
                                           MaxSDK::SharedMeshPtr pMesh, const Matrix3& iconTM );
#else
    void EmberPipeObject::SetViewportIcon( const MaxSDK::Graphics::RenderItemHandleArray& hIconMeshes, Mesh* pMesh,
                                           const Matrix3& iconTM );
#endif

    void SetViewportResolution( int x, int y, int z );

    void SetViewportSpacing( float x, float y, float z );

    void SetViewportSpacingNatural( int reduce = 0 );

    void SetViewportScalarChannel( const frantic::tstring& channel );

    void SetViewportVectorChannel( const frantic::tstring& channel );

    void SetViewportColorChannel( const frantic::tstring& channel );

    void SetViewportDisplayNone();

    void SetViewportDisplayDots( float minVal = 0.f, float maxVal = std::numeric_limits<float>::infinity(),
                                 float dotSize = 5.f );

    void SetViewportDisplayLines( bool normalize = false, float scale = 1.f );

    void SetViewportDisplayBounds( bool enabled = true );

    const Box3& GetBounds() const;

    float GetDefaultSpacing() const;

    const frantic::channels::channel_map& GetChannels() const;

    future_field_base::ptr_type GetFuture() const;

    bool GetInWorldSpace() const;

    // From IEmberField
    virtual boost::shared_ptr<frantic::volumetrics::field_interface> create_field( INode* pNode, TimeValue t,
                                                                                   Interval& valid );

    // From IEmberFieldAsync
    // virtual boost::shared_ptr<frantic::volumetrics::field_interface> try_create_field( INode* pNode, TimeValue t,
    // bool& outIsReady );

    // virtual future_field_base::ptr_type create_field_async( INode* pNode, TimeValue t );

    // From IAddRenderItems
    // virtual bool AddRenderItems( const MaxSDK::Graphics::MaxContext &maxContext, MaxSDK::Graphics::RenderNodeHandle
    // &hTargetNode, MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer );

    // From Animatable
    virtual BaseInterface* GetInterface( Interface_ID id );

    // ReferenceMaker
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

    // ReferenceTarget
    virtual void BaseClone( ReferenceTarget* from, ReferenceTarget* to, RemapDir& remap );

    // BaseObject
#if MAX_VERSION_MAJOR >= 17
    // Max 2015+ uses a new viewport system.
    virtual unsigned long GetObjectDisplayRequirement() const;

    void PopulateRenderItem(
        TimeValue t,
        frantic::max3d::viewport::ParticleRenderItem<frantic::max3d::viewport::fake_particle>& renderItem );

    virtual bool PrepareDisplay( const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext );

    virtual bool UpdatePerNodeItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                     MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                     MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer );

    virtual bool UpdatePerViewItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                     MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                     MaxSDK::Graphics::UpdateViewContext& viewContext,
                                     MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer );

    friend class EmberPipeObjectRenderItem;

#else
    virtual bool RequiresSupportForLegacyDisplayMode() const;
#endif

    virtual void GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* vp, Box3& box );

    virtual void GetLocalBoundBox( TimeValue t, INode* inode, ViewExp* vp, Box3& box );

    virtual CreateMouseCallBack* GetCreateMouseCallBack();

    virtual int Display( TimeValue t, INode* inode, ViewExp* vpt, int flags );

    virtual int HitTest( TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt );

    // Object
    virtual ObjectState Eval( TimeValue t );

    virtual void GetDeformBBox( TimeValue t, Box3& box, Matrix3* tm = NULL, BOOL useSel = FALSE );

    virtual int CanConvertToType( Class_ID obtype );

    virtual Object* ConvertToType( TimeValue t, Class_ID obtype );

    virtual Interval ChannelValidity( TimeValue t, int nchan );

    virtual void SetChannelValidity( int nchan, Interval v );

    virtual void InvalidateChannels( ChannelMask channels );

    virtual Interval ObjectValidity( TimeValue t );

    virtual Object* MakeShallowCopy( ChannelMask channels );

    virtual void ShallowCopy( Object* fromOb, ChannelMask channels );

    virtual void FreeChannels( ChannelMask channels );

    virtual void NewAndCopyChannels( ChannelMask channels );

    // From GeomObject
    virtual Mesh* GetRenderMesh( TimeValue t, INode* inode, View& view, BOOL& needDelete );

  protected:
    virtual ClassDesc2* GetClassDesc();

  private:
    boost::shared_ptr<frantic::volumetrics::field_interface> try_get_field( bool& outIsReady ) const;

    void update_accessors();

    void configure_viewport_sampling( float ( &off )[3], float ( &step )[3], int ( &count )[3], int& reduce );

    struct viewport_point_data {
        std::vector<Point3> pointData;
        std::vector<Point4> colorData;
        bool isVectors;
        int reduceLevel;
    };

    bool cache_viewport_data( TimeValue t );

    // virtual void DoDisplay( TimeValue t, MaxSDK::Graphics::IPrimitiveRenderer& renderer, const
    // MaxSDK::Graphics::DisplayCallbackContext& displayContext );

    void SampleFieldForViewport( TimeValue t, const boost::shared_ptr<frantic::volumetrics::field_interface>& field,
                                 std::vector<Point3>& outPoints, std::vector<Point4>& outColors, bool& outIsVectors,
                                 int& outReduceLevel );

    friend class EmberToParticles;
    friend class StokeRenderCallback;
    friend class StokeDisplayCallback;

    friend void do_viewport_update2( AnimHandle anim );

  private:
    // mutable boost::shared_ptr<frantic::volumetrics::field_interface> m_fieldData;
    // mutable boost::shared_future< boost::shared_ptr<frantic::volumetrics::field_interface> > m_futureField;

    future_field_base::ptr_type m_fieldData;

    frantic::channels::channel_map m_fieldMap; // This is the map that m_fieldData will have once it is populated.

    Box3 m_bounds;
    float m_spacing;
    bool m_inWorldSpace;

    Interval m_geomValid;
    ChannelMask m_chanValid; // Channels without intervals (ie. just valid or not) are stored here.

    struct viewport_data {
        frantic::tstring m_scalarChannel;   // Which channel are we displaying
        frantic::tstring m_vectorChannel;   // Which channel are we displaying
        frantic::tstring m_colorChannel;    // Which channel are we displaying
        display_mode::option m_displayMode; // What sort of visualization do we want

        int m_res[3];
        float m_spacing[3];
        int m_reduce;
        spacing_mode::option m_spacingMode;

        bool m_normalizeVectors;
        float m_vectorScale;

        float m_scalarMin, m_scalarMax;
        float m_dotSize;

        bool m_drawBounds;

        frantic::channels::channel_cvt_accessor<float> m_floatAccessor;
        frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> m_vecAccessor;
        frantic::channels::channel_cvt_accessor<frantic::graphics::color3f> m_colorAccessor;
        frantic::channels::channel_cvt_accessor<float> m_scalarColorAccessor;

#if MAX_VERSION_MAJOR >= 25
        MaxSDK::SharedMeshPtr m_pIcon;
#else
        Mesh* m_pIcon; // For use in legacy viewports.
#endif
        MaxSDK::Graphics::RenderItemHandleArray m_hIconMeshes; // For use in Nitrous viewports
        Matrix3 m_iconTM;

        viewport_data()
            : m_pIcon( NULL )
            , m_iconTM( TRUE ) {
            m_displayMode = display_mode::kDisplayNone;
            m_spacingMode = spacing_mode::kSpacingConstant;
            m_res[0] = m_res[1] = m_res[2] = 10;
            m_scalarMin = 0.f;
            m_scalarMax = std::numeric_limits<float>::infinity();
            m_dotSize = 3.f;
            m_drawBounds = false;
        }
    } m_viewportData;

    bool m_vpUpdatePending;
    boost::thread m_vpUpdateThread;

    bool m_vpDataValid;
    viewport_point_data m_vpData;

    MaxSDK::Graphics::CustomRenderItemHandle m_markerRenderItem;
    MaxSDK::Graphics::CustomRenderItemHandle m_iconRenderItem;
    MaxSDK::Graphics::CustomRenderItemHandle m_boundsRenderItem;

    bool m_wasRealized;
    Color m_wireColor;
};

} // namespace max3d
} // namespace ember

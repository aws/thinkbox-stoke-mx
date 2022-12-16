// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include <frantic/geometry/polymesh3.hpp>

#include <frantic/max3d/fpwrapper/mixin_wrapper.hpp>
#include <frantic/max3d/maxscript/scripted_object_ref.hpp>
#include <frantic/max3d/maxscript/scripted_object_ref_impl.hpp>
#include <frantic/max3d/parameter_extraction.hpp>

#ifdef X_AXIS
#undef X_AXIS
#endif

#ifdef Y_AXIS
#undef Y_AXIS
#endif

#ifdef Z_AXIS
#undef Z_AXIS
#endif
#include <openvdb/Grid.h>

#define FieldMesher_CLASS_ID Class_ID( 0x53e24fd6, 0x739211cb )        // class id of the plugin
#define FieldMesher_INTERFACE_ID Interface_ID( 0xae72d4a, 0x1cfd343b ) // class id of the plugin interface
#define FieldMesher_CLASS_NAME "FieldMesher"
#define FieldMesher_DISPLAY_NAME "Field Mesher"

enum {
    field_mesher_param_block,
    //
    field_mesher_param_block_count // keep last
};
class LoggerUpdater;

class FieldMesher : public GeomObject, public frantic::max3d::fpwrapper::FFMixinInterface<FieldMesher> {
  public:
    IParamBlock2* pblock2;
    Mesh mesh;
    MNMesh mm;
    Interval ivalid;

    frantic::graphics::boundbox3f m_meshBoundingBox;

  protected:
    static std::vector<int> m_displayModeDisplayCodes;
    static std::map<int, frantic::tstring> m_displayModeNames;
    static std::vector<int> m_samplerModeDisplayCodes;
    static std::map<int, frantic::tstring> m_samplerModeNames;

    class StaticInitializer {
      public:
        StaticInitializer();
    };
    static StaticInitializer m_staticInitializer;

    bool m_cachedPolymesh3IsTriangles;
    bool m_doneBuildNormals;

    // for offsetting with velocity
    frantic::geometry::polymesh3_ptr m_cachedPolymesh3;

    bool m_inRenderingMode; // are we rendering?
    TimeValue m_renderTime;

    static boost::shared_ptr<Mesh> m_pIconMesh; // gizmo/bbox icon

  public:
    static FieldMesher* editObj;

    FieldMesher();
    virtual ~FieldMesher();

    // Animatable methods
    void DeleteThis();
    Class_ID ClassID();
#if MAX_VERSION_MAJOR < 24
    void GetClassName( MSTR& s );
#else
    void GetClassName( MSTR& s, bool localized );
#endif
    int NumSubs();
    Animatable* SubAnim( int i );
#if MAX_VERSION_MAJOR < 24
    MSTR SubAnimName( int i );
#else
    MSTR SubAnimName( int i, bool localized );
#endif
    int NumParamBlocks();
    IParamBlock2* GetParamBlock( int i );
    IParamBlock2* GetParamBlockByID( BlockID i );
    void BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev = NULL );
    void EndEditParams( IObjParam* ip, ULONG flags, Animatable* next = NULL );
    ReferenceTarget* Clone( RemapDir& remap );
    void FreeCaches();
    int RenderBegin( TimeValue t, ULONG flags );
    int RenderEnd( TimeValue t );

    BaseInterface* GetInterface( Interface_ID id );

    // From ReferenceMaker
    int NumRefs();
    RefTargetHandle GetReference( int i );
    void SetReference( int i, RefTargetHandle rtarg );
    IOResult Load( ILoad* iload );
#if MAX_VERSION_MAJOR >= 17
    RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID, RefMessage message,
                                BOOL propagate );
#else
    RefResult NotifyRefChanged( Interval changeInt, RefTargetHandle hTarget, PartID& partID, RefMessage message );
#endif

    // Virtual methods From Object
    BOOL UseSelectionBrackets() { return TRUE; }

    // Virtual methods From BaseObject
    CreateMouseCallBack* GetCreateMouseCallBack();
#if MAX_VERSION_MAJOR < 24
    TYPE_STRING_TYPE GetObjectName() { return _T( FieldMesher_CLASS_NAME ); }
#else
    TYPE_STRING_TYPE GetObjectName( bool localized ) { return _T( FieldMesher_CLASS_NAME ); }
#endif
    BOOL HasViewDependentBoundingBox() { return TRUE; }

    int Display( TimeValue t, INode* inode, ViewExp* pView, int flags );
    int HitTest( TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt );

#if MAX_VERSION_MAJOR >= 17
    // IObjectDisplay2 entries
    unsigned long GetObjectDisplayRequirement() const;
#elif MAX_VERSION_MAJOR >= 14
    bool RequiresSupportForLegacyDisplayMode() const;
#endif

#if MAX_VERSION_MAJOR >= 17
    bool PrepareDisplay( const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext );
    bool UpdatePerNodeItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                             MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                             MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer );
#elif MAX_VERSION_MAJOR >= 15
    bool UpdateDisplay( const MaxSDK::Graphics::MaxContext& maxContext,
                        const MaxSDK::Graphics::UpdateDisplayContext& displayContext );
#elif MAX_VERSION_MAJOR == 14
    bool UpdateDisplay( unsigned long renderItemCategories,
                        const MaxSDK::Graphics::MaterialRequiredStreams& materialRequiredStreams, TimeValue t );
#endif

    // from Object
    ObjectState Eval( TimeValue t );
    Interval ObjectValidity( TimeValue t );
    void InitNodeName( MSTR& s );
    int CanConvertToType( Class_ID obtype );
    Object* ConvertToType( TimeValue t, Class_ID obtype );
    BOOL PolygonCount( TimeValue t, int& numFaces, int& numVerts );

    // From GeomObject
    int IntersectRay( TimeValue t, Ray& ray, float& at, Point3& norm );
    void GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* vpt, Box3& box );
    void GetLocalBoundBox( TimeValue t, INode* inode, ViewExp* vpt, Box3& box );
    void GetDeformBBox( TimeValue t, Box3& box, Matrix3* tm, BOOL useSel );
    Mesh* GetRenderMesh( TimeValue t, INode* inode, View& view, BOOL& needDelete );

    // To support the Frantic Function Publishing interface
    void InitializeFPDescriptor();

    void SetRenderTime( TimeValue t );
    void ClearRenderTime();
    TimeValue GetRenderTime() const;
    bool HasValidRenderTime() const;

    void SetEmptyValidityAndNotifyDependents();

    static const std::vector<int>& get_display_mode_codes();

    static const std::vector<int>& get_sampler_codes();

    bool render_using_viewport_settings( TimeValue t ) const;

    static frantic::tstring get_display_mode_name( int code );

    static frantic::tstring get_sampler_name( int code );

    int get_display_mode( TimeValue t ) const;
    int get_viewport_sampler( TimeValue t ) const;
    int get_render_sampler( TimeValue t ) const;
    void set_display_mode( int val, TimeValue t );
    void set_viewport_sampler( int val, TimeValue t );
    void set_render_sampler( int val, TimeValue t );

    void get_available_channels( TimeValue t, std::vector<frantic::tstring>& outChannels );

    void invalidate();

    void report_warning( const frantic::tstring& msg );
    void report_error( const frantic::tstring& msg );
    bool get_grids( openvdb::GridCPtrVec& outGrids, TimeValue t );
    bool is_in_viewport() const;

    void build_mesh( TimeValue t, LoggerUpdater* logger, frantic::tstring& outError );

  private:
    bool use_flip_normals( TimeValue t ) const;
    bool in_object_space( TimeValue t ) const;
    bool get_show_icon( TimeValue t ) const;
    float get_icon_size( TimeValue t ) const;
    bool get_keep_mesh_in_memory( TimeValue t ) const;
    int get_output_velocity_channel( TimeValue t ) const;
    bool get_vel_to_map_channel( TimeValue t ) const;
    bool get_enable_viewport_mesh( TimeValue t ) const;

    void clear_cache();
    void clear_trimesh3_cache();
    void clear_cache_mxs();

    void add_channel( openvdb::GridBase::ConstPtr grid );

    // this just wraps the NotifyDependents call
    void NotifyEveryone();

    // our methods
    void UpdateMesh( TimeValue t );

    INode* get_inode();

    bool get_grids( openvdb::GridCPtrVec& outGrids, bool& outTransformed, TimeValue t );

    void BuildMesh( TimeValue t );

    bool has_triangle_mesh();

    void build_normals();

    frantic::tstring get_node_name();
};

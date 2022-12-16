// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource.h"
#include "resource_ember.h"

#include <stoke/max3d/EmberMagmaContext.hpp>
#include <stoke/max3d/EmberObjectBase.hpp>
#include <stoke/max3d/IEmberHolder.hpp>

#include <ember/ember_compiler.hpp>

#include <frantic/magma/magma_exception.hpp>
#include <frantic/magma/max3d/DebugInformation.hpp>
#include <frantic/magma/max3d/IErrorReporter.hpp>
#include <frantic/magma/max3d/IMagmaHolder.hpp>

#include <frantic/max3d/create_mouse_callback.hpp>
#include <frantic/max3d/exception.hpp>
#include <frantic/max3d/fnpublish/MixinInterface.hpp>
#include <frantic/max3d/max_utility.hpp>
#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>
#include <frantic/max3d/rendering/renderplugin_utils.hpp>

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#define FieldMagma_CLASSID Class_ID( 0x59f57c53, 0x294a1923 )
#define FieldMagma_NAME "StokeFieldStatic"
#define FieldMagma_DISPLAYNAME "Stoke Field Static"
#define FieldMagma_VERSION 1

#define EmberHolder_CLASS Class_ID( 0x2b493510, 0x5134620d )

#define FieldMagma_INTERFACE Interface_ID( 0x7b5d64e8, 0xced4a9e )

extern void BuildMesh_Text_EMBER_Field( Mesh& outMesh );

namespace ember {
namespace max3d {

enum {
    kMagmaHolder,
    kLength,
    kWidth,
    kHeight,
    kGridSpacing,
    kViewportReduce,
    kIconSize,
    kBoundsMin,
    kBoundsMax,
    kViewportMaskChannel,
    kViewportVectorChannel,
    kViewportColorChannel,
    kViewportVectorScale,
    kViewportVectorNormalize,
    kViewportEnabled,
    kEnforceBounds,
    kViewportBoundsEnabled
};

ClassDesc2* GetFieldMagmaDesc();

class IFieldMagma : public frantic::max3d::fnpublish::MixinInterface<IFieldMagma> {
  public:
    virtual ~IFieldMagma() {}

    virtual FPInterface* DebugFieldAt( INode* pNode, const Tab<Point3*>& points,
                                       frantic::max3d::fnpublish::TimeWrapper t ) = 0;

    // From MixinInterface
    virtual ThisInterfaceDesc* GetDesc() { return IFieldMagma::GetStaticDesc(); }

    static ThisInterfaceDesc* GetStaticDesc() {
        static ThisInterfaceDesc theDesc( FieldMagma_INTERFACE, _T("FieldMagma"), 0 );

        if( theDesc.empty() ) { // ie. Check if we haven't initialized the descriptor yet
            theDesc.SetClassDesc( GetFieldMagmaDesc() );

            theDesc.function( _T("DebugFieldAt"), &IFieldMagma::DebugFieldAt )
                .param( _T("Node") )
                .param( _T("Locations") );
        }

        return &theDesc;
    }
};

class FieldMagma : public EmberObjectBase, public IFieldMagma, public frantic::magma::max3d::ErrorReporter {
  public:
    FieldMagma( BOOL loading );

    virtual ~FieldMagma();

    bool build_ember_expression( ember::ember_compiler& outExpr, TimeValue t, const Box3& bounds, float spacing,
                                 Interval& outValid );

    // From IFieldMagma
    virtual FPInterface* DebugFieldAt( INode* pNode, const Tab<Point3*>& points,
                                       frantic::max3d::fnpublish::TimeWrapper t );

    // FPMixinInterface
    virtual FPInterfaceDesc* GetDescByID( Interface_ID id );

    // Animatable
    virtual BaseInterface* GetInterface( Interface_ID id );

    // From ReferenceMaker
    virtual void BaseClone( RefTargetHandle from, RefTargetHandle to, RemapDir& remap );
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

// From BaseObject
#if MAX_VERSION_MAJOR < 24
    virtual TYPE_STRING_TYPE GetObjectName();
#else
    virtual TYPE_STRING_TYPE GetObjectName( bool localized );
#endif

    virtual CreateMouseCallBack* GetCreateMouseCallBack();

    // From Object
    virtual void InitNodeName( MSTR& s );

  protected:
    virtual void EvalField( EmberPipeObject& objToPopulate, TimeValue t );

    virtual void SetIconScale( float scale );

    virtual ClassDesc2* GetClassDesc();

    ReferenceTarget* get_magma_holder() const {
        return frantic::max3d::get<ReferenceTarget*>( m_pblock, kMagmaHolder );
    }
    // float get_length( TimeValue t ) const { return frantic::max3d::get<float>( m_pblock, kLength, t ); }
    // float get_width( TimeValue t ) const { return frantic::max3d::get<float>( m_pblock, kWidth, t ); }
    // float get_height( TimeValue t ) const { return frantic::max3d::get<float>( m_pblock, kHeight, t ); }
    Box3 get_bounds( TimeValue t ) const {
        return Box3( frantic::max3d::get<Point3>( m_pblock, kBoundsMin, t ),
                     frantic::max3d::get<Point3>( m_pblock, kBoundsMax, t ) );
    }
    Box3 get_bounds( TimeValue t, Interval& outValid ) const {
        return Box3( frantic::max3d::get<Point3>( m_pblock, kBoundsMin, t ),
                     frantic::max3d::get<Point3>( m_pblock, kBoundsMax, t, outValid ) );
    }
    float get_grid_spacing( TimeValue t ) const { return frantic::max3d::get<float>( m_pblock, kGridSpacing, t ); }
    float get_grid_spacing( TimeValue t, Interval& outValid ) const {
        return frantic::max3d::get<float>( m_pblock, kGridSpacing, t, outValid );
    }
    inline bool get_enforce_bounds() const { return frantic::max3d::get<bool>( m_pblock, kEnforceBounds ); }

    inline bool get_viewport_enabled() { return frantic::max3d::get<bool>( m_pblock, kViewportEnabled ); }
    inline const MCHAR* get_viewport_mask_channel() {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kViewportMaskChannel );
    }
    inline const MCHAR* get_viewport_vector_channel() {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kViewportVectorChannel );
    }
    inline const MCHAR* get_viewport_color_channel() {
        return frantic::max3d::get<const MCHAR*>( m_pblock, kViewportColorChannel );
    }
    inline float get_viewport_vector_scale( TimeValue t ) {
        return frantic::max3d::get<float>( m_pblock, kViewportVectorScale, t );
    }
    inline bool get_viewport_vector_normalize() {
        return frantic::max3d::get<bool>( m_pblock, kViewportVectorNormalize );
    }
    inline bool get_viewport_bounds_enabled() const {
        return frantic::max3d::get<bool>( m_pblock, kViewportBoundsEnabled );
    }
};

class FieldMagmaDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_paramDesc;

    FPInterfaceDesc m_errorReporterInterface;

    friend class FieldMagma;

  public:
    FieldMagmaDesc();

    FPInterfaceDesc* GetDescByID( Interface_ID id ) {
        if( id == ErrorReporter_INTERFACE )
            return &m_errorReporterInterface;
        return NULL;
    }

    int IsPublic() { return FALSE; }
    void* Create( BOOL loading ) { return new FieldMagma( loading ); }
    const TCHAR* ClassName() { return _T( FieldMagma_DISPLAYNAME ); }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return FieldMagma_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() { return _T( FieldMagma_NAME ); } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }                   // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( FieldMagma_DISPLAYNAME ); }
#endif
};

enum { kMainPBlock };

FieldMagmaDesc::FieldMagmaDesc()
    : m_paramDesc( kMainPBlock, _T("Parameters"), 0, FieldMagma_VERSION )
    , m_errorReporterInterface( ErrorReporter_INTERFACE, _T("ErrorReporter"), 0, NULL, FP_MIXIN, p_end ) {
    m_paramDesc.OwnerRefNum( 0 );
    m_paramDesc.OwnerClassDesc( this );

    m_paramDesc.Parameter<float>( kIconSize, _T("IconSize"), 0 ).DefaultValue( 30.f ).Range( 0.f, FLT_MAX );

    m_paramDesc.Parameter<ReferenceTarget*>( kMagmaHolder, _T("MagmaHolder"), 0 ).DefaultValue( NULL );

    m_paramDesc.Parameter<Point3>( kBoundsMin, _T("BoundsMin"), 0 )
        .EnableAnimation( true, IDS_FIELDMAGMA_BOUNDSMIN )
        .DefaultValue( Point3( -10, -10, 0 ) )
        .Range( Point3( -FLT_MAX, -FLT_MAX, -FLT_MAX ), Point3( FLT_MAX, FLT_MAX, FLT_MAX ) );

    m_paramDesc.Parameter<Point3>( kBoundsMax, _T("BoundsMax"), 0 )
        .EnableAnimation( true, IDS_FIELDMAGMA_BOUNDSMAX )
        .DefaultValue( Point3( 10, 10, 20 ) )
        .Range( Point3( -FLT_MAX, -FLT_MAX, -FLT_MAX ), Point3( FLT_MAX, FLT_MAX, FLT_MAX ) );

    m_paramDesc.Parameter<float>( kGridSpacing, _T("GridSpacing"), 0 )
        .EnableAnimation( true, IDS_FIELDMAGMA_SPACING )
        .DefaultValue( 1.f )
        .Range( 1e-5f, FLT_MAX );

    m_paramDesc.Parameter<bool>( kEnforceBounds, _T("LimitToBounds"), 0 ).DefaultValue( true );

    m_paramDesc.Parameter<bool>( kViewportEnabled, _T("ViewportEnabled"), 0 ).DefaultValue( true );

    m_paramDesc.Parameter<int>( kViewportReduce, _T("ViewportReduce"), 0 ).DefaultValue( 0 ).Range( 0, INT_MAX );

    m_paramDesc.Parameter<TYPE_STRING_TYPE>( kViewportMaskChannel, _T("ViewportMaskChannel"), 0 )
        .DefaultValue( _T("") );

    m_paramDesc.Parameter<TYPE_STRING_TYPE>( kViewportVectorChannel, _T("ViewportVectorChannel"), 0 )
        .DefaultValue( _T("") );

    m_paramDesc.Parameter<TYPE_STRING_TYPE>( kViewportColorChannel, _T("ViewportColorChannel"), 0 )
        .DefaultValue( _T("") );

    m_paramDesc.Parameter<float>( kViewportVectorScale, _T("ViewportVectorScale"), 0 )
        .DefaultValue( 1.f )
        .Range( 0.f, FLT_MAX );

    m_paramDesc.Parameter<bool>( kViewportVectorNormalize, _T("ViewportVectorNormalize"), 0 ).DefaultValue( false );

    m_paramDesc.Parameter<bool>( kViewportBoundsEnabled, _T("ViewportBoundsEnabled"), 0 ).DefaultValue( false );

    m_errorReporterInterface.SetClassDesc( this );

    frantic::magma::max3d::IErrorReporter::init_fpinterface_desc( m_errorReporterInterface );

    this->AddInterface( frantic::magma::max3d::ErrorReporter::GetStaticDesc() );
}

ClassDesc2* GetFieldMagmaDesc() {
    static FieldMagmaDesc theFieldMagmaDesc;
    return &theFieldMagmaDesc;
}

FieldMagma::FieldMagma( BOOL loading ) {
    GetFieldMagmaDesc()->MakeAutoParamBlocks( this );

    if( !loading )
        frantic::max3d::set( m_pblock, kMagmaHolder, 0,
                             static_cast<ReferenceTarget*>( CreateInstance( REF_TARGET_CLASS_ID, EmberHolder_CLASS ) ),
                             0 );
}

FieldMagma::~FieldMagma() {}

FPInterface* FieldMagma::DebugFieldAt( INode* pNode, const Tab<Point3*>& points,
                                       frantic::max3d::fnpublish::TimeWrapper t ) {
    std::unique_ptr<frantic::magma::max3d::DebugInformation> pResult( new frantic::magma::max3d::DebugInformation );

    pResult->get_storage().resize( static_cast<std::size_t>( points.Count() ) );

    try {
        ember::ember_compiler expr;

        Interval valid = FOREVER;

        if( !this->build_ember_expression( expr, t, this->get_bounds( t ), this->get_grid_spacing( t ), valid ) )
            return NULL;

        // Box3 bounds = this->get_bounds( t );
        // bool enforceBounds = this->get_enforce_bounds();

        char* buffer = static_cast<char*>( alloca( expr.get_output_map().structure_size() ) );

        for( int i = 0, iEnd = points.Count(); i < iEnd; ++i )
            expr.eval_debug( buffer, frantic::max3d::from_max_t( *points[i] ), pResult->get_storage()[i] );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }

    return pResult.release();
}

// FPMixinInterface
FPInterfaceDesc* FieldMagma::GetDescByID( Interface_ID id ) {
    return static_cast<FieldMagmaDesc*>( GetFieldMagmaDesc() )->GetDescByID( id );
}

// Animatable
BaseInterface* FieldMagma::GetInterface( Interface_ID id ) {
    if( BaseInterface* result = IFieldMagma::GetInterface( id ) )
        return result;
    if( BaseInterface* result = ErrorReporter::GetInterface( id ) )
        return result;
    return EmberObjectBase::GetInterface( id );
}

// ReferenceTarget
void FieldMagma::BaseClone( RefTargetHandle from, RefTargetHandle to, RemapDir& remap ) {
    EmberObjectBase::BaseClone( from, to, remap );

    // Copy the currently set error callback. This may not be something we want though, since the callback can be
    // designed to store extra context data.
    static_cast<FieldMagma*>( to )->CopyCallback( static_cast<FieldMagma*>( from ) );
}

RefResult FieldMagma::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& /*partID*/,
                                        RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        if( message == REFMSG_CHANGE ) {
            int tabIndex = 0;
            ParamID param = m_pblock->LastNotifyParamID( tabIndex );

            switch( param ) {
            case kBoundsMin:
            case kBoundsMax:
            case kGridSpacing:
            case kMagmaHolder:
            case kEnforceBounds:
                this->InvalidateCache();
                NotifyDependents( FOREVER, PART_GEOM, REFMSG_CHANGE );

                return REF_STOP;
            case kViewportReduce:
            case kIconSize:
            case kViewportMaskChannel:
            case kViewportVectorChannel:
            case kViewportColorChannel:
            case kViewportVectorScale:
            case kViewportVectorNormalize:
            case kViewportEnabled:
            case kViewportBoundsEnabled:
                this->InvalidateCache(); // This is required or else we end up using the same cached values (since
                                         // EvalField is not run again). We could have validity for geom & display
                                         // separately...
                NotifyDependents( FOREVER, PART_DISPLAY, REFMSG_CHANGE );

                return REF_STOP;
            default:
                return REF_STOP;
            }
        }
    }
    return REF_SUCCEED;
}

// From BaseObject
#if MAX_VERSION_MAJOR < 24
TYPE_STRING_TYPE FieldMagma::GetObjectName() { return _T( FieldMagma_DISPLAYNAME ); }
#else
TYPE_STRING_TYPE FieldMagma::GetObjectName( bool localized ) { return _T( FieldMagma_DISPLAYNAME ); }
#endif

CreateMouseCallBack* FieldMagma::GetCreateMouseCallBack() {
    static frantic::max3d::ClickAndDragCreateCallBack theCallback;
    return &theCallback;
}

// From Object
void FieldMagma::InitNodeName( MSTR& s ) { s = _T( FieldMagma_DISPLAYNAME ); }

class ember_field_interface : public frantic::volumetrics::field_interface {
    ember::ember_compiler m_ec;

  public:
    ember_field_interface( BOOST_RV_REF( ember::ember_compiler ) emberCompiler )
        : m_ec( boost::move( emberCompiler ) ) {}

    virtual const frantic::channels::channel_map& get_channel_map() const { return m_ec.get_output_map(); }

    virtual bool evaluate_field( void* dest, const frantic::graphics::vector3f& pos ) const {
        m_ec.eval( static_cast<char*>( dest ), pos );
        return true;
    }
};

class ember_bounded_field_interface : public frantic::volumetrics::field_interface {
    ember::ember_compiler m_ec;
    frantic::graphics::boundbox3f m_bounds;

  public:
    ember_bounded_field_interface( BOOST_RV_REF( ember::ember_compiler ) emberCompiler,
                                   const frantic::graphics::boundbox3f& bounds )
        : m_ec( boost::move( emberCompiler ) )
        , m_bounds( bounds ) {}

    virtual const frantic::channels::channel_map& get_channel_map() const { return m_ec.get_output_map(); }

    virtual bool evaluate_field( void* dest, const frantic::graphics::vector3f& pos ) const {
        if( !m_bounds.contains( pos ) )
            return false;

        m_ec.eval( static_cast<char*>( dest ), pos );
        return true;
    }
};

namespace {
class FieldMagmaContext : public frantic::magma::magma_compiler_interface::context_base {
  public:
    RenderGlobalContext globContext;
    INode* node;
    frantic::graphics::boundbox3f bounds;
    float spacing;

  public:
    FieldMagmaContext( INode* _node = NULL )
        : node( _node ) {
        frantic::max3d::rendering::initialize_renderglobalcontext( globContext );
    }

    virtual ~FieldMagmaContext() {}

    virtual frantic::tstring get_name() const { return _T("FieldMagmaContext"); }

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
            return boost::any( static_cast<const RenderGlobalContext*>( &globContext ) );
        else if( name == _T("CurrentINode") )
            return boost::any( node );
        else if( name == _T("InWorldSpace") )
            return boost::any( node == NULL );
        else if( name == _T("Bounds") )
            return boost::any( bounds );
        else if( name == _T("Spacing") )
            return boost::any( spacing );
        return frantic::magma::magma_compiler_interface::context_base::get_property( name );
    }
};
} // namespace

bool FieldMagma::build_ember_expression( ember::ember_compiler& outExpr, TimeValue t, const Box3& bounds, float spacing,
                                         Interval& outValid ) {
    frantic::magma::max3d::IMagmaHolder* magmaHolder =
        frantic::magma::max3d::GetMagmaHolderInterface( this->get_magma_holder() );

    if( magmaHolder ) {
        boost::shared_ptr<frantic::magma::magma_interface> magma = magmaHolder->get_magma_interface();
        if( magma ) {
            magma->set_expression_id( kMagmaHolder );

            boost::shared_ptr<FieldMagmaContext> context( new FieldMagmaContext( NULL ) );

            context->globContext.time = t;
            context->globContext.camToWorld.IdentityMatrix();
            context->globContext.worldToCam.IdentityMatrix();
            context->bounds.set( frantic::max3d::from_max_t( bounds.pmin ), frantic::max3d::from_max_t( bounds.pmax ) );
            context->spacing = spacing;

            outExpr.set_abstract_syntax_tree( magma );
            outExpr.set_compilation_context( context );
            outExpr.build();

            outValid &= magmaHolder->get_validity( t );

            return true;
        }
    }

    return false;
}

void FieldMagma::EvalField( EmberPipeObject& objToPopulate, TimeValue t ) {
    Interval valid = FOREVER;

    Box3 bounds = this->get_bounds( t, valid );
    float s = std::max<float>( this->get_grid_spacing( t, valid ), 1e-5f );

    boost::shared_ptr<frantic::volumetrics::field_interface> result;

    try {
        ember::ember_compiler expr;

        if( this->build_ember_expression( expr, t, bounds, s, valid ) ) {
            if( this->get_enforce_bounds() ) {
                result.reset(
                    new ember_bounded_field_interface( boost::move( expr ), frantic::max3d::from_max_t( bounds ) ) );
            } else {
                result.reset( new ember_field_interface( boost::move( expr ) ) );
            }

            if( !valid.InInterval( t ) )
                valid.SetInstant( t );

            this->ClearError();
        }
    } catch( const frantic::magma::magma_exception& e ) {
        this->SetError( e );
        result.reset();
        bounds.Init();

        valid.SetInstant( t ); // TODO: On an error, should we be valid until something changes, or do we want a
                               // time-change to try the evaluation again?
    } catch( const std::exception& e ) {
        this->SetError( frantic::magma::magma_exception()
                        << frantic::magma::magma_exception::error_name( frantic::strings::to_tstring( e.what() ) ) );
        result.reset();
        bounds.Init();

        valid.SetInstant( t ); // TODO: On an error, should we be valid until something changes, or do we want a
                               // time-change to try the evaluation again?
    }

    if( !result ) {
        objToPopulate.SetEmpty();
    } else {
        objToPopulate.Set( create_field_task( result ), result->get_channel_map(), bounds, s );
        objToPopulate.SetChannelValidity( GEOM_CHAN_NUM, valid );
        objToPopulate.SetChannelValidity( DISP_ATTRIB_CHAN_NUM, FOREVER );

        if( this->get_viewport_enabled() ) {
            int vpReduce = std::max( 0, m_pblock->GetInt( kViewportReduce ) );

            const MCHAR* maskChannel = this->get_viewport_mask_channel();
            if( !maskChannel )
                maskChannel = _T("");

            const MCHAR* vectorChannel = this->get_viewport_vector_channel();
            if( !vectorChannel )
                vectorChannel = _T("");

            const MCHAR* colorChannel = this->get_viewport_color_channel();
            if( !colorChannel )
                colorChannel = _T("");

            objToPopulate.SetViewportSpacingNatural( vpReduce );
            objToPopulate.SetViewportScalarChannel( maskChannel );
            objToPopulate.SetViewportVectorChannel( vectorChannel );
            objToPopulate.SetViewportColorChannel( colorChannel );

            if( !vectorChannel || vectorChannel[0] == _T( '\0' ) ) {
                /*if( !maskChannel || maskChannel[0] == _T('\0') ){
                        objToPopulate.SetViewportDisplayNone();
                }else{*/
                objToPopulate.SetViewportDisplayDots();
                //}
            } else {
                float vectorScale = this->get_viewport_vector_scale( t );
                bool vectorNormalize = this->get_viewport_vector_normalize();

                objToPopulate.SetViewportDisplayLines( vectorNormalize, vectorScale );
            }
        } else {
            objToPopulate.SetViewportDisplayNone();
        }

        objToPopulate.SetViewportDisplayBounds( this->get_viewport_bounds_enabled() );
    }

    float iconScale = frantic::max3d::get<float>( m_pblock, kIconSize );
    if( iconScale > 0.f ) {
        // NOTE: A static Mesh object caused intermittent access violations in Max 2015 (and 2014?)
#if MAX_VERSION_MAJOR >= 25
        static MaxSDK::SharedMeshPtr theIcon;
#else
        static Mesh* theIcon = NULL;
#endif
        static MaxSDK::Graphics::RenderItemHandleArray theIconMeshes;

        if( !theIcon ) {
#if MAX_VERSION_MAJOR >= 25
            theIcon = new MaxSDK::SharedMesh();
            BuildMesh_Text_EMBER_Field( theIcon->GetMesh() );
#else
            theIcon = CreateNewMesh();
            theIcon->SetStaticMesh( true );
            BuildMesh_Text_EMBER_Field( *theIcon );
#endif
        }

        Matrix3 iconTM;
        iconTM.SetScale( Point3( iconScale, iconScale, iconScale ) );

        objToPopulate.SetViewportIcon( theIconMeshes, theIcon, iconTM );
    }
}

void FieldMagma::SetIconScale( float scale ) { frantic::max3d::set<float>( m_pblock, kIconSize, 0, scale ); }

ClassDesc2* FieldMagma::GetClassDesc() { return GetFieldMagmaDesc(); }

} // namespace max3d
} // namespace ember

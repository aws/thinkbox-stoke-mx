// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/EmberPipeObject.hpp>

#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>

#undef X_AXIS
#undef Y_AXIS
#undef Z_AXIS

#include <ember/data_types.hpp>
#include <ember/openvdb.hpp>

#pragma warning( push, 3 )
#pragma warning( disable : 4503 4244 4267 4305 4701 )
#include <openvdb/openvdb.h>
#include <openvdb/tools/GridOperators.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/MeshToVolume.h>
#pragma warning( pop )

#pragma warning(                                                                                                       \
    disable : 4503 ) // OpenVDB has plentry of 'decorated name length exceeded, name was truncated' warnings

#include <boost/function.hpp>

#pragma warning( push, 3 )
#include <maxversion.h>
#include <modstack.h>
#pragma warning( pop )

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#pragma warning( push, 3 )
#include <maxscript/mxsplugin/mxsPlugin.h>
#pragma warning( pop )

inline ReferenceTarget* get_msplugin_delegate( ReferenceTarget* rtarg ) {
    if( MSPlugin* pMXSPlugin = static_cast<MSPlugin*>( rtarg->GetInterface( I_MAXSCRIPTPLUGIN ) ) )
        return pMXSPlugin->get_delegate();
    return NULL;
}

#define EmberFromMesh_CLASSID Class_ID( 0x58914dbf, 0x1b96639b )
#define EmberFromMesh_NAME "StokeFieldFromMesh"
#define EmberFromMesh_DISPLAYNAME "Stoke Field From Mesh"
#define EmberFromMesh_VERSION 1

extern void BuildMesh_MeshToField( Mesh& );

namespace ember {
namespace max3d {

enum { kSpacing, kOutputType, kIncludeCPT, kBandWidth, kIconSize, kViewportReduce };

class EmberFromMesh : public frantic::max3d::GenericReferenceTarget<OSModifier, EmberFromMesh> {
  public:
    struct output_type {
        enum option { kLevelset, kFogVolume };
    };

  public:
    EmberFromMesh( BOOL loading );

    // Reference Maker
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

    // BaseObject
    virtual CreateMouseCallBack* GetCreateMouseCallBack( void );

#if MAX_VERSION_MAJOR < 24
    virtual TYPE_STRING_TYPE GetObjectName();
#else
    virtual TYPE_STRING_TYPE GetObjectName( bool localized ) const;
#endif

    // Modifier
    virtual Interval LocalValidity( TimeValue t );
    virtual ChannelMask ChannelsUsed();
    virtual ChannelMask ChannelsChanged();
    virtual Class_ID InputType();
    virtual void ModifyObject( TimeValue t, ModContext& mc, ObjectState* os, INode* node );

  protected:
    virtual ClassDesc2* GetClassDesc();
};

class EmberFromMeshDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_paramDesc;

    friend class EmberFromMesh;

  public:
    EmberFromMeshDesc();

    int IsPublic() { return FALSE; }
    void* Create( BOOL loading ) { return new EmberFromMesh( loading ); }
    const TCHAR* ClassName() { return _T( EmberFromMesh_DISPLAYNAME ); }
    SClass_ID SuperClassID() { return OSM_CLASS_ID; }
    Class_ID ClassID() { return EmberFromMesh_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() {
        return _T( EmberFromMesh_NAME );
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( EmberFromMesh_DISPLAYNAME ); }
#endif
};

enum { kMainPBlock };

EmberFromMeshDesc::EmberFromMeshDesc()
    : m_paramDesc( kMainPBlock, _T("Parameters"), 0, EmberFromMesh_VERSION ) {
    m_paramDesc.OwnerRefNum( 0 );
    m_paramDesc.OwnerClassDesc( this );

    m_paramDesc.Parameter<float>( kSpacing, _T("Spacing"), 0, true ).DefaultValue( 1.f ).Range( 1e-4f, FLT_MAX );

    m_paramDesc.Parameter<int>( kOutputType, _T("OutputType"), 0 )
        .DefaultValue( EmberFromMesh::output_type::kLevelset );

    m_paramDesc.Parameter<bool>( kIncludeCPT, _T("IncludeCPT"), 0 ).DefaultValue( false );

    m_paramDesc.Parameter<float>( kBandWidth, _T("BandWidthVoxels"), 0 ).Range( 0.f, FLT_MAX ).DefaultValue( 3.f );

    m_paramDesc.Parameter<float>( kIconSize, _T("IconSize"), 0 ).Range( 0.f, FLT_MAX ).DefaultValue( 30.f );

    m_paramDesc.Parameter<int>( kViewportReduce, _T("ViewportReduce"), 0 ).Range( 0, INT_MAX ).DefaultValue( 0 );
}

ClassDesc2* GetEmberFromMeshDesc() {
    static EmberFromMeshDesc theEmberFromMeshDesc;
    return &theEmberFromMeshDesc;
}

EmberFromMesh::EmberFromMesh( BOOL /*loading*/ ) { GetEmberFromMeshDesc()->MakeAutoParamBlocks( this ); }

RefResult EmberFromMesh::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& partID,
                                           RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        if( message == REFMSG_CHANGE )
            return REF_SUCCEED;
    }
    return REF_DONTCARE;
}

CreateMouseCallBack* EmberFromMesh::GetCreateMouseCallBack( void ) { return NULL; }

#if MAX_VERSION_MAJOR < 24
TYPE_STRING_TYPE EmberFromMesh::GetObjectName() { return _T( EmberFromMesh_DISPLAYNAME ); }
#else
TYPE_STRING_TYPE EmberFromMesh::GetObjectName( bool localized ) const { return _T( EmberFromMesh_DISPLAYNAME ); }
#endif

Interval EmberFromMesh::LocalValidity( TimeValue /*t*/ ) { return FOREVER; }

ChannelMask EmberFromMesh::ChannelsUsed() { return TOPO_CHANNEL | GEOM_CHANNEL /*|TEXMAP_CHANNEL|VERTCOLOR_CHANNEL*/; }

ChannelMask EmberFromMesh::ChannelsChanged() { return OBJ_CHANNELS; }

Class_ID EmberFromMesh::InputType() { return triObjectClassID; }

namespace {
class levelset_from_mesh_op {
    BOOST_MOVABLE_BUT_NOT_COPYABLE( levelset_from_mesh_op );
    std::vector<openvdb::Vec3s> m_points;
    std::vector<openvdb::Vec3I> m_tris;
    float m_spacing;
    float m_bandWidth;

    openvdb::math::Mat4d m_tm;

  public:
    levelset_from_mesh_op( const Mesh& m, /*const Matrix3& tm, */ float spacing, float bandWidth )
        : m_tm()
    /*tm.GetAddr()[0][0], tm.GetAddr()[0][1], tm.GetAddr()[0][2], 0.f,
    tm.GetAddr()[1][0], tm.GetAddr()[1][1], tm.GetAddr()[1][2], 0.f,
    tm.GetAddr()[2][0], tm.GetAddr()[2][1], tm.GetAddr()[2][2], 0.f,
    tm.GetAddr()[3][0], tm.GetAddr()[3][1], tm.GetAddr()[3][2], 1.f
)*/
    {
        m_points.reserve( static_cast<std::size_t>( m.numVerts ) );
        m_tris.reserve( static_cast<std::size_t>( m.numFaces ) );

        for( std::size_t i = 0, iEnd = static_cast<std::size_t>( m.numVerts ); i < iEnd; ++i )
            m_points.push_back( openvdb::Vec3s( m.verts[i].x, m.verts[i].y, m.verts[i].z ) );

        for( std::size_t i = 0, iEnd = static_cast<std::size_t>( m.numFaces ); i < iEnd; ++i )
            m_tris.push_back( openvdb::Vec3I( m.faces[i].v[0], m.faces[i].v[1], m.faces[i].v[2] ) );

        m_spacing = spacing;
        m_bandWidth = bandWidth;

        // m_tm.preScale( openvdb::math::Vec3d(spacing) );
        // m_tm.preTranslate( openvdb::math::Vec3d(0.5) );
    }

    levelset_from_mesh_op( BOOST_RV_REF( levelset_from_mesh_op ) rhs ) {
        m_points.swap( rhs.m_points );
        m_tris.swap( rhs.m_tris );
        m_spacing = rhs.m_spacing;
        m_bandWidth = rhs.m_bandWidth;
        m_tm = rhs.m_tm;
    }

    levelset_from_mesh_op& operator=( BOOST_RV_REF( levelset_from_mesh_op ) rhs ) {
        m_points.swap( rhs.m_points );
        m_tris.swap( rhs.m_tris );
        m_spacing = rhs.m_spacing;
        m_bandWidth = rhs.m_bandWidth;
        m_tm = rhs.m_tm;
        return *this;
    }

    openvdb::FloatGrid::Ptr operator()() const {
        openvdb::math::Transform::Ptr xform = openvdb::math::Transform::createLinearTransform( m_spacing );

        xform->postTranslate( openvdb::Vec3d( m_spacing / 2.f ) );

        openvdb::FloatGrid::Ptr result =
            openvdb::tools::meshToLevelSet<openvdb::FloatGrid>( *xform, m_points, m_tris, m_bandWidth );

        /*if( frantic::logging::is_logging_debug() ){
                openvdb::math::CoordBBox bounds = result->evalActiveVoxelBoundingBox();

                frantic::logging::debug << _T("[") <<
                        bounds.min().x() << _T(", ") << bounds.min().y() << _T(", ") << bounds.min().z() << _T("] - [")
        << bounds.max().x() << _T(", ") << bounds.max().y() << _T(", ") << bounds.max().z() << _T("]") << std::endl;
        }

        result->setTransform( openvdb::math::Transform::createLinearTransform(
        xform->baseMap()->getAffineMap()->getConstMat4() * m_tm ) );*/

        return result;
    }
};

class levelset_field_op {
    BOOST_MOVABLE_BUT_NOT_COPYABLE( levelset_field_op );
    levelset_from_mesh_op m_impl;
    bool m_includeCPT;

  public:
    levelset_field_op( const Mesh& m, /*const Matrix3& toWorld, */ float spacing, float bandWidth, bool includeCPT )
        : m_impl( m, /*toWorld, */ spacing, bandWidth )
        , m_includeCPT( includeCPT ) {}

    levelset_field_op( BOOST_RV_REF( levelset_field_op ) rhs )
        : m_impl( boost::move( rhs.m_impl ) )
        , m_includeCPT( rhs.m_includeCPT ) {}

    levelset_field_op& operator=( BOOST_RV_REF( levelset_field_op ) rhs ) {
        m_impl = boost::move( rhs.m_impl );
        m_includeCPT = rhs.m_includeCPT;
    }

    boost::shared_ptr<frantic::volumetrics::field_interface>
    operator()( frantic::logging::progress_logger& progress ) const {
        openvdb::FloatGrid::Ptr pGrid = m_impl();

        pGrid->setName( "SignedDistance" );

        if( !m_includeCPT )
            return boost::shared_ptr<frantic::volumetrics::field_interface>(
                ember::create_field_interface( pGrid ).release() );

        progress_logger_interrupter adapter( progress );

        openvdb::GridPtrVec grids;
        grids.push_back( pGrid );
        grids.push_back( openvdb::tools::cpt( *pGrid, true, &adapter ) );
        grids.back()->setName( "ClosestPoint" );

        return boost::shared_ptr<frantic::volumetrics::field_interface>(
            ember::create_field_interface( grids ).release() );
    }
};

class fog_volume_field_op {
    BOOST_MOVABLE_BUT_NOT_COPYABLE( fog_volume_field_op );
    levelset_from_mesh_op m_impl;

  public:
    fog_volume_field_op( const Mesh& m, /*const Matrix3& toWorld, */ float spacing, float bandWidth )
        : m_impl( m, /*toWorld, */ spacing, bandWidth ) {}

    fog_volume_field_op( BOOST_RV_REF( fog_volume_field_op ) rhs )
        : m_impl( boost::move( rhs.m_impl ) ) {}

    fog_volume_field_op& operator=( BOOST_RV_REF( fog_volume_field_op ) rhs ) { m_impl = boost::move( rhs.m_impl ); }

    boost::shared_ptr<frantic::volumetrics::field_interface>
    operator()( frantic::logging::progress_logger& progress ) const {
        progress.check_for_abort();

        openvdb::FloatGrid::Ptr pGrid = m_impl();

        progress.check_for_abort();

        openvdb::tools::sdfToFogVolume( *pGrid );

        pGrid->setName( "Density" );

        return boost::shared_ptr<frantic::volumetrics::field_interface>(
            ember::create_field_interface( pGrid ).release() );
    }
};
} // namespace

namespace {
class EnumNodes : public DependentEnumProc {
    Modifier* m_mod;
    ModContext* m_modCtx;
    INode* m_result;

  private:
    bool find_target( INode* pNode ) {
        Object* pObj = pNode->GetObjectRef();
        if( !pObj )
            return false;

        while( pObj->ClassID() == derivObjClassID ) {
            IDerivedObject* pDerived = static_cast<IDerivedObject*>( pObj );

            for( int i = 0, iEnd = pDerived->NumModifiers(); i < iEnd; ++i ) {
                Modifier* pMod = pDerived->GetModifier( i );
                ModContext* pModCtx = pDerived->GetModContext( i );

                if( m_modCtx == pModCtx &&
                    ( m_mod == pMod || m_mod == static_cast<Modifier*>( get_msplugin_delegate( pMod ) ) ) )
                    return true;
            }

            pObj = pDerived->GetObjRef();
            if( !pObj )
                return false;
        }

        return false;
    }

  public:
    EnumNodes( Modifier* mod, ModContext* modCtx )
        : m_mod( mod )
        , m_modCtx( modCtx )
        , m_result( NULL ) {}

    INode* get_result() { return m_result; }

    virtual int proc( ReferenceMaker* rmaker ) {
        if( INode* pNode = static_cast<INode*>( rmaker->GetInterface( INODE_INTERFACE ) ) ) {
            if( find_target( pNode ) ) {
                m_result = pNode;

                return DEP_ENUM_HALT;
            }

            return DEP_ENUM_SKIP;
        }

        return DEP_ENUM_CONTINUE;
    }
};
} // namespace

void EmberFromMesh::ModifyObject( TimeValue t, ModContext& mc, ObjectState* os, INode* /*node*/ ) {
    if( !os || !os->obj || !os->obj->CanConvertToType( triObjectClassID ) )
        return;

    // INode* node = GetCOREInterface7()->FindNodeFromBaseObject( this );
    // EnumNodes enumNodes( this, &mc );

    // this->DoEnumDependents( &enumNodes );

    // INode* node = enumNodes.get_result();

    struct auto_obj {
        Object* pObj;
        BOOL needsDelete;
        ~auto_obj() {
            if( pObj && needsDelete )
                pObj->MaybeAutoDelete();
        }
    } objHolder = { os->obj->ConvertToType( t, triObjectClassID ), FALSE };
    objHolder.needsDelete = ( objHolder.pObj != os->obj );

    TriObject* pTriObj = static_cast<TriObject*>( objHolder.pObj );
    Mesh& m = pTriObj->GetMesh();

    Interval valid = FOREVER;
    float spacing = 1.f;
    float bandWidth = 3.f;

    // Matrix3 tm( TRUE );
    // if( node )
    //	tm = node->GetNodeTM( t, &valid );

    Box3 bounds = m.getBoundingBox( /*&tm*/ );

    m_pblock->GetValue( kSpacing, t, spacing, valid );
    m_pblock->GetValue( kBandWidth, t, bandWidth, valid );

    bandWidth = std::max( bandWidth, static_cast<float>( openvdb::LEVEL_SET_HALF_WIDTH ) ) * spacing;

    // Enlarge the mesh bounding box since the field has an inside & outside portion.
    bounds.EnlargeBy( bandWidth );

    std::unique_ptr<EmberPipeObject> newObj( new EmberPipeObject );

    int viewportReduce = std::max( 0, frantic::max3d::get<int>( m_pblock, kViewportReduce ) );

    newObj->SetViewportSpacingNatural( viewportReduce );

    frantic::channels::channel_map map;

    future_field_base::ptr_type pResult;

    switch( m_pblock->GetInt( kOutputType ) ) {
    case output_type::kLevelset: {
        bool includeCPT = frantic::max3d::get<bool>( m_pblock, kIncludeCPT );

        levelset_field_op theOp( m, /*tm, */ spacing, bandWidth, includeCPT );

        pResult = create_field_task( boost::move( theOp ) );

        map.define_channel<float>( _T("SignedDistance") );
        newObj->SetViewportScalarChannel( _T("SignedDistance") );

        if( includeCPT ) {
            map.define_channel<ember::vec3>( _T("ClosestPoint") );
            newObj->SetViewportVectorChannel( _T("ClosestPoint") );
        }

        newObj->SetViewportDisplayDots( -bandWidth, bandWidth );
    } break;

    case output_type::kFogVolume: {
        fog_volume_field_op theOp( m, /*tm, */ spacing, bandWidth );

        pResult = create_field_task( boost::move( theOp ) );

        map.define_channel<float>( _T("Density") );

        newObj->SetViewportScalarChannel( _T("Density") );
        newObj->SetViewportDisplayDots( 0, std::numeric_limits<float>::max() );
    } break;
    }

    map.end_channel_definition();

    // Include the incoming mesh's validity.
    valid &= pTriObj->ChannelValidity( t, GEOM_CHAN_NUM );
    valid &= pTriObj->ChannelValidity( t, TOPO_CHAN_NUM );

    newObj->Set( pResult, map, bounds, spacing, false );
    newObj->SetChannelValidity( GEOM_CHAN_NUM, valid );
    newObj->SetChannelValidity( DISP_ATTRIB_CHAN_NUM, FOREVER );

    float iconScale = frantic::max3d::get<float>( m_pblock, kIconSize );
    if( iconScale > 0.f ) {
#if MAX_VERSION_MAJOR >= 25
        static MaxSDK::SharedMeshPtr theIcon;
#else
        static Mesh* theIcon = NULL;
#endif
        static MaxSDK::Graphics::RenderItemHandleArray theIconMeshes;

        if( !theIcon ) {
#if MAX_VERSION_MAJOR >= 25
            theIcon = new MaxSDK::SharedMesh();
            BuildMesh_MeshToField( theIcon->GetMesh() );
#else
            theIcon = CreateNewMesh();
            BuildMesh_MeshToField( *theIcon );
#endif
        }

        Matrix3 iconTM;
        iconTM.SetScale( Point3( iconScale, iconScale, iconScale ) );

        newObj->SetViewportIcon( theIconMeshes, theIcon, iconTM );
    }

    os->obj = newObj.release();
}

ClassDesc2* EmberFromMesh::GetClassDesc() { return GetEmberFromMeshDesc(); }

} // namespace max3d
} // namespace ember

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/EmberPipeObject.hpp>
#if MAX_VERSION_MAJOR >= 17
#include <frantic/max3d/viewport/ParticleRenderItem.hpp>
#endif

#include <frantic/max3d/max_utility.hpp>

#include <ember/data_types.hpp>
#include <ember/transformed_field.hpp>

#include <frantic/logging/logging_level.hpp>
#include <frantic/win32/invoke_queue.hpp>

#if MAX_VERSION_MAJOR >= 17
#include <frantic/max3d/viewport/BoxRenderItem.hpp>
#include <frantic/max3d/viewport/DecoratedMeshEdgeRenderItem.hpp>

#pragma warning( push, 3 )
#include <Graphics/CustomRenderItemHandle.h>
#include <Graphics/GenerateMeshRenderItemsContext.h>
#include <Graphics/IDisplayManager.h>
#include <Graphics/IVirtualDevice.h>
#include <Graphics/Matrix44.h>
#include <Graphics/RenderItemHandleArray.h>
#include <Graphics/RenderItemHandleDecorator.h>
#include <Graphics/RenderNodeHandle.h>
#include <Graphics/SolidColorMaterialHandle.h>
#include <Graphics/UpdateDisplayContext.h>
#include <Graphics/Utilities/MeshEdgeRenderItem.h>
#include <Graphics/VertexColorMaterialHandle.h>
#pragma warning( pop )

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#pragma comment( lib, "DefaultRenderItems.lib" )
#pragma comment( lib, "DataBridge.lib" )
#pragma comment( lib, "GraphicsDriver.lib" )
#pragma comment( lib, "GraphicsUtility.lib" )

#endif

#include <boost/scope_exit.hpp>

#include <cstdio>

#include <frantic/misc/hybrid_assert.hpp>

frantic::win32::invoke_queue& GetMainThreadDispatcher();

namespace ember {
namespace max3d {

std::size_t g_maxMarkerCount = 250000;

void SetMaxMarkerCount( std::size_t count ) { g_maxMarkerCount = std::max<std::size_t>( 1, count ); }

std::size_t GetMaxMarkerCount() { return g_maxMarkerCount; }

template <class Callable>
inline void InvokeInMainThread( const Callable& callable ) {
    frantic::win32::invoke_queue& queue = GetMainThreadDispatcher();
    if( queue.is_owning_thread() ) {
        callable();
    } else {
        queue.invoke( callable );
    }
}

class EmberPipeObjectDesc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new EmberPipeObject; }
    const TCHAR* ClassName() { return _T("StokeObject"); }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return EmberPipeObject_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() { return _T("StokeObject"); } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }               // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T("StokeObject"); }
#endif
};

ClassDesc2* GetEmberPipeObjectDesc() {
    static EmberPipeObjectDesc theEmberPipeObjectDesc;
    return &theEmberPipeObjectDesc;
}

EmberPipeObject::EmberPipeObject()
    : m_vpUpdatePending( false )
    , m_geomValid( FOREVER )
    , m_chanValid( ~0 )
#if MAX_VERSION_MAJOR >= 17
    , m_wasRealized( false )
#endif
{
    m_spacing = 0.f;
    m_inWorldSpace = true;
    m_vpDataValid = false;
}

EmberPipeObject::~EmberPipeObject() {
    // If the viewport is waiting to be updated, interrupt and detach the thread since we don't care anymore.
    if( m_vpUpdateThread.joinable() ) {
        m_vpUpdateThread.interrupt();
        m_vpUpdateThread.detach();
    }
}

#pragma warning( push )
#pragma warning( disable : 4706 )
boost::shared_ptr<frantic::volumetrics::field_interface> EmberPipeObject::try_get_field( bool& outIsReady ) const {
    boost::shared_ptr<frantic::volumetrics::field_interface> pResult;

    if( ( outIsReady = m_fieldData->is_ready() ) )
        pResult = m_fieldData->get();

    return pResult;
}
#pragma warning( pop )

namespace {
void do_viewport_update() {
    Interface* ip = GetCOREInterface();
    ip->RedrawViews(
        ip->GetTime() ); // Forcing all views to be redrawn. I couldn't find a nicer way to only invalidate the nodes
                         // connected to this modifier (I suppose we could traverse the ref heirarchy).
}

// TODO: This might prevent the task from cancelling properly.
void update_viewport_when_ready( future_field_base::ptr_type pFuture ) {
    try {
        pFuture->get(); // Blocks until completed or cancelled

        // Queue a screen redraw.
        InvokeInMainThread( &do_viewport_update );
    } catch( frantic::logging::progress_cancel_exception& ) {
        FF_LOG( debug ) << _T( "Task cancelled and noted in viewport update thread" ) << std::endl;
    } catch( const boost::thread_interrupted& ) {
        FF_LOG( debug ) << _T( "Viewport update cancelled" ) << std::endl;
        throw; // Must propogate the interruption exception.
    } catch( ... ) {
        FF_LOG( error ) << frantic::strings::to_tstring( boost::current_exception_diagnostic_information() )
                        << std::endl;
    }
}
} // namespace

void EmberPipeObject::update_accessors() {
    try {
        m_viewportData.m_floatAccessor.reset( 1.f );
        m_viewportData.m_vecAccessor.reset();
        m_viewportData.m_colorAccessor.reset();
        m_viewportData.m_scalarColorAccessor.reset();

        // Sometimes we have only the viewport data in *this, without the
        if( m_fieldMap.channel_definition_complete() ) {
            if( m_fieldMap.has_channel( m_viewportData.m_scalarChannel ) &&
                m_fieldMap[m_viewportData.m_scalarChannel].arity() == 1 &&
                frantic::channels::is_channel_data_type_float(
                    m_fieldMap[m_viewportData.m_scalarChannel].data_type() ) )
                m_viewportData.m_floatAccessor = m_fieldMap.get_cvt_accessor<float>( m_viewportData.m_scalarChannel );

            if( m_fieldMap.has_channel( m_viewportData.m_vectorChannel ) &&
                m_fieldMap[m_viewportData.m_vectorChannel].arity() == 3 &&
                frantic::channels::is_channel_data_type_float(
                    m_fieldMap[m_viewportData.m_vectorChannel].data_type() ) )
                m_viewportData.m_vecAccessor =
                    m_fieldMap.get_cvt_accessor<frantic::graphics::vector3f>( m_viewportData.m_vectorChannel );

            // TODO: Should we support integer color too?
            if( m_fieldMap.has_channel( m_viewportData.m_colorChannel ) &&
                frantic::channels::is_channel_data_type_float(
                    m_fieldMap[m_viewportData.m_colorChannel].data_type() ) ) {
                if( m_fieldMap[m_viewportData.m_colorChannel].arity() == 3 )
                    m_viewportData.m_colorAccessor =
                        m_fieldMap.get_cvt_accessor<frantic::graphics::color3f>( m_viewportData.m_colorChannel );
                else if( m_fieldMap[m_viewportData.m_colorChannel].arity() == 1 )
                    m_viewportData.m_scalarColorAccessor =
                        m_fieldMap.get_cvt_accessor<float>( m_viewportData.m_colorChannel );
            }
        }

        if( !m_vpUpdatePending ) {
            m_vpUpdatePending = true;
            m_vpUpdateThread = boost::move( boost::thread::thread( &update_viewport_when_ready, m_fieldData ) );
            m_vpUpdateThread.detach();
        }

    } catch( const std::exception& e ) {
        FF_LOG( error ) << e.what() << std::endl;
        m_viewportData.m_displayMode = display_mode::kDisplayNone;
    }
}

namespace {
class default_field_interface : public frantic::volumetrics::field_interface {
    boost::scoped_array<char> m_data;
    frantic::channels::channel_map m_map;

  public:
    default_field_interface( const frantic::channels::channel_map& map, const void* data )
        : m_map( map )
        , m_data( new char[map.structure_size()] ) {
        memcpy( m_data.get(), data, map.structure_size() );
    }

    virtual const frantic::channels::channel_map& get_channel_map() const { return m_map; }

    virtual bool evaluate_field( void* dest, const frantic::graphics::vector3f& /*pos*/ ) const {
        memcpy( dest, m_data.get(), m_map.structure_size() );
        return true;
    }
};
} // namespace

void EmberPipeObject::SetEmpty() {
    frantic::channels::channel_map map;
    map.define_channel<float>( _T("Density") );
    map.end_channel_definition();

    float defaultDensity = 0.f;

    boost::shared_ptr<frantic::volumetrics::field_interface> pDefaultField(
        new default_field_interface( map, &defaultDensity ) );

    this->Set( create_field_task( pDefaultField ), map, Box3( Point3( 0, 0, 0 ), Point3( 0, 0, 0 ) ), 1.f );
}
//
// void EmberPipeObject::SetData( boost::shared_ptr<frantic::volumetrics::field_interface> pField, const Box3& bounds,
// float spacing ){ 	m_fieldData = pField; 	m_futureField = boost::make_shared_future( pField ); 	m_bounds = bounds;
//	m_spacing = spacing;
//	m_fieldMap = pField->get_channel_map();
//
//	this->update_accessors();
//}

void EmberPipeObject::Set( future_field_base::ptr_type fieldData, const frantic::channels::channel_map& map,
                           const Box3& bounds, float spacing, bool inWorldSpace ) {
    m_fieldData = fieldData;
    m_bounds = bounds;
    m_spacing = spacing;
    m_fieldMap = map;
    m_inWorldSpace = inWorldSpace;

    this->update_accessors();

    /*if( m_vpUpdateThread.joinable() ){
            m_vpUpdateThread.detach();
            m_vpUpdatePending = false;
    }*/
}

#if MAX_VERSION_MAJOR >= 25
void EmberPipeObject::SetViewportIcon( const MaxSDK::Graphics::RenderItemHandleArray& hIconMeshes,
                                       MaxSDK::SharedMeshPtr pMesh, const Matrix3& iconTM ) {
#else
void EmberPipeObject::SetViewportIcon( const MaxSDK::Graphics::RenderItemHandleArray& hIconMeshes, Mesh* pMesh,
                                       const Matrix3& iconTM ) {
#endif
    m_viewportData.m_pIcon = pMesh;
    m_viewportData.m_hIconMeshes = hIconMeshes;
    m_viewportData.m_iconTM = iconTM;
}

void EmberPipeObject::SetViewportResolution( int x, int y, int z ) {
    m_viewportData.m_spacingMode = spacing_mode::kSpacingConstant;
    m_viewportData.m_res[0] = x;
    m_viewportData.m_res[1] = y;
    m_viewportData.m_res[2] = z;
}

void EmberPipeObject::SetViewportSpacing( float x, float y, float z ) {
    m_viewportData.m_spacingMode = spacing_mode::kSpacingDynamic;
    m_viewportData.m_spacing[0] = x;
    m_viewportData.m_spacing[1] = y;
    m_viewportData.m_spacing[2] = z;
}

void EmberPipeObject::SetViewportSpacingNatural( int reduce ) {
    m_viewportData.m_spacingMode = spacing_mode::kSpacingNatural;
    m_viewportData.m_reduce = reduce;
}

void EmberPipeObject::SetViewportScalarChannel( const frantic::tstring& channel ) {
    m_viewportData.m_scalarChannel = channel;
    m_viewportData.m_floatAccessor.reset( 1.f );
    if( m_fieldMap.has_channel( channel ) && m_fieldMap[channel].arity() == 1 &&
        frantic::channels::is_channel_data_type_float( m_fieldMap[channel].data_type() ) )
        m_viewportData.m_floatAccessor = m_fieldMap.get_cvt_accessor<float>( channel );
}

void EmberPipeObject::SetViewportVectorChannel( const frantic::tstring& channel ) {
    m_viewportData.m_vectorChannel = channel;
    m_viewportData.m_vecAccessor.reset();
    if( m_fieldMap.has_channel( channel ) && m_fieldMap[channel].arity() == 3 &&
        frantic::channels::is_channel_data_type_float( m_fieldMap[channel].data_type() ) )
        m_viewportData.m_vecAccessor = m_fieldMap.get_cvt_accessor<frantic::graphics::vector3f>( channel );
}

void EmberPipeObject::SetViewportColorChannel( const frantic::tstring& channel ) {
    m_viewportData.m_colorChannel = channel;
    m_viewportData.m_colorAccessor.reset();
    if( m_fieldMap.has_channel( channel ) &&
        frantic::channels::is_channel_data_type_float( m_fieldMap[channel].data_type() ) ) {
        if( m_fieldMap[channel].arity() == 3 )
            m_viewportData.m_colorAccessor = m_fieldMap.get_cvt_accessor<frantic::graphics::color3f>( channel );
        else if( m_fieldMap[channel].arity() == 1 )
            m_viewportData.m_scalarColorAccessor = m_fieldMap.get_cvt_accessor<float>( channel );
    }
}

void EmberPipeObject::SetViewportDisplayNone() { m_viewportData.m_displayMode = display_mode::kDisplayNone; }

void EmberPipeObject::SetViewportDisplayDots( float minVal, float maxVal, float dotSize ) {
    m_viewportData.m_displayMode = display_mode::kDisplayDots;
    // Set the scalar ranges only if they aren't NaN.
    if( minVal == minVal )
        m_viewportData.m_scalarMin = minVal;
    if( maxVal == maxVal )
        m_viewportData.m_scalarMax = maxVal;
    if( dotSize == dotSize )
        m_viewportData.m_dotSize = std::max( 0.f, dotSize );
}

void EmberPipeObject::SetViewportDisplayLines( bool normalize, float scale ) {
    m_viewportData.m_displayMode = display_mode::kDisplayLines;
    m_viewportData.m_normalizeVectors = normalize;
    m_viewportData.m_vectorScale = scale;
}

void EmberPipeObject::SetViewportDisplayBounds( bool enabled ) { m_viewportData.m_drawBounds = enabled; }

const Box3& EmberPipeObject::GetBounds() const { return m_bounds; }

float EmberPipeObject::GetDefaultSpacing() const { return m_spacing; }

const frantic::channels::channel_map& EmberPipeObject::GetChannels() const { return m_fieldMap; }

future_field_base::ptr_type EmberPipeObject::GetFuture() const { return m_fieldData; }

bool EmberPipeObject::GetInWorldSpace() const { return m_inWorldSpace; }

boost::shared_ptr<frantic::volumetrics::field_interface> EmberPipeObject::create_field( INode* pNode, TimeValue t,
                                                                                        Interval& valid ) {
    boost::shared_ptr<frantic::volumetrics::field_interface> result = m_fieldData->get();

    valid &= m_geomValid;

    if( !m_inWorldSpace ) {
        if( pNode != NULL ) {
            // Get the node's transform & validity information.
            // TODO: Get object-offset transform info too! We can't use GetObjectTM on the node since this function can
            // be called while evaluating the node's geometry pipeline (right?).
            frantic::graphics::transform4f tm = frantic::max3d::from_max_t( pNode->GetNodeTM( t, &valid ) );

            // TODO: Some grids probably can have the transform directly applied (if they are uniquely owned though,
            // which never actually happens).
            if( !tm.is_identity() )
                result = boost::make_shared<ember::transformed_field>( result, tm );
        } else {
            FF_LOG( warning ) << _T("EmberPipeObject::create_field() Called with a NULL node") << std::endl;
        }
    }

    return result;
}

//#pragma warning( push )
//#pragma warning( disable : 4706 )
// boost::shared_ptr<frantic::volumetrics::field_interface> EmberPipeObject::try_create_field( INode* pNode, TimeValue
// t, bool& outIsReady ){ 	boost::shared_ptr<frantic::volumetrics::field_interface> pResult;
//
//	if( (outIsReady = m_fieldData->is_ready()) ){
//		pResult = m_fieldData->get();
//	}
//
//	return pResult;
//}
//#pragma warning( pop )
//
// future_field_base::ptr_type EmberPipeObject::create_field_async( INode* pNode, TimeValue t ){
//	return m_fieldData;
//}

// From Animatable
BaseInterface* EmberPipeObject::GetInterface( Interface_ID id ) {
    // if( id == IADD_RENDERITEMS_HELPER_INTERFACE_ID )
    //	return static_cast<MaxSDK::Graphics::IAddRenderItemsHelper*>( this );
    if( BaseInterface* bi = frantic::max3d::volumetrics::IEmberField::GetInterface( id ) )
        return bi;
    return frantic::max3d::GenericReferenceTarget<GeomObject, EmberPipeObject>::GetInterface( id );
}

// ReferenceMaker
RefResult EmberPipeObject::NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                             RefMessage message, BOOL propagate ) {
    return REF_SUCCEED;
}

// ReferenceTarget
void EmberPipeObject::BaseClone( ReferenceTarget* from, ReferenceTarget* to, RemapDir& remap ) {
    if( !to || !from || to == from )
        return;

    frantic::max3d::GenericReferenceTarget<GeomObject, EmberPipeObject>::BaseClone( from, to, remap );

    // TODO: We might need to make the field interface objects cloneable, but for now they can be shallow copied since
    // they are immutable.
    static_cast<EmberPipeObject*>( to )->m_fieldData = static_cast<EmberPipeObject*>( from )->m_fieldData;
    static_cast<EmberPipeObject*>( to )->m_fieldMap = static_cast<EmberPipeObject*>( from )->m_fieldMap;
    static_cast<EmberPipeObject*>( to )->m_bounds = static_cast<EmberPipeObject*>( from )->m_bounds;
    static_cast<EmberPipeObject*>( to )->m_spacing = static_cast<EmberPipeObject*>( from )->m_spacing;
    static_cast<EmberPipeObject*>( to )->m_geomValid = static_cast<EmberPipeObject*>( from )->m_geomValid;
    static_cast<EmberPipeObject*>( to )->m_chanValid = static_cast<EmberPipeObject*>( from )->m_chanValid;
    static_cast<EmberPipeObject*>( to )->m_viewportData = static_cast<EmberPipeObject*>( from )->m_viewportData;
    static_cast<EmberPipeObject*>( to )->m_inWorldSpace = static_cast<EmberPipeObject*>( from )->m_inWorldSpace;
}

// BaseObject
void EmberPipeObject::GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* /*vp*/, Box3& box ) {
    box = m_bounds;

    Matrix3 tm = inode->GetNodeTM( t );

    if( !m_inWorldSpace )
        box = box * tm;

    if( m_viewportData.m_pIcon )
#if MAX_VERSION_MAJOR >= 25
        box += m_viewportData.m_pIcon->GetMeshPtr()->getBoundingBox() * ( m_viewportData.m_iconTM * tm );
#else
        box += m_viewportData.m_pIcon->getBoundingBox() * ( m_viewportData.m_iconTM * tm );
#endif
}

void EmberPipeObject::GetLocalBoundBox( TimeValue t, INode* inode, ViewExp* /*vp*/, Box3& box ) {
    box = m_bounds;

    if( m_inWorldSpace )
        box = box * Inverse( inode->GetNodeTM( t ) );

    if( m_viewportData.m_pIcon )
#if MAX_VERSION_MAJOR >= 25
        box += m_viewportData.m_pIcon->GetMeshPtr()->getBoundingBox() * m_viewportData.m_iconTM;
#else
        box += m_viewportData.m_pIcon->getBoundingBox() * m_viewportData.m_iconTM;
#endif
}

CreateMouseCallBack* EmberPipeObject::GetCreateMouseCallBack() { return NULL; }

void EmberPipeObject::configure_viewport_sampling( float ( &off )[3], float ( &step )[3], int ( &count )[3],
                                                   int& reduce ) {
    // We have a constant number of steps in each direction.
    if( m_viewportData.m_spacingMode == spacing_mode::kSpacingConstant ) {
        step[0] = ( m_bounds.pmax.x - m_bounds.pmin.x ) / static_cast<float>( m_viewportData.m_res[0] );
        step[1] = ( m_bounds.pmax.y - m_bounds.pmin.y ) / static_cast<float>( m_viewportData.m_res[1] );
        step[2] = ( m_bounds.pmax.z - m_bounds.pmin.z ) / static_cast<float>( m_viewportData.m_res[2] );
        count[0] = m_viewportData.m_res[0];
        count[1] = m_viewportData.m_res[1];
        count[2] = m_viewportData.m_res[2];
        off[0] = m_bounds.pmin.x + 0.5f * step[0];
        off[1] = m_bounds.pmin.y + 0.5f * step[1];
        off[2] = m_bounds.pmin.z + 0.5f * step[2];
        reduce = 0;
    } else if( m_viewportData.m_spacingMode == spacing_mode::kSpacingDynamic ) {
        step[0] = m_viewportData.m_spacing[0];
        step[1] = m_viewportData.m_spacing[1];
        step[2] = m_viewportData.m_spacing[2];
        off[0] = ( std::ceil( m_bounds.pmin.x / step[0] - 0.5f ) + 0.5f ) * step[0];
        off[1] = ( std::ceil( m_bounds.pmin.y / step[1] - 0.5f ) + 0.5f ) * step[1];
        off[2] = ( std::ceil( m_bounds.pmin.z / step[2] - 0.5f ) + 0.5f ) * step[2];
        count[0] = std::max( 0, static_cast<int>( std::floor( ( m_bounds.pmax.x - off[0] ) / step[0] ) ) ) + 1;
        count[1] = std::max( 0, static_cast<int>( std::floor( ( m_bounds.pmax.y - off[1] ) / step[1] ) ) ) + 1;
        count[2] = std::max( 0, static_cast<int>( std::floor( ( m_bounds.pmax.z - off[2] ) / step[2] ) ) ) + 1;
        reduce = 0;
    } else if( m_viewportData.m_spacingMode == spacing_mode::kSpacingNatural ) {
        reduce = m_viewportData.m_reduce;
        step[0] = step[1] = step[2] = m_spacing * static_cast<float>( reduce + 1 );
        off[0] = ( std::ceil( m_bounds.pmin.x / m_spacing - 0.5f ) + 0.5f ) * m_spacing;
        off[1] = ( std::ceil( m_bounds.pmin.y / m_spacing - 0.5f ) + 0.5f ) * m_spacing;
        off[2] = ( std::ceil( m_bounds.pmin.z / m_spacing - 0.5f ) + 0.5f ) * m_spacing;
        count[0] = std::max( 0, static_cast<int>( std::floor( ( m_bounds.pmax.x - off[0] ) / step[0] ) ) ) + 1;
        count[1] = std::max( 0, static_cast<int>( std::floor( ( m_bounds.pmax.y - off[1] ) / step[1] ) ) ) + 1;
        count[2] = std::max( 0, static_cast<int>( std::floor( ( m_bounds.pmax.z - off[2] ) / step[2] ) ) ) + 1;

        boost::int64_t maxCount = static_cast<boost::int64_t>( GetMaxMarkerCount() );

        while( static_cast<boost::int64_t>( count[0] ) * static_cast<boost::int64_t>( count[1] ) *
                   static_cast<boost::int64_t>( count[2] ) >
               maxCount ) {
            // Prevent display lockups!
            ++reduce;
            step[0] = step[1] = step[2] = m_spacing * static_cast<float>( reduce + 1 );
            count[0] = static_cast<int>( std::floor( ( m_bounds.pmax.x - off[0] ) / step[0] ) ) + 1;
            count[1] = static_cast<int>( std::floor( ( m_bounds.pmax.y - off[1] ) / step[1] ) ) + 1;
            count[2] = static_cast<int>( std::floor( ( m_bounds.pmax.z - off[2] ) / step[2] ) ) + 1;
        }
    }
}

void EmberPipeObject::SampleFieldForViewport( TimeValue t,
                                              const boost::shared_ptr<frantic::volumetrics::field_interface>& field,
                                              std::vector<Point3>& outPoints, std::vector<Point4>& outColors,
                                              bool& outIsVectors, int& outReduceLevel ) {
    char* buffer = static_cast<char*>( alloca( m_fieldMap.structure_size() ) );

    bool doColor = m_viewportData.m_colorAccessor.is_valid();
    bool doScalarColor = m_viewportData.m_scalarColorAccessor.is_valid();

    float off[3], step[3];
    int count[3];

    outReduceLevel = 0;

    configure_viewport_sampling( off, step, count, outReduceLevel );

    std::size_t estimate = static_cast<std::size_t>( count[0] * count[1] * count[2] / 8 );

    switch( m_viewportData.m_displayMode ) {
    case display_mode::kDisplayDots:
        if( m_viewportData.m_floatAccessor.is_valid() && !m_bounds.IsEmpty() ) {
            outPoints.reserve( estimate );
            if( doColor || doScalarColor )
                outColors.reserve( estimate );

            frantic::graphics::vector3f p( off[0], off[1], off[2] );
            for( int x = 0; x < count[0]; ++x, p.x += step[0], p.y = off[1] ) {
                for( int y = 0; y < count[1]; ++y, p.y += step[1], p.z = off[2] ) {
                    for( int z = 0; z < count[2]; ++z, p.z += step[2] ) {
                        if( field->evaluate_field( buffer, p ) ) {
                            float scalarVal = m_viewportData.m_floatAccessor( buffer );

                            if( scalarVal > m_viewportData.m_scalarMin && scalarVal < m_viewportData.m_scalarMax ) {
                                outPoints.push_back( frantic::max3d::to_max_t( p ) );
                                if( doColor ) {
                                    AColor c = frantic::max3d::to_max_t( m_viewportData.m_colorAccessor.get( buffer ) );
                                    c.ClampMinMax();
                                    outColors.push_back( c );
                                } else if( doScalarColor ) {
                                    float sc = m_viewportData.m_scalarColorAccessor.get( buffer );
                                    AColor c( sc, sc, sc );
                                    c.ClampMinMax();
                                    outColors.push_back( c );
                                }
                            }
                        }
                    } // for z
                }     // for y
            }         // for x
        }
        break;
    case display_mode::kDisplayLines:
        outIsVectors = true;
        if( m_viewportData.m_vecAccessor.is_valid() && !m_bounds.IsEmpty() ) {
            outPoints.reserve( 2 * estimate );
            if( doColor )
                outColors.reserve( 2 * estimate );

            Point3 p[2];

            p[0].x = off[0];
            p[0].y = off[1];
            p[0].z = off[2];
            for( int x = 0; x < count[0]; ++x, p[0].x += step[0], p[0].y = off[1] ) {
                for( int y = 0; y < count[1]; ++y, p[0].y += step[1], p[0].z = off[2] ) {
                    for( int z = 0; z < count[2]; ++z, p[0].z += step[2] ) {
                        if( field->evaluate_field( buffer, ember::vec3( p[0].x, p[0].y, p[0].z ) ) ) {
                            float scalarVal = m_viewportData.m_floatAccessor( buffer );

                            if( scalarVal > m_viewportData.m_scalarMin && scalarVal < m_viewportData.m_scalarMax ) {
                                if( doColor ) {
                                    AColor c = frantic::max3d::to_max_t( m_viewportData.m_colorAccessor.get( buffer ) );
                                    c.ClampMinMax();
                                    outColors.push_back( c );
                                    outColors.push_back( c ); // TODO: We could indicate directionality by adjusting the
                                                              // coloring along a vector.
                                } else if( doScalarColor ) {
                                    float sc = m_viewportData.m_scalarColorAccessor.get( buffer );
                                    AColor c( sc, sc, sc );
                                    c.ClampMinMax();
                                    outColors.push_back( c );
                                    outColors.push_back( c ); // TODO: We could indicate directionality by adjusting the
                                                              // coloring along a vector.
                                }

                                ember::vec3 v = m_viewportData.m_vecAccessor( buffer );
                                // if( !v.is_zero() ){
                                if( m_viewportData.m_normalizeVectors )
                                    v = ember::vec3::normalize( v );
                                v *= m_viewportData.m_vectorScale;

                                p[1].Set( p[0].x + v.x, p[0].y + v.y, p[0].z + v.z );

                                outPoints.push_back( p[0] );
                                outPoints.push_back( p[1] );
                                //}
                            }
                        }
                    } // for z
                }     // for y
            }         // for x
        }
        break;
    }
}

bool EmberPipeObject::cache_viewport_data( TimeValue t ) {
    if( !m_vpDataValid ) {
        bool isReady = false;
        boost::shared_ptr<frantic::volumetrics::field_interface> pField;

        // Since we don't do anything different for world-space fields (at least at this point), we can always cache the
        // viewport data in the base object.

        try {
            pField = this->try_get_field( isReady );
        } catch( ... ) {
            FF_LOG( error ) << frantic::strings::to_tstring( boost::current_exception_diagnostic_information() )
                            << std::endl;
            return false;
        }

        if( isReady ) {
            // TODO: We should spawn a task to asynchronously fill the viewport buffer!
            m_vpData.pointData.clear();
            m_vpData.colorData.clear();
            m_vpData.isVectors = false;

            try {
                m_vpDataValid = true;
                this->SampleFieldForViewport( t, pField, m_vpData.pointData, m_vpData.colorData, m_vpData.isVectors,
                                              m_vpData.reduceLevel );
            } catch( ... ) {
                m_vpData.pointData.clear();
                m_vpData.colorData.clear();

                FF_LOG( error ) << frantic::strings::to_tstring( boost::current_exception_diagnostic_information() )
                                << std::endl;
            }
        }
    }

    return m_vpDataValid;
}

#if MAX_VERSION_MAJOR >= 17

void EmberPipeObject::PopulateRenderItem(
    TimeValue t, frantic::max3d::viewport::ParticleRenderItem<frantic::max3d::viewport::fake_particle>& renderItem ) {
    if( this->cache_viewport_data( t ) ) {
        MaxSDK::Graphics::PrimitiveType primType = MaxSDK::Graphics::PrimitivePointList;
        if( m_vpData.isVectors )
            primType = MaxSDK::Graphics::PrimitiveLineList;

        frantic::max3d::viewport::render_type renderType = primType == MaxSDK::Graphics::PrimitiveLineList
                                                               ? frantic::max3d::viewport::RT_VELOCITY
                                                               : frantic::max3d::viewport::RT_POINT_LARGE;

        std::vector<frantic::max3d::viewport::fake_particle> particleData;

        std::size_t end = m_vpData.pointData.size();
        if( m_vpData.isVectors ) {
            end /= 2;
            renderItem.SetPrecomputedVelocityOffset( true );
        }

        for( std::size_t i = 0; i < end; ++i ) {
            frantic::max3d::viewport::fake_particle particle;
            if( m_vpData.isVectors ) {
                const std::size_t baseIndex = i * 2;
                particle.m_position = m_vpData.pointData[baseIndex];
                particle.m_vector = m_vpData.pointData[baseIndex + 1];
            } else {
                particle.m_position = m_vpData.pointData[i];
            }

            Point4 vpColor;

            if( m_vpData.colorData.size() > 0 ) {
                Point4& vpColor = m_vpData.colorData[i];
                particle.m_color.r = vpColor.x;
                particle.m_color.g = vpColor.y;
                particle.m_color.b = vpColor.z;
            } else {
                particle.m_color = m_wireColor;
            }

            particleData.push_back( particle );
        }

        renderItem.Initialize( particleData, renderType );

        Point3 p( 0.5f * ( m_bounds.pmax.x + m_bounds.pmin.x ), 0.5f * ( m_bounds.pmax.y + m_bounds.pmin.y ),
                  m_bounds.pmax.z );

        if( m_vpData.reduceLevel > 0 ) {
            MCHAR reduceMsg[8 + 3 + 1]; // 8 chars for "Reduce: ", 3 chars for the number & 1 char for NULL;
            _sntprintf( reduceMsg, sizeof( reduceMsg ) / sizeof( reduceMsg[0] ), _T("Reduce: %d"),
                        std::min( 999, m_vpData.reduceLevel ) );

            renderItem.SetMessage( p, reduceMsg );
        } else {
            renderItem.SetMessage( p, M_STD_STRING() );
        }
    } else if( !m_vpUpdatePending ) {
        // Note: Even if this is called in a different (ie. non-main) thread, there is no race condition since we only
        // ever set this once and never clear it.
        m_vpUpdatePending = true;
        m_vpUpdateThread = boost::move( boost::thread::thread( &update_viewport_when_ready, m_fieldData ) );
        m_vpUpdateThread.detach();
    }
}

unsigned long EmberPipeObject::GetObjectDisplayRequirement() const {
    // We do not need per-view updates or the legacy render code to be called.
    return 0;
}

bool EmberPipeObject::PrepareDisplay( const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext ) {
    // TODO: Cache data that doesn't depend on the node (ie. Currently everything where Stoke MX is concerned).

    typedef frantic::max3d::viewport::ParticleRenderItem<frantic::max3d::viewport::fake_particle> render_item_t;

    boost::filesystem::path shaderPath( frantic::win32::GetModuleFileName( hInstance ) );
    shaderPath =
        ( shaderPath.parent_path() / _T("\\..\\Shaders\\particle_small.fx") ).string<frantic::tstring>().c_str();

    if( !boost::filesystem::exists( shaderPath ) ) {
        FF_LOG( error ) << shaderPath.c_str() << " doesn't exist. Please re-install StokeMX.";
    }

    std::unique_ptr<render_item_t> newRenderItem( new render_item_t( shaderPath.c_str() ) );

    Point3 p( 0.5f * ( m_bounds.pmax.x + m_bounds.pmin.x ), 0.5f * ( m_bounds.pmax.y + m_bounds.pmin.y ),
              m_bounds.pmax.z );

    newRenderItem->SetInWorldSpace( this->GetInWorldSpace() );
    newRenderItem->SetPointSize( m_viewportData.m_dotSize );
    newRenderItem->SetSkipInverseTransform( true );
    newRenderItem->SetMessage( p, _T("Calculating...") );

    m_markerRenderItem.Release();
    m_markerRenderItem.Initialize();
    m_markerRenderItem.SetCustomImplementation( newRenderItem.release() );

    MaxSDK::Graphics::Matrix44 iconTM;
    MaxSDK::Graphics::MaxWorldMatrixToMatrix44( iconTM, m_viewportData.m_iconTM );

#if MAX_VERSION_MAJOR >= 25
    std::unique_ptr<MaxSDK::Graphics::Utilities::MeshEdgeRenderItem> iconImpl(
        new frantic::max3d::viewport::DecoratedMeshEdgeRenderItem( m_viewportData.m_pIcon, false, iconTM ) );
#else
    std::unique_ptr<MaxSDK::Graphics::Utilities::MeshEdgeRenderItem> iconImpl(
        new frantic::max3d::viewport::DecoratedMeshEdgeRenderItem( m_viewportData.m_pIcon, false, false, iconTM ) );
#endif

    m_iconRenderItem.Release();
    m_iconRenderItem.Initialize();
    m_iconRenderItem.SetCustomImplementation( iconImpl.release() );

    std::unique_ptr<frantic::max3d::viewport::BoxRenderItem> boundsImpl( new frantic::max3d::viewport::BoxRenderItem );

    boundsImpl->Initialize( m_bounds );
    boundsImpl->SetInWorldSpace( this->GetInWorldSpace() );

    m_boundsRenderItem.Release();
    m_boundsRenderItem.Initialize();
    m_boundsRenderItem.SetCustomImplementation( boundsImpl.release() );

    return true;
}

bool EmberPipeObject::UpdatePerNodeItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                          MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                          MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer ) {
    targetRenderItemContainer.ClearAllRenderItems();

    if( m_viewportData.m_displayMode != display_mode::kDisplayNone ) {
        AColor nodeColor = AColor( nodeContext.GetRenderNode().GetWireColor() );
        m_wireColor = nodeColor;
        dynamic_cast<frantic::max3d::viewport::ParticleRenderItem<frantic::max3d::viewport::fake_particle>*>(
            m_markerRenderItem.GetCustomeImplementation() )
            ->SetCallback(
                std::bind( &EmberPipeObject::PopulateRenderItem, this, std::placeholders::_1, std::placeholders::_2 ) );
        targetRenderItemContainer.AddRenderItem( m_markerRenderItem );
    }

    if( m_viewportData.m_pIcon )
        targetRenderItemContainer.AddRenderItem( m_iconRenderItem );

    if( m_viewportData.m_drawBounds )
        targetRenderItemContainer.AddRenderItem( m_boundsRenderItem );

    return true;
}

bool EmberPipeObject::UpdatePerViewItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                          MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                          MaxSDK::Graphics::UpdateViewContext& viewContext,
                                          MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer ) {
    assert( false && "We do not require UpdatePerViewItems() at this time" );
    return frantic::max3d::GenericReferenceTarget<GeomObject, EmberPipeObject>::UpdatePerViewItems(
        updateDisplayContext, nodeContext, viewContext, targetRenderItemContainer );
}

#else

// In Max 2013/2014 builds, we only support the legacy display mode (since the Max 2013 style Nitrous viewports are
// pretty terrible).
bool EmberPipeObject::RequiresSupportForLegacyDisplayMode() const { return true; }
#endif

namespace {
void DrawPoints( GraphicsWindow* gw, const std::vector<Point3>& pointData, const std::vector<Point4>& colorData ) {
    if( colorData.empty() ) {
        gw->startMarkers();
        for( auto it = pointData.begin(), itEnd = pointData.end(); it != itEnd; ++it )
            gw->marker( const_cast<Point3*>( &*it ), SM_DOT_MRKR );
        gw->endMarkers();
    } else {
        assert( colorData.size() == pointData.size() );

        gw->startMarkers();
        auto itColor = colorData.begin();
        for( auto it = pointData.begin(), itEnd = pointData.end(); it != itEnd; ++it, ++itColor ) {
            gw->setColor( LINE_COLOR, itColor->x, itColor->y, itColor->z );
            gw->marker( const_cast<Point3*>( &*it ), SM_DOT_MRKR );
        }
        gw->endMarkers();
    }
}

BOOL HitTestPoints( GraphicsWindow* gw, const std::vector<Point3>& pointData, std::size_t bufferSize = 1000 ) {
    auto it = pointData.begin(), itEnd = pointData.end();
    while( it != itEnd ) {
        gw->startMarkers();
        for( std::size_t i = 0; i < bufferSize && it != itEnd; ++it, ++i )
            gw->marker( const_cast<Point3*>( &*it ), SM_DOT_MRKR );
        gw->endMarkers();
        if( gw->checkHitCode() )
            return TRUE;
    }
    return FALSE;
}

void DrawVectors( GraphicsWindow* gw, const std::vector<Point3>& pointData, const std::vector<Point4>& colorData ) {
    assert( colorData.empty() || /*2 **/ colorData.size() == pointData.size() );
    assert( pointData.size() % 2 == 0 );

    if( colorData.empty() ) {
        gw->startMarkers();
        gw->startSegments();
        for( auto it = pointData.begin(), itEnd = pointData.end(); it != itEnd; it += 2 ) {
            gw->segment( const_cast<Point3*>( &*it ), TRUE );
            gw->marker( const_cast<Point3*>( &*it ), POINT_MRKR );
        }
        gw->endSegments();
        gw->endMarkers();
    } else {
        gw->startMarkers();
        gw->startSegments();
        auto itColor = colorData.begin();
        for( auto it = pointData.begin(), itEnd = pointData.end(); it != itEnd; it += 2, itColor += 2 ) {
            gw->setColor( LINE_COLOR, itColor->x, itColor->y, itColor->z );
            gw->segment( const_cast<Point3*>( &*it ), TRUE );
            gw->marker( const_cast<Point3*>( &*it ), POINT_MRKR );
        }
        gw->endSegments();
        gw->endMarkers();
    }
}

BOOL HitTestVectors( GraphicsWindow* gw, const std::vector<Point3>& pointData, std::size_t bufferSize = 500 ) {
    assert( pointData.size() % 2 == 0 );

    auto it = pointData.begin(), itEnd = pointData.end();
    while( it != itEnd ) {
        gw->startSegments();
        for( std::size_t i = 0; i < bufferSize && it != itEnd; it += 2 )
            gw->segment( const_cast<Point3*>( &*it ), TRUE );
        gw->endSegments();
        if( gw->checkHitCode() )
            return TRUE;
    }
    return FALSE;
}
} // namespace

int EmberPipeObject::Display( TimeValue t, INode* inode, ViewExp* vpt, int /*flags*/ ) {
    // if( MaxSDK::Graphics::IsRetainedModeEnabled() )
    //	return FALSE;

#if MAX_VERSION_MAJOR >= 17
    assert( !MaxSDK::Graphics::IsRetainedModeEnabled() &&
            "Legacy Display() code should not execute in Max 2015+, if Nitrous is enabled." );

    if( MaxSDK::Graphics::IsRetainedModeEnabled() )
        return TRUE;
#endif

    GraphicsWindow* gw = vpt->getGW();
    if( !gw )
        return FALSE;

    DWORD rndLimits = gw->getRndLimits();

    Matrix3 dataTM( TRUE );
    if( !this->m_inWorldSpace )
        dataTM = inode->GetNodeTM( t );

    gw->setTransform( dataTM );

    if( !this->cache_viewport_data( t ) ) {
        // We need to force a VP update once the data is ready...
        if( !m_vpUpdatePending ) {
            m_vpUpdatePending = true;
            m_vpUpdateThread = boost::move( boost::thread::thread( &update_viewport_when_ready, m_fieldData ) );
            m_vpUpdateThread.detach();
        }

        frantic::max3d::DrawBox( gw, m_bounds );

        gw->text( &m_bounds.pmin, _T("Calculating...") );
    } else {
        AColor defaultColor( inode->GetWireColor() );
        if( inode->Selected() )
            defaultColor.White();

        gw->setColor( LINE_COLOR, defaultColor.r, defaultColor.g, defaultColor.b );

        if( !m_vpData.isVectors ) {
            DrawPoints( gw, m_vpData.pointData, m_vpData.colorData );
        } else {
            DrawVectors( gw, m_vpData.pointData, m_vpData.colorData );
        }

        if( m_viewportData.m_drawBounds && !m_bounds.IsEmpty() ) {
            Color boundsColor( inode->Selected() ? RGB( 255, 255, 255 ) : inode->GetWireColor() );

            gw->setColor( LINE_COLOR, boundsColor.r, boundsColor.g, boundsColor.b );

            frantic::max3d::DrawBox( gw, m_bounds );
        }

        if( m_viewportData.m_reduce > 0 ) {
            MCHAR reduceMsg[8 + 3 + 1]; // 8 chars for "Reduce: ", 3 chars for the number & 1 char for NULL;
            _stprintf( reduceMsg, _T("Reduce: %d"), std::min( 999, m_viewportData.m_reduce ) );

            Point3 p( 0.5f * ( m_bounds.pmax.x + m_bounds.pmin.x ), 0.5f * ( m_bounds.pmax.y + m_bounds.pmin.y ),
                      m_bounds.pmax.z );
            gw->text( &p, reduceMsg );
        }
    }

    if( m_viewportData.m_pIcon ) {
        gw->setTransform( m_viewportData.m_iconTM * inode->GetNodeTM( t ) );

        frantic::max3d::draw_mesh_wireframe( gw,
#if MAX_VERSION_MAJOR >= 25
                                             m_viewportData.m_pIcon->GetMeshPtr(),
#else
                                             m_viewportData.m_pIcon,
#endif
                                             inode->Selected()
                                                 ? frantic::graphics::color3f::white()
                                                 : frantic::graphics::color3f::from_RGBA( inode->GetWireColor() ) );
    }

    gw->setRndLimits( rndLimits );

    return TRUE;
}

int EmberPipeObject::HitTest( TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt ) {
#if MAX_VERSION_MAJOR >= 17
    assert( !MaxSDK::Graphics::IsHardwareHitTesting( vpt ) &&
            "Legacy HitTest() code should not execute in Max 2015+, if Nitrous is enabled." );

    if( MaxSDK::Graphics::IsHardwareHitTesting( vpt ) )
        return FALSE;
#endif

    GraphicsWindow* gw = vpt->getGW();

    if( !gw || !inode || ( ( flags & HIT_SELONLY ) && !inode->Selected() ) )
        return FALSE;

    DWORD rndLimits = gw->getRndLimits();

    HitRegion hitRegion;
    MakeHitRegion( hitRegion, type, crossing, 4, p );

    gw->setRndLimits( GW_PICK | GW_WIREFRAME );
    gw->setHitRegion( &hitRegion );
    gw->clearHitCode();

    BOOST_SCOPE_EXIT( gw, rndLimits ) { gw->setRndLimits( rndLimits ); }
    BOOST_SCOPE_EXIT_END

    Matrix3 dataTM( TRUE );
    if( !this->m_inWorldSpace )
        dataTM = inode->GetNodeTM( t );

    gw->setTransform( dataTM );

    if( !this->cache_viewport_data( t ) ) {
        frantic::max3d::DrawBox( gw, m_bounds );
    } else {
        if( m_viewportData.m_drawBounds ) {
            frantic::max3d::DrawBox( gw, m_bounds );
            if( gw->checkHitCode() )
                return TRUE;
        }

        if( !m_vpData.isVectors ) {
            if( HitTestPoints( gw, m_vpData.pointData ) )
                return TRUE;
        } else {
            if( HitTestVectors( gw, m_vpData.pointData ) )
                return TRUE;
        }
    }

    if( m_viewportData.m_pIcon ) {
        gw->setTransform( m_viewportData.m_iconTM * inode->GetNodeTM( t ) );
#if MAX_VERSION_MAJOR >= 25
        if( m_viewportData.m_pIcon->GetMeshPtr()->select( gw, NULL, &hitRegion ) )
#else
        if( m_viewportData.m_pIcon->select( gw, NULL, &hitRegion ) )
#endif
            return TRUE;
    }

    return FALSE;
}

// Object
ObjectState EmberPipeObject::Eval( TimeValue /*t*/ ) { return ObjectState( this ); }

void EmberPipeObject::GetDeformBBox( TimeValue /*t*/, Box3& box, Matrix3* tm, BOOL /*useSel*/ ) {
    if( tm )
        tm->ValidateFlags();

    if( tm && !tm->IsIdentity() ) {
        // Calculate a fancier bounding box ...
        box = m_bounds;
    } else {
        box = m_bounds;
    }
}

int EmberPipeObject::CanConvertToType( Class_ID obtype ) { return obtype == EmberPipeObject_CLASSID; }

Object* EmberPipeObject::ConvertToType( TimeValue /*t*/, Class_ID obtype ) {
    if( obtype != EmberPipeObject_CLASSID )
        return NULL;
    return this;
}

Interval EmberPipeObject::ChannelValidity( TimeValue t, int nchan ) {
    if( this->IsBaseClassOwnedChannel( nchan ) ) {
        Interval iv = GeomObject::ChannelValidity( t, nchan );
        return iv;
    }

    Interval iv = FOREVER;

    switch( nchan ) {
    case GEOM_CHAN_NUM:
        iv = m_geomValid;
        break;
    case DISP_ATTRIB_CHAN_NUM:
        if( !( m_chanValid & DISP_ATTRIB_CHANNEL ) )
            iv = NEVER;
        break;
    default:
        break;
    }

    return iv;
}

void EmberPipeObject::SetChannelValidity( int nchan, Interval v ) {
    GeomObject::SetChannelValidity( nchan, v );

    switch( nchan ) {
    case GEOM_CHAN_NUM:
        m_geomValid = v;
        break;
    case DISP_ATTRIB_CHAN_NUM:
        m_chanValid = ( m_chanValid & ~DISP_ATTRIB_CHANNEL ) | ( v.Empty() ? 0 : DISP_ATTRIB_CHANNEL );
        break;
    default:
        break;
    }
}

void EmberPipeObject::InvalidateChannels( ChannelMask channels ) {
    GeomObject::InvalidateChannels( channels );

    if( channels & GEOM_CHANNEL ) {
        m_geomValid.SetEmpty();
    }

    if( channels & DISP_ATTRIB_CHANNEL ) {
        m_chanValid &= ~DISP_ATTRIB_CHANNEL;
    }
}

Interval EmberPipeObject::ObjectValidity( TimeValue t ) {
    Interval iv = GeomObject::ObjectValidity( t );

    iv &= m_geomValid;

    if( ( m_chanValid & DISP_ATTRIB_CHANNEL ) == 0 )
        iv.SetEmpty();

    return iv;
}

Object* EmberPipeObject::MakeShallowCopy( ChannelMask channels ) {
    EmberPipeObject* result = new EmberPipeObject;
    result->ShallowCopy( this, channels );
    return result;
}

void EmberPipeObject::ShallowCopy( Object* fromOb, ChannelMask channels ) {
    GeomObject::ShallowCopy( fromOb, channels );

    if( channels & GEOM_CHANNEL ) {
        EmberPipeObject* fromEmber = static_cast<EmberPipeObject*>( fromOb );

        m_fieldData = fromEmber->m_fieldData;
        m_fieldMap = fromEmber->m_fieldMap;
        m_bounds = fromEmber->m_bounds;
        m_spacing = fromEmber->m_spacing;
        m_geomValid = fromEmber->m_geomValid;
        m_inWorldSpace = fromEmber->m_inWorldSpace;
    }

    if( channels & DISP_ATTRIB_CHANNEL ) {
        EmberPipeObject* fromEmber = static_cast<EmberPipeObject*>( fromOb );

        m_chanValid = ( m_chanValid & ~DISP_ATTRIB_CHANNEL ) | ( fromEmber->m_chanValid & DISP_ATTRIB_CHANNEL );
        m_viewportData = fromEmber->m_viewportData;

        // We need to regenerate the viewport data
        m_vpDataValid = false;

        this->update_accessors();
    }
}

void EmberPipeObject::FreeChannels( ChannelMask channels ) {
    if( channels & GEOM_CHANNEL ) {
        m_geomValid.SetEmpty();
        m_fieldData.reset();
        m_fieldMap.reset();
        m_bounds.Init();
        m_spacing = 1.f;
    }

    if( channels & DISP_ATTRIB_CHANNEL ) {
        // Nothing to de-allocate
    }
}

void EmberPipeObject::NewAndCopyChannels( ChannelMask channels ) {
    GeomObject::NewAndCopyChannels( channels );

    if( channels & GEOM_CHANNEL ) {
        // We don't have to do anything because our implementation object doesn't have any way to be modified.
    }

    if( channels & DISP_ATTRIB_CHANNEL ) {
    }
}

Mesh* EmberPipeObject::GetRenderMesh( TimeValue /*t*/, INode* /*inode*/, View& /*view*/, BOOL& needDelete ) {
    static Mesh* emptyMesh = NULL;
    if( !emptyMesh )
        emptyMesh = CreateNewMesh();

    needDelete = FALSE;

    return emptyMesh;
}

ClassDesc2* EmberPipeObject::GetClassDesc() { return GetEmberPipeObjectDesc(); }

} // namespace max3d
} // namespace ember

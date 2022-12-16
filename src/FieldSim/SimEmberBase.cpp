// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource.h"

#include <stoke/max3d/FieldSim/SimEmberBase.hpp>
#include <stoke/max3d/FieldSim/SimEmberBaseParamBlock.hpp>
#include <stoke/max3d/FieldSim/SimEmberSimulator.hpp>
#include <stoke/max3d/ISimEmber.hpp>

#include <ember/concatenated_field.hpp>
#include <ember/openvdb.hpp>
#include <ember/staggered_grid.hpp>

#include <frantic/max3d/create_mouse_callback.hpp>
#include <frantic/max3d/exception.hpp>
#include <frantic/max3d/maxscript/maxscript.hpp> // TODO: This is a ugly header performance-wise. Try not to use it.
#include <frantic/max3d/maxscript/shared_value_functor.hpp>
#include <frantic/max3d/maxscript/shared_value_ptr.hpp>
#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>
#include <frantic/max3d/time.hpp>

#include <frantic/logging/logging_level.hpp>
#include <frantic/win32/invoke_queue.hpp>

#pragma warning( push, 3 )
#pragma warning( disable : 4800 4244 4146 4267 4355 4503 )
#include <openvdb/Grid.h>
#include <openvdb/io/File.h>
#pragma warning( pop )

#pragma warning( disable : 4503 ) // "decorated name length exceeded, name was truncated" casued by OpenVDB template
                                  // instantiations.

#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/function.hpp>

#pragma warning( push, 3 )
#include <tbb/parallel_reduce.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tbb_allocator.h>
#pragma warning( pop )

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#undef NDEBUG
#include <cassert>

#define SimEmberBase_CLASSID Class_ID( 0x26300a68, 0x12566139 )
#define SimEmberBase_NAME "StokeFieldSimBase"
#define SimEmberBase_DISPLAYNAME "Stoke Field Sim Base"
#define SimEmberBase_VERSION 1

extern void BuildMesh_Text_EMBER_Sim( Mesh& outMesh );

frantic::win32::invoke_queue& GetMainThreadDispatcher();

namespace ember {
namespace max3d {

template <class Callable>
inline void InvokeInMainThread( const Callable& callable ) {
    frantic::win32::invoke_queue& queue = GetMainThreadDispatcher();
    if( queue.is_owning_thread() ) {
        callable();
    } else {
        queue.invoke( callable );
    }
}

ClassDesc2* GetSimEmberBaseDesc();

ISimEmberBase::~ISimEmberBase() {}

ISimEmberBase::ThisInterfaceDesc* ISimEmberBase::GetDesc() { return GetStaticDesc(); }

ISimEmberBase::ThisInterfaceDesc* ISimEmberBase::GetStaticDesc() {
    static ThisInterfaceDesc theDesc( SimEmberBase_INTERFACE, _T("SimEmberBase"), 0 );

    if( theDesc.empty() ) { // ie. Check if we haven't initialized the descriptor yet
        theDesc.function( _T("ResetCache"), &ISimEmberBase::ResetCache );
        theDesc.function( _T("FlushCache"), &ISimEmberBase::FlushCache );
        theDesc.function( _T("FlushCacheAsync"), &ISimEmberBase::FlushCacheAsync ).param( _T("Callback") );
        theDesc.function( _T("CancelFlushCache"), &ISimEmberBase::CancelFlushCache );
        theDesc.function( _T("InitializeCache"), &ISimEmberBase::InitializeCache );
        theDesc.function( _T("AddData"), &ISimEmberBase::AddData ).param( _T("NewData") ).param( _T("Time") );
        theDesc.function( _T("GetCacheTimes"), &ISimEmberBase::GetCacheTimes );
        theDesc.function( _T("GetMemoryCacheTimes"), &ISimEmberBase::GetMemoryCacheTimes );
        theDesc.function( _T("SetNumSerializerThreads"), &ISimEmberBase::SetNumSerializerThreads )
            .param( _T("NumThreads") );
        theDesc.function( _T("SetSerializerCallback"), &ISimEmberBase::SetSerializerCallback ).param( _T("Callback") );
    }

    return &theDesc;
}

class SimEmberBaseDesc : public ClassDesc2 {
    frantic::max3d::ParamBlockBuilder m_pbBuilder;

    friend class SimEmberBase;

  public:
    SimEmberBaseDesc();

    int IsPublic() { return FALSE; }
    void* Create( BOOL loading ) { return new SimEmberBase( loading ); }
    const TCHAR* ClassName() { return _T( SimEmberBase_NAME ); }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return SimEmberBase_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() {
        return _T( SimEmberBase_DISPLAYNAME );
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( SimEmberBase_NAME ); }
#endif
};

SimEmberBaseDesc::SimEmberBaseDesc()
    : m_pbBuilder( kMainBlock, _T("Parameters"), 0, SimEmberBase_VERSION ) {
    m_pbBuilder.OwnerClassDesc( this );
    m_pbBuilder.OwnerRefNum( 0 );
    InitParamBlockDesc( m_pbBuilder );

    ISimEmberBase::GetStaticDesc()->SetClassDesc( this );
}

ClassDesc2* GetSimEmberBaseDesc() {
    static SimEmberBaseDesc theSimEmberBaseDesc;
    return &theSimEmberBaseDesc;
}

std::size_t SimEmberBase::field_size_estimator::operator()( const serializer_data& dataSet ) const {
    return dataSet.memoryUsage;
}

namespace {
std::size_t calculate_memory_usage( const boost::shared_ptr<frantic::volumetrics::field_interface>& field ) {
    std::size_t result = 0;

    if( boost::shared_ptr<ember::concatenated_field> fieldGroup =
            boost::dynamic_pointer_cast<ember::concatenated_field>( field ) ) {
        for( std::size_t i = 0, iEnd = fieldGroup->get_num_fields(); i < iEnd; ++i )
            result += calculate_memory_usage( fieldGroup->get_field( i ) );
    } else if( boost::shared_ptr<ember::openvdb_field_interface> gridField =
                   boost::dynamic_pointer_cast<ember::openvdb_field_interface>( field ) ) {
        result = gridField->get_memory_usage();
    } else if( boost::shared_ptr<ember::staggered_discretized_field> staggeredField =
                   boost::dynamic_pointer_cast<ember::staggered_discretized_field>( field ) ) {
        result = staggeredField->get_grid().get_data().capacity() * sizeof( frantic::graphics::vector3f );
    } else {
        throw std::runtime_error( "Invalid grid type for SimEmber" );
    }

    return result;
}
} // namespace

inline void SimEmberBase::field_serializer::serialize( const frantic::tstring& path, const serializer_data& val ) const
/*throw()*/ {
    write_openvdb_file( path, val.pField, val.bounds, val.spacing );
}

inline SimEmberBase::serializer_data SimEmberBase::field_serializer::deserialize( const frantic::tstring& path ) const {
    serializer_data result;

    ember::openvdb_meta vdbMeta;

    result.pField = ember::create_field_interface_from_file( path, vdbMeta )
                        ->get(); // Blocks until ready. TODO: We should allow the serializer to operate asynchronously.
    result.bounds = vdbMeta.bounds;
    result.spacing = vdbMeta.spacing;
    result.memoryUsage = vdbMeta.memoryUsage;

    return result;
}

inline void SimEmberBase::field_serializer::process_errors() {
    if( m_storedError )
        boost::rethrow_exception( m_storedError );
}

SimEmberBase::SimEmberBase( BOOL loading )
    : m_dataCache( DEFAULT_CACHE_SIZE, m_serializer ) {
    GetSimEmberBaseDesc()->MakeAutoParamBlocks( this );

    if( !loading ) {
        // Invoke the PBAccessor handlers for default values
        m_pblock->CallSet( kUseDiskCache );
        m_pblock->CallSet( kSequenceCacheCapacity );
        m_pblock->CallSet( kSerializeQueueCapacity );
        m_pblock->CallSet( kSequencePath );
    }
}

SimEmberBase::~SimEmberBase() {}

BaseInterface* SimEmberBase::GetInterface( Interface_ID id ) {
    if( BaseInterface* result = ISimEmberBase::GetInterface( id ) )
        return result;

    return EmberObjectBase::GetInterface( id );
}

class SimEmberBase::SimEmberBasePLC : public PostLoadCallback {
    SimEmberBase* m_pOwner;

  public:
    explicit SimEmberBasePLC( SimEmberBase* pOwner )
        : m_pOwner( pOwner ) {}

    virtual void proc( ILoad* /*iload*/ ) {
        if( m_pOwner && m_pOwner->m_pblock && m_pOwner->m_pblock->GetInt( kUseDiskCache ) )
            m_pOwner->m_dataCache.set_disk_path( m_pOwner->m_pblock->GetStr( kSequencePath ),
                                                 SimEmberBase::cache_type::synchronize );

        delete this;
    }
};

IOResult SimEmberBase::Load( ILoad* iload ) {
    IOResult result = GeomObject::Load( iload );

    if( result == IO_OK )
        iload->RegisterPostLoadCallback( new SimEmberBasePLC( this ) );

    return result;
}

RefResult SimEmberBase::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& /*partID*/,
                                          RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        int tabIndex = -1;
        ParamID paramID = m_pblock->LastNotifyParamID( tabIndex );

        if( message == REFMSG_CHANGE ) {
            bool affectsViewport = false;
            bool affectsObject = false;

            switch( paramID ) {
            case kIconSize:
            case kViewportEnabled:
            case kViewportPercentage:
            case kViewportMaskChannel:
            case kViewportVectorChannel:
            case kViewportColorChannel:
            case kUseViewportLimit:
            case kViewportVectorScale:
            case kViewportVectorNormalize:
            case kViewportBoundsEnabled:
            case kViewportReduce:
                affectsViewport = true;
                break;
            case kViewportLimit:
                if( m_pblock->GetInt( kUseViewportLimit ) )
                    affectsViewport = true;
                break;
            case kUsePlaybackTime:
            case kUsePlaybackInterpolation:
                affectsObject = true;
                break;
            case kPlaybackTime:
                if( m_pblock->GetInt( kUsePlaybackTime ) )
                    affectsObject = true;
                break;

            case kSequencePath:
                if( m_pblock->GetInt( kUseDiskCache ) )
                    affectsObject = true;
                break;
            case kUseDiskCache:
                if( const MCHAR* szPath = m_pblock->GetStr( kSequencePath ) ) {
                    if( *szPath != _T( '\0' ) )
                        affectsObject = true;
                }
                break;
            case kSequenceCacheCapacity:
            case kSequenceCacheUsage:
            case kSerializeQueueCapacity:
            case kSerializeQueueUsage:
                return REF_STOP;
            };

            if( affectsObject ) {
                this->InvalidateCache();
                this->NotifyDependents( FOREVER, PART_GEOM, REFMSG_CHANGE );

                return REF_STOP;
            } else if( affectsViewport ) {
                this->InvalidateCache(); // This is required or else old stored values will continued to be used. We can
                                         // improve performance by storing validity for geometry & display separately.
                this->NotifyDependents( FOREVER, PART_DISPLAY, REFMSG_CHANGE );

                return REF_STOP;
            }
        }
    }

    return REF_SUCCEED;
}

CreateMouseCallBack* SimEmberBase::GetCreateMouseCallBack() {
    static frantic::max3d::ClickAndDragCreateCallBack theCallback;
    return &theCallback;
}

// Arbitrarily using 10 ticks as the finite differences offset when calculating the derivative of the playback graph.
#define TICK_OFFSET 10

void SimEmberBase::EvalField( EmberPipeObject& objToPopulate, TimeValue t ) {
    Interval valid = FOREVER;

    int frameRate = GetFrameRate();

    BOOL usePlaybackTime = FALSE;
    m_pblock->GetValue( kUsePlaybackTime, t, usePlaybackTime, valid );

    double playbackTime;
    double playbackDerivative = 1.0; // If we are using the playback controller, we need to know the derivative in order
                                     // to modify time dependent channels like Velocity.

    if( usePlaybackTime ) {
        double playbackTime2;

        float playbackTimeFloat;
        m_pblock->GetValue( kPlaybackTime, t, playbackTimeFloat, valid );

        playbackTime = static_cast<double>( playbackTimeFloat );
        playbackTime2 = m_pblock->GetFloat( kPlaybackTime, t + TICK_OFFSET );

        // Derivative is a ratio, and therefore doesn't need to be converted to seconds.
        playbackDerivative =
            ( playbackTime2 - playbackTime ) / frantic::max3d::to_frames<double>( TICK_OFFSET, frameRate );
    } else {
        playbackTime = frantic::max3d::to_frames<double>( t, frameRate );

        valid.SetInstant( t );
    }

    std::pair<double, double> bracketingKeys = m_dataCache.find_bracketing_keys( playbackTime );
    if( !frantic::math::is_finite( bracketingKeys.first ) ) {
        objToPopulate.SetEmpty();
    } else if( !frantic::math::is_finite( bracketingKeys.second ) || bracketingKeys.first == playbackTime ) {
        serializer_data data = m_dataCache.find( bracketingKeys.first );

        if( !data.pField ) {
            objToPopulate.SetEmpty();
            return;
        } else {
            Box3 max3dBounds( frantic::max3d::to_max_t( data.bounds.minimum() ),
                              frantic::max3d::to_max_t( data.bounds.maximum() ) );

            objToPopulate.Set( create_field_task( data.pField ), data.pField->get_channel_map(), max3dBounds,
                               data.spacing );
        }
    } else {
        double leftOffset = playbackTime - bracketingKeys.first;
        double rightOffset = bracketingKeys.second - playbackTime;

        if( leftOffset <= rightOffset ) {
            serializer_data data = m_dataCache.find( bracketingKeys.first );

            Box3 max3dBounds( frantic::max3d::to_max_t( data.bounds.minimum() ),
                              frantic::max3d::to_max_t( data.bounds.maximum() ) );

            objToPopulate.Set( create_field_task( data.pField ), data.pField->get_channel_map(), max3dBounds,
                               data.spacing );
        } else {
            serializer_data data = m_dataCache.find( bracketingKeys.second );

            Box3 max3dBounds( frantic::max3d::to_max_t( data.bounds.minimum() ),
                              frantic::max3d::to_max_t( data.bounds.maximum() ) );

            objToPopulate.Set( create_field_task( data.pField ), data.pField->get_channel_map(), max3dBounds,
                               data.spacing );
        }
    }

    // int vpReduce = this->get_viewport_reduce();

    if( frantic::max3d::get<bool>( m_pblock, kViewportEnabled, t ) ) {
        int vpReduce = std::max( 0, get_viewport_reduce( m_pblock ) );

        objToPopulate.SetViewportSpacingNatural( vpReduce );

        const frantic::channels::channel_map& map = objToPopulate.GetChannels();

        const MCHAR* szMaskChannel = get_viewport_mask_channel( m_pblock );
        if( !szMaskChannel )
            szMaskChannel = _T("");

        const MCHAR* szVelocityChannel = get_viewport_vector_channel( m_pblock );
        if( !szVelocityChannel )
            szVelocityChannel = _T("");

        const MCHAR* szColorChannel = get_viewport_color_channel( m_pblock );
        if( !szColorChannel )
            szColorChannel = _T("");

        objToPopulate.SetViewportScalarChannel( szMaskChannel );
        objToPopulate.SetViewportVectorChannel( szVelocityChannel );
        objToPopulate.SetViewportColorChannel( szColorChannel );

        if( !szVelocityChannel || szVelocityChannel[0] == _T( '\0' ) ) {
            objToPopulate.SetViewportDisplayDots( 0.f, std::numeric_limits<float>::infinity() );
        } else {
            bool normalize = get_viewport_vector_normalize( m_pblock );
            float scale = get_viewport_vector_scale( m_pblock, t );

            objToPopulate.SetViewportDisplayLines( normalize, scale );
        }
    } else {
        objToPopulate.SetViewportDisplayNone();
    }

    objToPopulate.SetViewportDisplayBounds( get_viewport_bounds_enabled( m_pblock ) );

    objToPopulate.SetChannelValidity( GEOM_CHAN_NUM, valid );
    objToPopulate.SetChannelValidity( DISP_ATTRIB_CHAN_NUM, FOREVER );

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
            BuildMesh_Text_EMBER_Sim( theIcon->GetMesh() );
#else
            theIcon = CreateNewMesh();
            BuildMesh_Text_EMBER_Sim( *theIcon );
#endif
        }

        Matrix3 iconTM;
        iconTM.SetScale( Point3( iconScale, iconScale, iconScale ) );

        objToPopulate.SetViewportIcon( theIconMeshes, theIcon, iconTM );
    }
}

void SimEmberBase::ResetCache() {
    try {
        m_dataCache.clear();
        m_dataCache.wait_for_pending();

        m_serializer.process_errors();
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }

    // We've potentially changed the particles that should be displayed.
    NotifyDependents( FOREVER, PART_GEOM, REFMSG_CHANGE );
}

void SimEmberBase::FlushCache() {
    try {
        m_dataCache.flush( false );
        m_dataCache.wait_for_pending();

        m_serializer.process_errors();
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

namespace {
inline void log_exception( const boost::shared_ptr<Value>& pFunction, const MAXScriptException& e ) {
    FF_LOG( error ) << _T("In function: ") << frantic::max3d::mxs::to_string( pFunction.get() ) << _T( '\n' )
                    << frantic::max3d::mxs::to_string( e ) << std::endl;
}

template <class T, class Enabler = void>
struct function_type {
    typedef typename T::function_type type;
};

template <class Fn>
struct function_type<Fn, typename boost::enable_if<boost::is_function<Fn>>::type> {
    typedef Fn type;
};

template <class Callable, int NumArgs>
class dispatch_to_invoke_queue;

template <class Callable>
class dispatch_to_invoke_queue<Callable, 0> {
    typedef typename function_type<Callable>::type function_type;
    typedef void result_type;

    Callable m_fn;
    frantic::win32::invoke_queue* m_pQueue;

  public:
    dispatch_to_invoke_queue( frantic::win32::invoke_queue& invokeQueue, const Callable& fn )
        : m_fn( fn )
        , m_pQueue( &invokeQueue ) {}

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    dispatch_to_invoke_queue( frantic::win32::invoke_queue& invokeQueue, Callable&& fn )
        : m_fn( std::move( fn ) )
        , m_pQueue( &invokeQueue ) {}
#endif

    void operator()() const {
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
        m_pQueue->invoke( std::move( m_fn ) );
#else
        m_pQueue->invoke( m_fn );
#endif
    }
};

template <class Callable>
class dispatch_to_invoke_queue<Callable, 1> {
    typedef typename function_type<Callable>::type function_type;
    typedef void result_type;
    typedef typename boost::function_traits<function_type>::arg1_type arg1_type;

    Callable m_fn;
    frantic::win32::invoke_queue* m_pQueue;

  public:
    dispatch_to_invoke_queue( frantic::win32::invoke_queue& invokeQueue, const Callable& fn )
        : m_fn( fn )
        , m_pQueue( &invokeQueue ) {}

    void operator()( arg1_type a1 ) const { m_pQueue->invoke( m_fn, a1 ); }
};

template <class Callable>
class dispatch_to_invoke_queue<Callable, 2> {
    typedef typename function_type<Callable>::type function_type;
    typedef void result_type;
    typedef typename boost::function_traits<function_type>::arg1_type arg1_type;
    typedef typename boost::function_traits<function_type>::arg2_type arg2_type;

    Callable m_fn;
    frantic::win32::invoke_queue* m_pQueue;

  public:
    dispatch_to_invoke_queue( frantic::win32::invoke_queue& invokeQueue, const Callable& fn )
        : m_fn( fn )
        , m_pQueue( &invokeQueue ) {}

    void operator()( arg1_type a1, arg2_type a2 ) const { m_pQueue->invoke( m_fn, a1, a2 ); }
};

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
template <class Callable>
dispatch_to_invoke_queue<Callable, boost::function_traits<typename function_type<Callable>::type>::arity>
make_dispatcher_to_invoke_queue( frantic::win32::invoke_queue& queue, Callable&& fn ) {
    return dispatch_to_invoke_queue<Callable, boost::function_traits<typename function_type<Callable>::type>::arity>(
        queue, std::forward<Callable>( fn ) );
}
#endif

template <class Callable>
dispatch_to_invoke_queue<Callable, boost::function_traits<typename function_type<Callable>::type>::arity>
make_dispatcher_to_invoke_queue( frantic::win32::invoke_queue& queue, const Callable& fn ) {
    return dispatch_to_invoke_queue<Callable, boost::function_traits<typename function_type<Callable>::type>::arity>(
        queue, fn );
}
} // namespace

void SimEmberBase::FlushCacheAsync( Value* pCallback ) {
    try {
        if( !pCallback || !is_function( pCallback ) )
            throw MAXException( _T("FlushParticleCacheAsync expected parameter to be a MAXScript function") );

        m_dataCache.flush_async( make_dispatcher_to_invoke_queue(
            GetMainThreadDispatcher(),
            frantic::max3d::mxs::shared_value_functor<void>( frantic::max3d::mxs::make_shared_value( pCallback ) ) ) );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void SimEmberBase::CancelFlushCache() {
    try {
        m_dataCache.cancel_flush();
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void SimEmberBase::InitializeCache( const MCHAR* szSourceSequencePattern ) {
    try {
        frantic::tstring sourceSequencePattern( szSourceSequencePattern ? szSourceSequencePattern : _T("") );

        // Drop any existing data, because we want to have our cache synchronize completely with the target disk
        // sequence.
        m_dataCache.clear();

        // Enable the disk cache
        m_pblock->SetValue( kUseDiskCache, 0, TRUE );

        // This will set the pblock value, but it doesn't synchronize with the existing files on disk.
        m_pblock->SetValue( kSequencePath, 0, const_cast<MCHAR*>( sourceSequencePattern.c_str() ) );

        // Assign the path again, but flag it for synchronizing the cache with the files already in the location.
        m_dataCache.set_disk_path( sourceSequencePattern, SimEmberBase::cache_type::synchronize );

        // TODO: We could start deserializing some items in order to fill the memory cache ...

        // We've potentially changed the particles that should be displayed.
        NotifyDependents( FOREVER, PART_GEOM, REFMSG_CHANGE );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void SimEmberBase::AddData( FPInterface* pData, double cacheFrame ) {
    // MXSField* pMXSField = GetMXSFieldInterface( pData );
    // if( !pMXSField )
    //	throw MAXException( _T("Expected an Ember field object") );
    SimEmberSimulator* sim = GetSimEmberSimulatorInterface( pData );
    if( !sim )
        throw MAXException( _T("Expected an Ember field collection") );

    serializer_data data;

    data.pField = sim->get_field_collection(); // TODO: Convert to VDB grids here!
    data.bounds = frantic::max3d::from_max_t( sim->get_worldspace_bounds() );
    data.spacing = sim->get_spacing();
    data.memoryUsage = calculate_memory_usage( data.pField );

    assert( data.pField && frantic::math::is_finite( data.spacing ) );

    m_dataCache.insert( cacheFrame, data );

    // We've potentially changed our geometry.
    NotifyDependents( FOREVER, PART_GEOM, REFMSG_CHANGE );

    try {
        m_serializer.process_errors();
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

Tab<double> SimEmberBase::GetCacheTimes() {
    Tab<double> result;

    for( cache_type::const_key_iterator it = m_dataCache.key_begin(), itEnd = m_dataCache.key_end(); it != itEnd;
         ++it ) {
        double f = *it;

        result.Append( 1, &f );
    }

    return result;
}

Tab<double> SimEmberBase::GetMemoryCacheTimes() {
    Tab<double> result;

    for( cache_type::const_memory_iterator it = m_dataCache.memory_begin(), itEnd = m_dataCache.memory_end();
         it != itEnd; ++it ) {
        double f = it.key();

        result.Append( 1, &f );
    }

    return result;
}

void SimEmberBase::SetNumSerializerThreads( int numThreads ) {
    try {
        m_dataCache.set_num_serializer_threads( static_cast<std::size_t>( std::max( 1, numThreads ) ) );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void SimEmberBase::SetSerializerCallback( Value* pCallback ) {
    try {
        if( !pCallback || pCallback == &undefined ) {
            m_dataCache.get_serializer().set_callback( boost::function<void( const frantic::tstring& )>() );
        } else {
            if( !is_function( pCallback ) )
                throw MAXException( _T("SetSerializerCallback expected parameter to be a MAXScript function") );

            m_dataCache.get_serializer().set_callback( make_dispatcher_to_invoke_queue(
                GetMainThreadDispatcher(), frantic::max3d::mxs::shared_value_functor<void, frantic::tstring>(
                                               frantic::max3d::mxs::make_shared_value( pCallback ) ) ) );
        }
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void SimEmberBase::SetIconScale( float scale ) { frantic::max3d::set<float>( m_pblock, kIconSize, 0, scale ); }

ClassDesc2* SimEmberBase::GetClassDesc() { return GetSimEmberBaseDesc(); }

// Fwd decl.
Mesh* GetStokeMesh();

field_interface_ptr SimEmberBase::GetNearestSample( double /*frame*/, double& /*outTimeOffset*/ ) {
    /*double nearestTime = m_dataCache.find_nearest_key( frame );

    if( !frantic::math::is_finite(nearestTime) )
            return field_interface_ptr();

    outTimeOffset = frame - nearestTime;

    return m_dataCache.find( nearestTime );*/
    return field_interface_ptr();
}

std::pair<field_interface_ptr, field_interface_ptr>
SimEmberBase::GetBracketingSamples( double /*frame*/, double& /*outTimeOffset*/, double& /*outTimeStep*/ ) {
    // std::pair<double,double> bracketTimes = m_dataCache.find_bracketing_keys( frame );

    // if( !frantic::math::is_finite(bracketTimes.first) ){ // We are treating just the left and both infinite the same
    // since we don't velocity offset before the first frame. 	outTimeStep = std::numeric_limits<double>::infinity();
    //	outTimeOffset = 0.f;

    //	return std::make_pair( field_interface_ptr(), field_interface_ptr() );
    //}else if( !frantic::math::is_finite(bracketTimes.second) || bracketTimes.first == frame ){ // If we queried
    //exactly at the sample time, just return one sample. 	outTimeStep = std::numeric_limits<double>::infinity();
    //	outTimeOffset = frame - bracketTimes.first;

    //	return std::make_pair( m_dataCache.find( bracketTimes.first ), field_interface_ptr() );
    //}else{
    //	double leftOffset = frame - bracketTimes.first;
    //	double rightOffset = bracketTimes.second - frame;

    //	std::pair<field_interface_ptr,field_interface_ptr> result;

    //	if( leftOffset <= rightOffset ){
    //		outTimeStep = bracketTimes.second - bracketTimes.first;
    //		outTimeOffset = leftOffset;
    //		result.first = m_dataCache.find( bracketTimes.first );
    //		result.second = m_dataCache.find( bracketTimes.second );
    //	}else{
    //		outTimeStep = bracketTimes.first - bracketTimes.second;
    //		outTimeOffset = -rightOffset;
    //		result.first = m_dataCache.find( bracketTimes.second );
    //		result.second = m_dataCache.find( bracketTimes.first );
    //	}

    //	return result;
    //}

    return std::pair<field_interface_ptr, field_interface_ptr>();
}

field_interface_ptr SimEmberBase::CreateEmptySample( const frantic::channels::channel_map& /*outMap*/ ) const {
    field_interface_ptr pData;

    // Find the first non-null entry in memory.
    for( cache_type::const_memory_iterator it = m_dataCache.memory_begin(), itEnd = m_dataCache.memory_end();
         it != itEnd && !pData; ++it )
        pData = it.value().pField;

    // TODO: Set the empty field to have the same channels as pData;
    return field_interface_ptr();
}

} // namespace max3d
} // namespace ember

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource.h"

#include <stoke/max3d/ParticleSim/BaseInterfacePtr.hpp>
#include <stoke/max3d/ParticleSim/IReflowSystem.hpp>
#include <stoke/max3d/ParticleSim/StokeBase.hpp>
#include <stoke/max3d/ParticleSim/StokeBaseParamBlock.hpp>

#include <stoke/particle_set_istream.hpp>

#include <stoke/max3d/EmberHolder.hpp>

#include <frantic/max3d/channels/channel_map_interop.hpp>
#include <frantic/max3d/exception.hpp>
#include <frantic/max3d/max_utility.hpp>
#include <frantic/max3d/maxscript/maxscript.hpp> // TODO: This is a ugly header performance-wise. Try not to use it.
#include <frantic/max3d/time.hpp>

#include <frantic/magma/max3d/IErrorReporter.hpp>

#include <frantic/channels/channel_map_adaptor.hpp>
#include <frantic/particles/particle_file_stream_factory.hpp>
#include <frantic/particles/streams/age_culled_particle_istream.hpp>
#include <frantic/particles/streams/apply_function_particle_istream.hpp>
#include <frantic/particles/streams/duplicate_channel_particle_istream.hpp>
#include <frantic/particles/streams/empty_particle_istream.hpp>
#include <frantic/particles/streams/fractional_by_id_particle_istream.hpp>
#include <frantic/particles/streams/set_channel_particle_istream.hpp>
#include <frantic/particles/streams/time_interpolation_particle_istream.hpp>

#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/function.hpp>

#if MAX_VERSION_MAJOR >= 25
#include <SharedMesh.h>
#endif

#pragma warning( push, 3 )
#include <tbb/parallel_reduce.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tbb_allocator.h>
#pragma warning( pop )

#define StokeBase_CLASSID Class_ID( 0x6c091f90, 0x4f4823a5 )
#define StokeBase_NAME "StokeBase"
#define StokeBase_VERSION 1

namespace stoke {
namespace max3d {
void InvokeInMainThread( const boost::function<void( void )>& fn );
}
} // namespace stoke

namespace stoke {
namespace max3d {

ClassDesc2* GetStokeBaseDesc();

IStokeBase::~IStokeBase() {}

void IStokeBase::AddParticleSetMXS( FPInterface* pParticleSetInterface, TimeValue cacheTime ) {
    if( !pParticleSetInterface )
        throw MAXException( _T("Invalid parameter") );

    IParticleSet* pParticleSet =
        static_cast<IParticleSet*>( pParticleSetInterface->GetInterface( ParticleSet_INTERFACE ) );

    if( !pParticleSet )
        throw MAXException( _T("Parameter must be a ParticleSet") );

    this->AddParticleSet( pParticleSet, cacheTime );
}

void IStokeBase::InitInterfaceDesc( FPInterfaceDesc& inoutDesc ) {
    inoutDesc.AppendFunction( kFnResetParticleCache, _T("ResetParticleCache"), 0, TYPE_VOID, 0, 0, p_end );
    inoutDesc.AppendFunction( kFnFlushParticleCache, _T("FlushParticleCache"), 0, TYPE_VOID, 0, 0, p_end );
    inoutDesc.AppendFunction( kFnFlushParticleCacheAsync, _T("FlushParticleCacheAsync"), 0, TYPE_VOID, 0, 1,
                              _T("Callback"), 0, TYPE_VALUE, p_end );
    inoutDesc.AppendFunction( kFnCancelFlushParticleCache, _T("CancelFlushParticleCache"), 0, TYPE_VOID, 0, 0, p_end );
    inoutDesc.AppendFunction( kFnSetChannelsToSave, _T("SetChannelsToSave"), 0, TYPE_VOID, 0, 1, _T("Channels"), 0,
                              TYPE_STRING_TAB, p_end );
    inoutDesc.AppendFunction( kFnInitializeParticleCache, _T("InitializeParticleCache"), 0, TYPE_VOID, 0, 1,
                              _T("SourcePattern"), 0, TYPE_STRING, p_end );
    inoutDesc.AppendFunction( kFnAddParticleSet, _T("AddParticleSet"), 0, TYPE_VOID, 0, 2, _T("ParticleSet"), 0,
                              TYPE_INTERFACE, _T("Time"), 0, TYPE_TIMEVALUE, p_end );
    inoutDesc.AppendFunction( kFnWriteParticleCache, _T("WriteParticleCache"), 0, TYPE_VOID, 0, 2,
                              _T("DestinationPattern"), 0, TYPE_STRING, _T("FromMemoryOnly"), 0, TYPE_bool,
                              f_keyArgDefault, false, p_end );
    inoutDesc.AppendFunction( kFnGetParticleCacheTimes, _T("GetParticleCacheTimes"), 0, TYPE_TIMEVALUE_TAB_BV, 0, 0,
                              p_end );
    inoutDesc.AppendFunction( kFnGetMemoryCacheTimes, _T("GetMemoryCacheTimes"), 0, TYPE_TIMEVALUE_TAB_BV, 0, 0,
                              p_end );
    inoutDesc.AppendFunction( kFnSetNumSerializerThreads, _T("SetNumSerializerThreads"), 0, TYPE_VOID, 0, 1,
                              _T("NumThreads"), 0, TYPE_INT, p_end );
    inoutDesc.AppendFunction( kFnSetSerializerCallback, _T("SetSerializerCallback"), 0, TYPE_VOID, 0, 1, _T("Callback"),
                              0, TYPE_VALUE, p_end );
    inoutDesc.AppendFunction( kFnInitSimMagmaHolder, _T( "InitSimMagmaHolder" ), 0, TYPE_VOID, 0, 0, p_end );
}

class StokeBaseDesc : public ClassDesc2 {
    ParamBlockDesc2 m_paramDesc;
    FPInterfaceDesc m_StokeBaseDesc;
    FPInterfaceDesc m_errorReporterInterface;

    friend class StokeBase;

  public:
    StokeBaseDesc();

    int IsPublic() { return FALSE; }
    void* Create( BOOL loading ) { return new StokeBase( loading ); }
    const TCHAR* ClassName() { return _T( StokeBase_NAME ); }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return StokeBase_CLASSID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    const TCHAR* InternalName() { return _T( StokeBase_NAME ); } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }                  // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( StokeBase_NAME ); }
#endif
};

StokeBaseDesc::StokeBaseDesc()
    : m_paramDesc( 0,                                                                          // Block num
                   _T("Parameters"),                                                           // Internal name
                   NULL,                                                                       // Localized name
                   NULL,                                                                       // ClassDesc2*
                   P_AUTO_CONSTRUCT + P_AUTO_UI + P_MULTIMAP + P_VERSION + P_CALLSETS_ON_LOAD, // Flags
                   StokeBase_VERSION,
                   0, // PBlock Ref Num
                   0, // multimap count
                   p_end )
    ,

    m_StokeBaseDesc( StokeBase_INTERFACE, _T("StokeBase"), 0, NULL, FP_MIXIN, p_end )
    , m_errorReporterInterface( ErrorReporter_INTERFACE, _T( "ErrorReporter" ), 0, NULL, FP_MIXIN, p_end ) {
    m_paramDesc.SetClassDesc( this );
    m_StokeBaseDesc.SetClassDesc( this );

    IStokeBase::InitInterfaceDesc( m_StokeBaseDesc );

    InitParamBlockDesc( m_paramDesc );

    m_errorReporterInterface.SetClassDesc( this );

    frantic::magma::max3d::IErrorReporter::init_fpinterface_desc( m_errorReporterInterface );

    this->AddInterface( frantic::magma::max3d::ErrorReporter::GetStaticDesc() );
}

ClassDesc2* GetStokeBaseDesc() {
    static StokeBaseDesc theStokeBaseDesc;
    return &theStokeBaseDesc;
}

StokeBase::StokeBase( BOOL loading )
    : m_particleCache( DEFAULT_CACHE_SIZE, m_serializer ) {
    GetStokeBaseDesc()->MakeAutoParamBlocks( this );

    if( !loading ) {
        // Invoke the PBAccessor handlers for default values
        m_pblock->CallSet( kUseDiskCache );
        m_pblock->CallSet( kSequenceCacheCapacity );
        m_pblock->CallSet( kSerializeQueueCapacity );
        m_pblock->CallSet( kSequencePath );
    }

    m_iparticleobjectext = krakatoa::max3d::CreatePRTParticleObjectExt( this );
}

StokeBase::~StokeBase() {}

FPInterfaceDesc* StokeBase::GetDescByID( Interface_ID id ) {
    if( id == StokeBase_INTERFACE )
        return &static_cast<StokeBaseDesc*>( GetStokeBaseDesc() )->m_StokeBaseDesc;
    if( id == ErrorReporter_INTERFACE )
        return &static_cast<StokeBaseDesc*>( GetStokeBaseDesc() )->m_errorReporterInterface;
    return &FPInterface::nullInterface;
}

BaseInterface* StokeBase::GetInterface( Interface_ID id ) {
    if( id == StokeBase_INTERFACE )
        return static_cast<IStokeBase*>( this );

    if( id == PARTICLEOBJECTEXT_INTERFACE )
        return m_iparticleobjectext.get();
    if( BaseInterface* result = ErrorReporter::GetInterface( id ) )
        return result;

    if( BaseInterface* bi = IStokeBase::GetInterface( id ) )
        return bi;

    return PRTObject<StokeBase>::GetInterface( id );
}

class StokeBase::StokeBasePLC : public PostLoadCallback {
    StokeBase* m_pOwner;

  public:
    explicit StokeBasePLC( StokeBase* pOwner )
        : m_pOwner( pOwner ) {}

    virtual void proc( ILoad* iload ) {
        if( m_pOwner && m_pOwner->m_pblock && m_pOwner->m_pblock->GetInt( kUseDiskCache ) )
            m_pOwner->m_particleCache.set_disk_path( m_pOwner->m_pblock->GetStr( kSequencePath ),
                                                     StokeBase::cache_type::synchronize );

        delete this;
    }
};

IOResult StokeBase::Load( ILoad* iload ) {
    IOResult result = krakatoa::max3d::PRTObject<StokeBase>::Load( iload );

    if( result == IO_OK )
        iload->RegisterPostLoadCallback( new StokeBasePLC( this ) );

    return result;
}

RefResult StokeBase::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& /*partID*/,
                                       RefMessage message, BOOL /*propagate*/ ) {
    if( hTarget == m_pblock ) {
        int tabIndex = -1;
        ParamID paramID = m_pblock->LastNotifyParamID( tabIndex );

        if( message == REFMSG_CHANGE ) {
            bool affectsViewport = false;
            bool affectsObject = false;

            switch( paramID ) {
            case kIconSize:
                // We send the display change notification, but don't invalidate the viewport particles because only the
                // Display method is affected.
                this->NotifyDependents( FOREVER, PART_DISPLAY, REFMSG_CHANGE );

                return REF_STOP;
            case kViewportEnabled:
            case kViewportPercentage:
            case kViewportVectorChannel:
            case kViewportColorChannel:
            case kUseViewportLimit:
            case kViewportVectorScale:
            case kViewportVectorNormalize:
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
                this->InvalidateObjectAndViewport();

                this->NotifyDependents( FOREVER, PART_GEOM, REFMSG_CHANGE );

                return REF_STOP;
            } else if( affectsViewport ) {
                // Convert this message into a viewport display change message.
                this->InvalidateViewport();

                this->NotifyDependents( FOREVER, PART_DISPLAY, REFMSG_CHANGE );

                return REF_STOP;
            }
        }
    }

    return REF_SUCCEED;
}

void StokeBase::ResetParticleCache() {
    try {
        m_particleCache.clear();
        m_particleCache.wait_for_pending();

        m_serializer.process_errors();
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }

    // We've potentially changed the particles that should be displayed.
    NotifyDependents( FOREVER, PART_GEOM, REFMSG_CHANGE );
}

void StokeBase::FlushParticleCache() {
    try {
        m_particleCache.flush( false );
        m_particleCache.wait_for_pending();

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

void make_collectable( Value* pVal ) { pVal->make_collectable(); }

inline boost::shared_ptr<Value> make_shared_value( Value* pVal ) {
    return boost::shared_ptr<Value>( pVal->make_heap_static(), &make_collectable );
}

template <class T>
struct invoke_mxs_function;

template <>
struct invoke_mxs_function<void( void )> {
    inline static void apply( const boost::shared_ptr<Value>& pFunction ) {
        try {
            pFunction->apply( NULL, 0 );
        } catch( const MAXScriptException& e ) {
            log_exception( pFunction, e );
        }
    }
};

template <class P>
struct invoke_mxs_function<void( const P& )> {
    inline static void apply( const boost::shared_ptr<Value>& pFunction, const P& p ) {
        try {
            Value** locals = NULL;
            value_local_array( locals, 1 );

            locals[0] = frantic::max3d::fpwrapper::MaxTypeTraits<P>::to_value( p );

            pFunction->apply( locals, 1 );
#if MAX_VERSION_MAJOR < 19
            pop_value_local_array( locals );
#endif
        } catch( const MAXScriptException& e ) {
            log_exception( pFunction, e );
        }
    }
};

template <class P1, class P2>
struct invoke_mxs_function<void( const P1&, const P2& )> {
    inline static void apply( const boost::shared_ptr<Value>& pFunction, const P1& p1, const P2& p2 ) {
        try {
            Value** locals = NULL;
            value_local_array( locals, 2 );

            locals[0] = frantic::max3d::fpwrapper::MaxTypeTraits<P1>::to_value( p1 );
            locals[1] = frantic::max3d::fpwrapper::MaxTypeTraits<P2>::to_value( p2 );

            pFunction->apply( locals, 2 );
#if MAX_VERSION_MAJOR < 19
            pop_value_local_array( locals );
#endif
        } catch( const MAXScriptException& e ) {
            log_exception( pFunction, e );
        }
    }
};

void rethrow_in_main_thread( const boost::exception_ptr& e ) {
    // We can't actually throw the exception because it will crash the program.
    // TODO: We can have an installable error handler that gets invoked.
    FF_LOG( error ) << frantic::strings::to_tstring( boost::diagnostic_information( e ) ) << std::endl;
}

template <class T>
class invoke_in_main_thread;

template <>
class invoke_in_main_thread<void( void )> {
    boost::function<void( void )> m_impl;

  public:
    template <class Callable>
    explicit invoke_in_main_thread( const Callable& impl )
        : m_impl( impl ) {}

    void operator()() const { InvokeInMainThread( m_impl ); }

    void operator()( const boost::exception_ptr& e ) const {
        InvokeInMainThread( boost::bind( &rethrow_in_main_thread, e ) );
    }
};

template <class P1>
class invoke_in_main_thread<void( const P1& )> {
    boost::function<void( const P1& )> m_impl;

  public:
    template <class Callable>
    explicit invoke_in_main_thread( const Callable& impl )
        : m_impl( impl ) {}

    void operator()( const P1& p1 ) const { InvokeInMainThread( boost::bind( m_impl, p1 ) ); }

    void operator()( const boost::exception_ptr& e ) const {
        InvokeInMainThread( boost::bind( &rethrow_in_main_thread, e ) );
    }
};

template <class P1, class P2>
class invoke_in_main_thread<void( const P1&, const P2& )> {
    boost::function<void( const P1&, const P2& )> m_impl;

  public:
    template <class Callable>
    explicit invoke_in_main_thread( const Callable& impl )
        : m_impl( impl ) {}

    void operator()( const P1& p1, const P2& p2 ) const { InvokeInMainThread( boost::bind( m_impl, p1, p2 ) ); }

    void operator()( const boost::exception_ptr& e ) const {
        InvokeInMainThread( boost::bind( &rethrow_in_main_thread, e ) );
    }
};
} // namespace

void StokeBase::FlushParticleCacheAsync( Value* pCallback ) {
    try {
        if( !pCallback || !is_function( pCallback ) )
            throw MAXException( _T("FlushParticleCacheAsync expected parameter to be a MAXScript function") );

        // Prevent garbage collection from killing this object.
        // TODO: We could use a shared_ptr to reference count and then make it collectable again.
        pCallback = pCallback->make_heap_static(); // Static allows it to live across 3ds Max scene reset.

        m_particleCache.flush_async( invoke_in_main_thread<void( void )>(
            boost::bind( &invoke_mxs_function<void( void )>::apply, make_shared_value( pCallback ) ) ) );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void StokeBase::CancelFlushParticleCache() {
    try {
        m_particleCache.cancel_flush();
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void StokeBase::SetChannelsToSave( const Tab<const MCHAR*>* pChannels ) {
    try {
        frantic::channels::channel_map saveMap;
        frantic::max3d::channels::set_channel_map( saveMap, *pChannels );

        if( !saveMap.has_channel( _T("Position") ) ||
            ( saveMap[_T("Position")].data_type() != frantic::channels::data_type_float32 ||
              saveMap[_T("Position")].arity() != 3 ) )
            throw std::runtime_error( "Position channel must be present and of type \"float32[3]\"" );

        if( saveMap.has_channel( _T("Velocity") ) &&
            ( !frantic::channels::is_channel_data_type_float( saveMap[_T("Velocity")].data_type() ) ||
              saveMap[_T("Velocity")].arity() != 3 ) )
            throw std::runtime_error( "Velocity channel must be \"float##[3]\"" );

        if( saveMap.has_channel( _T("Age") ) &&
            ( !frantic::channels::is_channel_data_type_float( saveMap[_T("Age")].data_type() ) ||
              saveMap[_T("Age")].arity() != 1 ) )
            throw std::runtime_error( "Age channel must be \"float##[1]\"" );

        if( saveMap.has_channel( _T("LifeSpan") ) &&
            ( !frantic::channels::is_channel_data_type_float( saveMap[_T("LifeSpan")].data_type() ) ||
              saveMap[_T("LifeSpan")].arity() != 1 ) )
            throw std::runtime_error( "LifeSpan channel must be \"float##[1]\"" );

        if( saveMap.has_channel( _T("ID") ) &&
            ( !frantic::channels::is_channel_data_type_int( saveMap[_T("ID")].data_type() ) ||
              saveMap[_T("ID")].arity() != 1 ) )
            throw std::runtime_error( "ID channel must be \"int##[1]\"" );

        m_particleCache.flush( false );
        m_particleCache.wait_for_pending();

        m_serializer.process_errors();

        m_serializer.override_output_map( saveMap );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void StokeBase::InitializeParticleCache( const MCHAR* szSourceSequencePattern ) {
    try {
        frantic::tstring sourceSequencePattern( szSourceSequencePattern ? szSourceSequencePattern : _T("") );

        // Drop any existing data, because we want to have our cache synchronize completely with the target disk
        // sequence.
        m_particleCache.clear();

        // Enable the disk cache
        m_pblock->SetValue( kUseDiskCache, 0, TRUE );

        // This will set the pblock value, but it doesn't synchronize with the existing files on disk.
        m_pblock->SetValue( kSequencePath, 0, const_cast<MCHAR*>( sourceSequencePattern.c_str() ) );

        // Assign the path again, but flag it for synchronizing the cache with the files already in the location.
        m_particleCache.set_disk_path( sourceSequencePattern, StokeBase::cache_type::synchronize );

        // TODO: We could start deserializing some items in order to fill the memory cache ...

        // We've potentially changed the particles that should be displayed.
        NotifyDependents( FOREVER, PART_GEOM, REFMSG_CHANGE );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void StokeBase::AddParticleSet( IParticleSet* pParticleSet, TimeValue cacheTime ) {
    double frame = static_cast<double>( cacheTime ) / static_cast<double>( GetTicksPerFrame() );

    try {
        boost::shared_ptr<stoke::particle_set_interface> pParticleSetCopy;

        pParticleSetCopy = pParticleSet->GetImpl()->clone();

        m_particleCache.insert( frame, pParticleSetCopy );
    } catch( std::runtime_error& e ) {
        FF_LOG( error ) << e.what() << std::endl;
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }

    // We've potentially changed the particles that should be displayed.
    NotifyDependents( FOREVER, PART_GEOM, REFMSG_CHANGE );

    try {
        m_serializer.process_errors();
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void StokeBase::WriteParticleCache( const MCHAR* szDestSequencePattern, bool memoryOnly ) const {
    try {
        frantic::files::filename_sequence sequence( szDestSequencePattern ? szDestSequencePattern : _T("") );

        // This call has shitty error handling
        // sequence.create_directory();

        try {
            boost::filesystem::create_directories( sequence.get_filename_pattern().get_directory( false ) );
        } catch( const boost::filesystem::filesystem_error& e ) {
            std::stringstream ss;

            ss << "Failed to create directory:\n  \""
               << frantic::strings::to_string( sequence.get_filename_pattern().get_directory( false ) );
            ss << "\" because:\n\tError: " << e.code().message() << " (" << e.code().value() << ")";
            ss << "\n\tPath: " << e.path1().string<std::string>();
            if( !e.path2().empty() )
                ss << "\n\n\tOther Path: " << e.path2().string<std::string>();

            throw std::runtime_error( ss.str() );
        }

        if( !memoryOnly ) {
            for( cache_type::const_key_iterator it = m_particleCache.key_begin(), itEnd = m_particleCache.key_end();
                 it != itEnd; ++it )
                m_serializer.serialize_immediate( sequence[*it], m_particleCache.find( *it ) );
        } else {
            for( cache_type::const_memory_iterator it = m_particleCache.memory_begin(),
                                                   itEnd = m_particleCache.memory_end();
                 it != itEnd; ++it )
                m_serializer.serialize_immediate( sequence[it.key()], it.value() );
        }

    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

Tab<TimeValue> StokeBase::GetParticleCacheTimes() const {
    Tab<TimeValue> result;

    double ticksPerFrame = static_cast<double>( GetTicksPerFrame() );

    for( cache_type::const_key_iterator it = m_particleCache.key_begin(), itEnd = m_particleCache.key_end();
         it != itEnd; ++it ) {
        TimeValue t = static_cast<TimeValue>( ticksPerFrame * ( *it ) );

        result.Append( 1, &t );
    }

    return result;
}

Tab<TimeValue> StokeBase::GetMemoryCacheTimes() const {
    Tab<TimeValue> result;

    double ticksPerFrame = static_cast<double>( GetTicksPerFrame() );

    for( cache_type::const_memory_iterator it = m_particleCache.memory_begin(), itEnd = m_particleCache.memory_end();
         it != itEnd; ++it ) {
        TimeValue t = static_cast<TimeValue>( ticksPerFrame * it.key() );

        result.Append( 1, &t );
    }

    return result;
}

void StokeBase::SetNumSerializerThreads( int numThreads ) {
    try {
        m_particleCache.set_num_serializer_threads( static_cast<std::size_t>( std::max( 1, numThreads ) ) );
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void StokeBase::SetSerializerCallback( Value* pCallback ) {
    try {
        if( !pCallback || pCallback == &undefined ) {
            m_particleCache.get_serializer().set_callback( boost::function<void( const frantic::tstring& )>() );
        } else {
            if( !is_function( pCallback ) )
                throw MAXException( _T("SetSerializerCallback expected parameter to be a MAXScript function") );

            // Prevent garbage collection from killing this object.
            // TODO: We could use a shared_ptr to reference count and then make it collectable again.
            pCallback = pCallback->make_heap_static(); // Static allows it to live across 3ds Max scene reset.

            m_particleCache.get_serializer().set_callback( invoke_in_main_thread<void( const frantic::tstring& )>(
                boost::bind( &invoke_mxs_function<void( const frantic::tstring& )>::apply,
                             make_shared_value( pCallback ), _1 ) ) );
        }
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }
}

void StokeBase::InitSimMagmaHolder() {
    m_pblock->SetValue( kSimulationMagmaFlow, 0, new ember::max3d::EmberHolder( STOKE_SIM_NODE_SET ) );
    m_pblock->SetValue( kBirthMagmaFlow, 0, new ember::max3d::EmberHolder( STOKE_SIM_NODE_SET ) );
}

ClassDesc2* StokeBase::GetClassDesc() { return GetStokeBaseDesc(); }

// Fwd decl.
Mesh* GetStokeMesh();

Mesh* StokeBase::GetIconMesh( Matrix3& outTM ) {
    float scale = m_pblock->GetFloat( kIconSize );

    outTM.SetScale( Point3( scale, scale, scale ) );

    return GetStokeMesh();
}

#if MAX_VERSION_MAJOR >= 25
// Fwd decl.
MaxSDK::SharedMeshPtr GetStokeMeshShared();

MaxSDK::SharedMeshPtr StokeBase::GetIconMeshShared( Matrix3& outTM ) {
    float scale = m_pblock->GetFloat( kIconSize );

    outTM.SetScale( Point3( scale, scale, scale ) );

    return GetStokeMeshShared();
}
#endif

void StokeBase::SetIconSize( float scale ) { m_pblock->SetValue( kIconSize, 0, scale ); }

bool StokeBase::InWorldSpace() const { return true; }

struct time_offset_op {
    static vec3 apply( const vec3& p, const vec3& v, float timeStepSeconds ) { return p + timeStepSeconds * v; }

    inline static particle_istream_ptr apply_to_stream( particle_istream_ptr pin, float timeOffsetSeconds ) {
        if( std::abs( timeOffsetSeconds ) > 1e-5f ) {
            frantic::tstring destChannel = _T("Position");

            boost::array<frantic::tstring, 2> srcChannels = { _T("Position"), _T("Velocity") };

            pin.reset(
                new frantic::particles::streams::apply_function_particle_istream<vec3( const vec3&, const vec3& )>(
                    pin, boost::bind( &time_offset_op::apply, _1, _2, timeOffsetSeconds ), destChannel, srcChannels ) );
        }

        return pin;
    }
};

struct velocity_scale_op {
    static vec3 apply( const vec3& v, float scale ) { return v * scale; }

    inline static particle_istream_ptr apply_to_stream( particle_istream_ptr pin, float scale ) {
        if( std::abs( 1.f - scale ) > 1e-5f ) {
            frantic::tstring destChannel = _T("Velocity");
            boost::array<frantic::tstring, 1> srcChannels = { _T("Velocity") };

            pin.reset( new frantic::particles::streams::apply_function_particle_istream<vec3( const vec3& )>(
                pin, boost::bind( &velocity_scale_op::apply, _1, scale ), destChannel, srcChannels ) );
        }

        return pin;
    }
};

struct viewport_vector_op {
    static vec3 apply( const vec3& v, float scale ) { return v * scale; }

    static vec3 apply_normalized( const vec3& v, float scale ) { return vec3::normalize( v ) * scale; }

    inline static particle_istream_ptr apply_to_stream( particle_istream_ptr pin, const frantic::tstring& srcChannel,
                                                        float scale, bool preNormalize ) {
        if( srcChannel == _T("Velocity") && !preNormalize )
            scale /= static_cast<float>( GetFrameRate() );

        frantic::tstring destChannel = _T("PRTViewportVector");
        boost::array<frantic::tstring, 1> srcChannels = { srcChannel };

        if( srcChannels[0].empty() )
            srcChannels[0] = destChannel;

        if( preNormalize ) {
            pin.reset( new frantic::particles::streams::apply_function_particle_istream<vec3( const vec3& )>(
                pin, boost::bind( &viewport_vector_op::apply_normalized, _1, scale ), destChannel, srcChannels ) );
        } else if( std::abs( 1.f - scale ) > 1e-5f ) { // Its a waste of time to apply a 1.0 scale.
            pin.reset( new frantic::particles::streams::apply_function_particle_istream<vec3( const vec3& )>(
                pin, boost::bind( &viewport_vector_op::apply, _1, scale ), destChannel, srcChannels ) );
        } else if( srcChannels[0] != destChannel ) { // No need to copy a channel to itself.
            pin.reset( new frantic::particles::streams::duplicate_channel_particle_istream( pin, srcChannels[0],
                                                                                            destChannel ) );
        }

        return pin;
    }
};

struct viewport_color_op {
    static vec3 apply( float f ) { return vec3( f, f, f ); }

    inline static particle_istream_ptr apply_to_stream( particle_istream_ptr pin, const frantic::tstring& srcChannel,
                                                        const frantic::graphics::color3f& defaultColor ) {
        frantic::tstring destChannel = _T("PRTViewportColor");
        boost::array<frantic::tstring, 1> srcChannels = { srcChannel };

        if( srcChannel.empty() ) {
            // Bobo want's an empty string to not display any channel, so I need to overwrite with the node's wire
            // color.
            pin.reset( new frantic::particles::streams::set_channel_particle_istream<frantic::graphics::color3f>(
                pin, _T("PRTViewportColor"), defaultColor ) );
        } else if( pin->get_native_channel_map().has_channel( srcChannel ) ) {
            const frantic::channels::channel& ch = pin->get_native_channel_map()[srcChannel];

            if( frantic::channels::is_channel_data_type_float( ch.data_type() ) ) {
                if( ch.arity() == 1 ) {
                    // Convert the scalar to a greyscale color. Unfortunately we can't do this in-place, so the
                    // destination channel must be different.
                    // TODO: Is it possible to do an in-place conversion of a channel? What does that imply for the
                    // native_channel_map?
                    if( srcChannel == destChannel )
                        throw std::runtime_error( "PRTViewportColor channel must be float[3]" );
                    pin.reset( new frantic::particles::streams::apply_function_particle_istream<vec3( float )>(
                        pin, &viewport_color_op::apply, destChannel, srcChannels ) );
                } else if( ch.arity() == 3 ) {
                    if( srcChannel != destChannel )
                        pin.reset( new frantic::particles::streams::duplicate_channel_particle_istream(
                            pin, srcChannel, _T("PRTViewportColor") ) );
                }
            }
        }

        return pin;
    }
};

// Arbitrarily using 10 ticks as the finite differences offset when calculating the derivative of the playback graph.
#define TICK_OFFSET 10

particle_istream_ptr
StokeBase::GetInternalStream( INode* pNode, TimeValue t, Interval& inoutValidity,
                              frantic::max3d::particles::IMaxKrakatoaPRTEvalContextPtr pEvalContext ) {
    if( !m_inRenderMode && !m_pblock->GetInt( kViewportEnabled ) )
        return this->CreateEmptyStream( pEvalContext->GetDefaultChannels() );

    int frameRate = GetFrameRate();

    double playbackTime;
    double playbackDerivative = 1.0; // If we are using the playback controller, we need to know the derivative in order
                                     // to modify time dependent channels like Velocity.

    bool usePlaybackTime = m_pblock->GetInt( kUsePlaybackTime ) != FALSE;

    if( usePlaybackTime ) {
        double playbackTime2;

        playbackTime = m_pblock->GetFloat( kPlaybackTime, t );
        playbackTime2 = m_pblock->GetFloat( kPlaybackTime, t + TICK_OFFSET );

        // Derivative is a ratio, and therefore doesn't need to be converted to seconds.
        playbackDerivative =
            ( playbackTime2 - playbackTime ) / frantic::max3d::to_frames<double>( TICK_OFFSET, frameRate );
    } else {
        playbackTime = frantic::max3d::to_frames<double>( t, frameRate );
    }

    inoutValidity &= Interval( t, t );

    particle_istream_ptr pResult;

    bool usePlaybackInterpolation = m_pblock->GetInt( kUsePlaybackInterpolation ) != FALSE;

    if( usePlaybackInterpolation ) {
        double timeOffset = 0;
        double timeStep = 0;

        std::pair<particle_set_interface_ptr, particle_set_interface_ptr> closestSets =
            this->GetBracketingSamples( playbackTime, timeOffset, timeStep );

        // If we have no 'left' sample, return an empty stream
        if( ( !closestSets.first || closestSets.first->get_count() == 0 ) )
            return this->CreateEmptyStream( pEvalContext->GetDefaultChannels() );

        float timeOffsetSeconds = static_cast<float>( timeOffset / static_cast<double>( frameRate ) );
        float timeStepSeconds = static_cast<float>( timeStep / static_cast<double>( frameRate ) );

        if( !closestSets.second ) {
            // If we only have one sample, we need to use velocity extrapolation.
            pResult.reset( new particle_set_istream( closestSets.first ) );
            pResult = frantic::particles::streams::age_culled_particle_istream::apply_to_stream( pResult,
                                                                                                 -timeOffsetSeconds );
            pResult = time_offset_op::apply_to_stream( pResult, timeOffsetSeconds );
        } else {
            // We have two samples so we can do cubic interpolation between them.
            particle_istream_ptr pLeft( new particle_set_istream( closestSets.first ) );
            particle_istream_ptr pRight( new particle_set_istream( closestSets.second ) );

            // Don't bother interpolating if we are +- 1 tick from a sample. We could check for an exact match, but the
            // playback graph often causes rounding errors that make us 1 off.
            if( std::abs( timeOffsetSeconds ) <
                4.1666666e-4f ) { // 2 ticks is 2/4800 seconds, so i look for values less than that.
                pResult = pLeft;
            } else {
                double interpParam = timeOffset / timeStep; // in [0, 1]

                pResult = frantic::particles::streams::apply_interpolation_particle_istream_with_lifespan_culling(
                    pLeft, pRight, timeStepSeconds, interpParam );

                // Cull any particles that shouldn't be born yet.
                // pLeft = frantic::particles::streams::age_culled_particle_istream::apply_to_stream( pLeft,
                // -timeOffsetSeconds );

                // pResult.reset( new frantic::particles::streams::time_interpolation_particle_istream( pLeft, pRight,
                // timeStepSeconds, timeOffsetSeconds / timeStepSeconds ) );
            }
        }
    } else {
        double timeOffset = 0;

        particle_set_interface_ptr pSet = this->GetNearestSample( playbackTime, timeOffset );
        if( !pSet || pSet->get_count() == 0 )
            return this->CreateEmptyStream( pEvalContext->GetDefaultChannels() );

        float timeOffsetSeconds = static_cast<float>( timeOffset / static_cast<double>( frameRate ) );

        pResult.reset( new particle_set_istream( pSet ) );
        pResult =
            frantic::particles::streams::age_culled_particle_istream::apply_to_stream( pResult, -timeOffsetSeconds );
        pResult = time_offset_op::apply_to_stream( pResult, timeOffsetSeconds );
    }

    pResult->set_channel_map( pEvalContext->GetDefaultChannels() );

    if( !m_inRenderMode ) {
        float viewFraction = m_pblock->GetFloat( kViewportPercentage );
        float viewLimit = m_pblock->GetFloat( kViewportLimit );
        bool useViewLimit = m_pblock->GetInt( kUseViewportLimit ) != FALSE;

        boost::int64_t actualLimit = std::numeric_limits<boost::int64_t>::max();
        if( useViewLimit )
            actualLimit =
                std::max( static_cast<boost::int64_t>( std::ceil( static_cast<double>( viewLimit ) * 1000.0 ) ), 1i64 );

        pResult = frantic::particles::streams::apply_fractional_by_id_particle_istream( pResult, viewFraction, _T("ID"),
                                                                                        actualLimit );
    }

    pResult = velocity_scale_op::apply_to_stream( pResult, static_cast<float>( playbackDerivative ) );

    if( !m_inRenderMode ) {
        // Adjust the display channels according to the user's settings.
        // NB: Do this after adjusting the position & velocity so that is reflected in the viewport.
        frantic::tstring viewportVectorChannel = m_pblock->GetStr( kViewportVectorChannel );
        frantic::tstring viewportColorChannel = m_pblock->GetStr( kViewportColorChannel );

        float viewportVectorScale = std::max( 0.f, m_pblock->GetFloat( kViewportVectorScale ) );
        bool viewportVectorNormalize = m_pblock->GetInt( kViewportVectorNormalize ) != FALSE;

        if( pEvalContext->GetDefaultChannels().has_channel( _T("PRTViewportVector") ) ) {
            if( !viewportVectorChannel.empty() &&
                pResult->get_native_channel_map().has_channel( viewportVectorChannel ) )
                pResult = viewport_vector_op::apply_to_stream( pResult, viewportVectorChannel, viewportVectorScale,
                                                               viewportVectorNormalize );
        }

        if( pEvalContext->GetDefaultChannels().has_channel( _T("PRTViewportColor") ) )
            pResult = viewport_color_op::apply_to_stream(
                pResult, viewportColorChannel, frantic::graphics::color3f::from_RGBA( pNode->GetWireColor() ) );
    }

    return pResult;
}

particle_set_interface_ptr StokeBase::GetNearestSample( double frame, double& outTimeOffset ) {
    double nearestTime = m_particleCache.find_nearest_key( frame );

    if( !frantic::math::is_finite( nearestTime ) )
        return particle_set_interface_ptr();

    outTimeOffset = frame - nearestTime;

    return m_particleCache.find( nearestTime );
}

std::pair<particle_set_interface_ptr, particle_set_interface_ptr>
StokeBase::GetBracketingSamples( double frame, double& outTimeOffset, double& outTimeStep ) {
    std::pair<double, double> bracketTimes = m_particleCache.find_bracketing_keys( frame );

    if( !frantic::math::is_finite( bracketTimes.first ) ) { // We are treating just the left and both infinite the same
                                                            // since we don't velocity offset before the first frame.
        outTimeStep = std::numeric_limits<double>::infinity();
        outTimeOffset = 0.f;

        return std::make_pair( particle_set_interface_ptr(), particle_set_interface_ptr() );
    } else if( !frantic::math::is_finite( bracketTimes.second ) ||
               bracketTimes.first == frame ) { // If we queried exactly at the sample time, just return one sample.
        outTimeStep = std::numeric_limits<double>::infinity();
        outTimeOffset = frame - bracketTimes.first;

        return std::make_pair( m_particleCache.find( bracketTimes.first ), particle_set_interface_ptr() );
    } else {
        double leftOffset = frame - bracketTimes.first;
        double rightOffset = bracketTimes.second - frame;

        std::pair<particle_set_interface_ptr, particle_set_interface_ptr> result;

        if( leftOffset <= rightOffset ) {
            outTimeStep = bracketTimes.second - bracketTimes.first;
            outTimeOffset = leftOffset;
            result.first = m_particleCache.find( bracketTimes.first );
            result.second = m_particleCache.find( bracketTimes.second );
        } else {
            outTimeStep = bracketTimes.first - bracketTimes.second;
            outTimeOffset = -rightOffset;
            result.first = m_particleCache.find( bracketTimes.second );
            result.second = m_particleCache.find( bracketTimes.first );
        }

        return result;
    }
}

particle_istream_ptr StokeBase::CreateEmptyStream( const frantic::channels::channel_map& outMap ) const {
    particle_set_interface_ptr pParticleData;

    // Find the first non-null entry in memory.
    for( cache_type::const_memory_iterator it = m_particleCache.memory_begin(), itEnd = m_particleCache.memory_end();
         it != itEnd && !pParticleData; ++it )
        pParticleData = it.value();

    frantic::channels::channel_map nativeMap;
    nativeMap.define_channel<frantic::graphics::vector3f>( _T("Position") );
    nativeMap.define_channel<frantic::graphics::vector3f>( _T("Velocity") );
    nativeMap.define_channel<float>( _T("Age") );
    nativeMap.define_channel<float>( _T("LifeSpan") );
    nativeMap.define_channel<boost::int64_t>( _T("ID") );

    // Using the first non-null cache entry, determine which extra channels are in the cache.
    // TODO: This would be more reliable if we enforced equivalent channels for each particle_set we add to the cache.
    if( pParticleData ) {
        for( std::size_t i = 0, iEnd = pParticleData->get_num_channels(); i < iEnd; ++i ) {
            frantic::tstring chName;
            std::pair<frantic::channels::data_type_t, std::size_t> chType;

            pParticleData->get_particle_channel( i, &chName, &chType );

            nativeMap.define_channel( chName, chType.second, chType.first );
        }
    }

    nativeMap.end_channel_definition();

    if( !nativeMap.has_channel( _T("NormalizedAge") ) )
        nativeMap.append_channel<float>( _T("NormalizedAge") );

    return particle_istream_ptr( new frantic::particles::streams::empty_particle_istream( outMap, nativeMap ) );
}

frantic::particles::particle_istream_ptr StokeBase::create_particle_stream(
    INode* pNode, Interval& outValidity,
    boost::shared_ptr<frantic::max3d::particles::IMaxKrakatoaPRTEvalContext> pEvalContext ) {
    return this->CreateStream( pNode, outValidity, pEvalContext );
}

} // namespace max3d
} // namespace stoke

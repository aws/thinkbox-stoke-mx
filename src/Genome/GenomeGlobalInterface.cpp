// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "StokeMXVersion.h"
#include <stoke\max3d\Genome\Genome.hpp>

#include <frantic/magma/magma_exception.hpp>
#include <frantic/magma/max3d/DebugInformation.hpp>

#include <frantic/max3d/ifnpub_wrap.hpp>

#include <frantic/files/paths.hpp>
#include <frantic/logging/logging_level.hpp>
#include <frantic/win32/log_window.hpp>
#include <frantic/win32/wrap.hpp>

#include <boost/filesystem.hpp>
#include <boost/scoped_array.hpp>

#pragma warning( push, 3 )
#pragma warning( disable : 4512 )
#include <boost/scope_exit.hpp>
#pragma warning( pop )

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#pragma warning( push, 3 )
#include <modstack.h>
#include <notify.h>
#include <object.h>
#pragma warning( pop )

// Filled by DllMain()
extern HINSTANCE hInstance;

#define GenomeGlobal_INTERFACE Interface_ID( 0x28517e8e, 0x27c426cc )

/**
 * This singleton instance exposes a global interface for querying Genome related things.
 */
class IGenomeGlobal : public FPStaticInterface {
    static IGenomeGlobal s_instance;

    IGenomeGlobal();

  public:
    static IGenomeGlobal& get_instance() { return s_instance; }

    /**
     * Genome is expected to be contained in a directory structure such as 'C:\Program Files
     * (x86)\Thinkbox\Genome\3dsMax2012\x64\Genome.dlm'. The 'HOME' directory is the root directory that contains all of
     * Genome's binaries and assets. In the example it is 'C:\Program Files (x86)\Thinkbox\Genome\'.
     * @return The path to the 'HOME' directory.
     */
    MSTR get_genome_home() const;

    MSTR GetVersion();

    /**
     * This will evaluate a Genome modifier on a specific object, returning debugging information
     * @param node The node to evaluate
     * @param modIndex The index of the modifier we want to evaluate
     * @return An instance of class DebugInformation.
     */
    IObject* evaluate_genome_debug( INode* node, int modIndex, TimeValue t = TIME_NegInfinity ) const;

    /**
     * Our logging systems supports 5 differernt logging levels. See <frantic/logging/logging_level.hpp>
     * @param loggingLevel The new logging level to use.
     */
    void set_logging_level( int loggingLevel ) const;

    /**
     * @return The current logging level.
     */
    int get_logging_level() const;

    void set_logging_popup_level( int loggingLevel );

    int get_logging_popup_level() const;

    void show_log_window( bool visible = true ) const;

    void log_message( frantic::logging::logging_level type, const TCHAR* msg ) const;

    void initialize_logging();

    /**
     * We can set a limit to the maximum number of debugger iterations in order to prevent memory overload.
     * @param maxIterations The maximum number of iterations that the debugger will record.
     */
    void set_max_debug_iterations( int maxIterations ) const;

    /**
     * @return The maximum number of iterations for the debugger to process.
     */
    int get_max_debug_iterations() const;

  public:
    enum enumFnIDs {
        kFnGetGenomeHome,
        // Tombstones for old licensing functions.
        kFnRemoved1,
        kFnRemoved2,
        kFnEvaluateGenomeDebug,
        kFnGetLoggingLevel,
        kFnSetLoggingLevel,
        kFnSetMaxDebuggerIterations,
        kFnGetMaxDebuggerIterations,
        kFnGetLoggingPopupLevel,
        kFnSetLoggingPopupLevel,
        kFnShowLogWindow
    };

    enum enumIDs { kLoggingEnum };

#pragma warning( push, 3 )
#pragma warning( disable : 4238 )
    BEGIN_FUNCTION_MAP
    RO_PROP_FN( kFnGetGenomeHome, get_genome_home, TYPE_TSTR_BV )
    PROP_FNS( kFnGetLoggingLevel, get_logging_level, kFnSetLoggingLevel, set_logging_level, TYPE_ENUM )
    PROP_FNS( kFnGetMaxDebuggerIterations, get_max_debug_iterations, kFnSetMaxDebuggerIterations,
              set_max_debug_iterations, TYPE_INT )
    PROP_FNS( kFnGetLoggingPopupLevel, get_logging_popup_level, kFnSetLoggingPopupLevel, set_logging_popup_level,
              TYPE_ENUM )
    FNT_2( kFnEvaluateGenomeDebug, TYPE_IOBJECT, evaluate_genome_debug, TYPE_INODE, TYPE_INDEX )
    VFN_1( kFnShowLogWindow, show_log_window, TYPE_bool )
    END_FUNCTION_MAP
#pragma warning( pop )

  private:
    mutable frantic::win32::log_window m_logWindow;
    frantic::logging::logging_level
        m_popupLogLevel; // Messages of this sort (and higher) will force the log window to be displayed.
};

IGenomeGlobal IGenomeGlobal::s_instance;

IGenomeGlobal::IGenomeGlobal()
    : m_logWindow( _T("Stoke Log Window"), hInstance, NULL, true )
    , m_popupLogLevel( frantic::logging::LOG_WARNINGS ) {
    LoadDescriptor( GenomeGlobal_INTERFACE, _T("GenomeGlobalInterface"), 0, NULL, FP_CORE, PARAMTYPE_END );

    this->AppendEnum( kLoggingEnum, 6, _M( "none" ), frantic::logging::level::none, _M( "error" ),
                      frantic::logging::level::error, _M( "warning" ), frantic::logging::level::warning,
                      _M( "progress" ), frantic::logging::level::progress, _M( "stats" ),
                      frantic::logging::level::stats, _M( "debug" ), frantic::logging::level::debug, PARAMTYPE_END );

    this->AppendProperty( kFnGetGenomeHome, FP_NO_FUNCTION, _T("GenomeHome"), 0, TYPE_TSTR_BV, PARAMTYPE_END );
    this->AppendProperty( kFnGetLoggingLevel, kFnSetLoggingLevel, _T("LoggingLevel"), 0, TYPE_ENUM, kLoggingEnum,
                          PARAMTYPE_END );
    this->AppendProperty( kFnGetLoggingPopupLevel, kFnSetLoggingPopupLevel, _T("LoggingPopupLevel"), 0, TYPE_ENUM,
                          kLoggingEnum, PARAMTYPE_END );
    this->AppendProperty( kFnGetMaxDebuggerIterations, kFnSetMaxDebuggerIterations, _T("MaxDebuggerIterations"), 0,
                          TYPE_INT, PARAMTYPE_END );

    this->AppendFunction( kFnEvaluateGenomeDebug, _M( "EvaluateDebug" ), 0, TYPE_IOBJECT, 0, 2, _M( "Node" ), 0,
                          TYPE_INODE, _M( "ModIndex" ), 0, TYPE_INDEX, PARAMTYPE_END );

    this->AppendFunction( kFnShowLogWindow, _M( "ShowLogWindow" ), 0, TYPE_VOID, 0, 1, _M( "Visible" ), 0, TYPE_bool,
                          PARAMTYPE_END );
}

MSTR IGenomeGlobal::get_genome_home() const {
    boost::filesystem::path modulePath( frantic::win32::GetModuleFileName( hInstance ) );

    // Module path is expected to be like 'C:\Program Files (x86)\Thinkbox\Genome\3dsMax2012\x64\Genome.dlm'
    // The 'home' location is 'C:\Program Files (x86)\Thinkbox\Genome\'

    // std::wstring result = modulePath.parent_path().parent_path().parent_path().make_preferred().wstring();
    std::wstring result = modulePath.parent_path().parent_path().make_preferred().wstring();

    result = frantic::files::ensure_trailing_pathseparator( result );

    return MSTR( result.c_str() );
}

MSTR IGenomeGlobal::GetVersion() { return _M( FRANTIC_VERSION ); }

IObject* IGenomeGlobal::evaluate_genome_debug( INode* node, int modIndex, TimeValue t ) const {
    using frantic::magma::max3d::DebugInformation;

    std::unique_ptr<DebugInformation> result;

    try {
        int derIndex = -1;
        Modifier* theMod = NULL;
        IDerivedObject* derObj = GetCOREInterface7()->FindModifier( *node, modIndex, derIndex, theMod );

        if( derObj ) {
            IGenome* genomeMod = GetGenomeInterface( theMod );
            if( genomeMod ) {
                // The index within the IDerivedObject increases as you go "down" the stack, so we want the current
                // index plus one, or the next derived object.
                ObjectState os = ( derIndex + 1 < derObj->NumModifiers() ) ? derObj->Eval( t, derIndex + 1 )
                                                                           : derObj->GetObjRef()->Eval( t );

                if( Object* obj = os.obj ) {
#pragma warning( push )
#pragma warning( disable : 4512 )
                    BOOST_SCOPE_EXIT( ( &os )( obj ) ) {
                        if( obj != os.obj && !obj->IsObjectLocked() )
                            obj->MaybeAutoDelete();
                        os.DeleteObj( TRUE );
                    }
                    BOOST_SCOPE_EXIT_END
#pragma warning( pop )

                    result.reset( new DebugInformation );

                    genomeMod->EvaluateDebug( t, *derObj->GetModContext( derIndex ), &os, node, result->get_storage() );
                }
            }
        }
    } catch( const frantic::magma::magma_exception& e ) {
        FF_LOG( error ) << e.get_message( true ) << std::endl;
        result.reset();
    } catch( const std::exception& e ) {
        FF_LOG( error ) << e.what() << std::endl;
        result.reset();
    } catch( ... ) {
        FF_LOG( error ) << "IGenomeGlobal::evaluate_genome_debug() Unknown error" << std::endl;
        result.reset();
    }

    return result.release();
}

void IGenomeGlobal::set_logging_level( int loggingLevel ) const { frantic::logging::set_logging_level( loggingLevel ); }

int IGenomeGlobal::get_logging_level() const { return frantic::logging::get_logging_level(); }

void IGenomeGlobal::set_logging_popup_level( int logLevel ) {
    m_popupLogLevel = static_cast<frantic::logging::logging_level>( logLevel );
}

int IGenomeGlobal::get_logging_popup_level() const { return m_popupLogLevel; }

namespace {
const TCHAR* g_logPrefix[] = { _T(""), _T("DBG: "), _T("STS: "), _T("PRG: "), _T("WRN: "), _T("ERR: ") };

class mprintf_functor {
    frantic::logging::logging_level m_level;

  public:
    mprintf_functor( frantic::logging::logging_level level )
        : m_level( level ) {}

    void operator()( const MCHAR* p ) {
        if( MAXScript_running )
            mprintf( _M( "%s%s\n" ), g_logPrefix[m_level], p );
    }
};

struct to_genome_log {
    frantic::logging::logging_level m_level;

  public:
    to_genome_log( frantic::logging::logging_level level )
        : m_level( level ) {}

    void operator()( const MCHAR* szMsg ) { IGenomeGlobal::get_instance().log_message( m_level, szMsg ); }
};

void DoInitializeLogWindow( void* pLogWindow, NotifyInfo* ) {
    if( pLogWindow != NULL ) {
        static_cast<frantic::win32::log_window*>( pLogWindow )->init( hInstance, GetCOREInterface()->GetMAXHWnd() );

        // Switch the log streams to the window, and delete the old buffer.
        delete frantic::logging::debug.rdbuf(
            frantic::logging::new_ffstreambuf( to_genome_log( frantic::logging::LOG_DEBUG ) ) );
        delete frantic::logging::stats.rdbuf(
            frantic::logging::new_ffstreambuf( to_genome_log( frantic::logging::LOG_STATS ) ) );
        delete frantic::logging::progress.rdbuf(
            frantic::logging::new_ffstreambuf( to_genome_log( frantic::logging::LOG_PROGRESS ) ) );
        delete frantic::logging::warning.rdbuf(
            frantic::logging::new_ffstreambuf( to_genome_log( frantic::logging::LOG_WARNINGS ) ) );
        delete frantic::logging::error.rdbuf(
            frantic::logging::new_ffstreambuf( to_genome_log( frantic::logging::LOG_ERRORS ) ) );
    }
}
} // namespace

void ShowLogWindow( bool visible ) { IGenomeGlobal::get_instance().show_log_window( visible ); }

void IGenomeGlobal::show_log_window( bool visible ) const { m_logWindow.show( visible ); }

void IGenomeGlobal::log_message( frantic::logging::logging_level type, const TCHAR* msg ) const {
    if( type <= m_popupLogLevel && type != frantic::logging::LOG_NONE && !m_logWindow.is_visible() )
        m_logWindow.show();

    const TCHAR* szPrefix = _T("");
    if( type >= frantic::logging::LOG_NONE && type < frantic::logging::LOG_CUSTOM )
        szPrefix = g_logPrefix[type];

    m_logWindow.log( frantic::tstring() + szPrefix + msg );
}

void IGenomeGlobal::initialize_logging() {
    frantic::logging::redirect_stream(
        frantic::logging::debug, frantic::logging::new_ffstreambuf( mprintf_functor( frantic::logging::LOG_DEBUG ) ) );
    frantic::logging::redirect_stream(
        frantic::logging::stats, frantic::logging::new_ffstreambuf( mprintf_functor( frantic::logging::LOG_STATS ) ) );
    frantic::logging::redirect_stream( frantic::logging::progress, frantic::logging::new_ffstreambuf( mprintf_functor(
                                                                       frantic::logging::LOG_PROGRESS ) ) );
    frantic::logging::redirect_stream( frantic::logging::warning, frantic::logging::new_ffstreambuf( mprintf_functor(
                                                                      frantic::logging::LOG_WARNINGS ) ) );
    frantic::logging::redirect_stream(
        frantic::logging::error, frantic::logging::new_ffstreambuf( mprintf_functor( frantic::logging::LOG_ERRORS ) ) );
    frantic::logging::set_logging_level( frantic::logging::level::warning );

    RegisterNotification( &DoInitializeLogWindow, &m_logWindow, NOTIFY_SYSTEM_STARTUP );
}

namespace {
int g_maxDebuggerIterations = 10000;
}

// This global function provides access to the value stored, without exposing it as a global variable.
int GetMaximumDebuggerIterations() { return g_maxDebuggerIterations; }

void IGenomeGlobal::set_max_debug_iterations( int maxIterations ) const {
    g_maxDebuggerIterations = std::max( maxIterations, 0 );
}

int IGenomeGlobal::get_max_debug_iterations() const { return g_maxDebuggerIterations; }

void InitializeLogging() { IGenomeGlobal::get_instance().initialize_logging(); }

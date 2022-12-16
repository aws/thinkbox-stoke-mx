// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <ifnpub.h>
#include <notify.h>

// #include <Graphics\IDisplayManager.h>

#include <frantic/logging/logging_level.hpp>
#include <frantic/max3d/fnpublish/StaticInterface.hpp>

// Old Max versions are not const-correct wrt. Tab<> of C-strings.
#define MCHAR_PTR const MCHAR*

namespace stoke {
namespace max3d {

class IReflowField;

// Fwd decl.
FPInterface* CreateReflowField( INode* pNode );
FPInterface* CreateDummyReflowField();

FPInterface* CreateParticleVelocityField( INode* pNode, float spacing, int ( *pBounds )[6], int boundsPadding,
                                          bool removeDivergence );

FPInterface* CreateAdditiveVelocityFieldMXS( const Tab<FPInterface*>* pFields );

FPInterface* CreateKrakatoaGenerator( INode* pNode, float jitterRadius, bool ignoreIDs );

#if defined( FUMEFX_SDK_AVAILABLE )
FPInterface* CreateFumeFXGenerator( INode* pNode );
#endif

FPInterface* CreateRK2Advector();
FPInterface* CreateRK3Advector();
FPInterface* CreateRK4Advector();

FPInterface* CreateIDAllocator();

FPInterface* CreateParticleSet( const Tab<MCHAR_PTR>* extraChannels );

void AdvectParticleSetMXS( FPInterface* pParticleSet, FPInterface* pAdvectorInterface,
                           FPInterface* pVelocityFieldInterface, float timeStepSeconds );

void UpdateAgeChannelMXS( FPInterface* pParticleSet, float timeStepSeconds );

void UpdateAdvectionOffsetChannelMXS( FPInterface* pParticleSet, FPInterface* pAdvectorInterface,
                                      FPInterface* pVelocityFieldInterface, float timeStepSeconds );

void ApplyAdvectionOffsetChannelMXS( FPInterface* pParticleSet );

void MagmaEvaluateParticleSet( FPInterface* pParticleSet, ReferenceTarget* pMagmaHolder, TimeValue t,
                               FPInterface* pErrorReporter );

void WriteParticleSetMXS( FPInterface* pParticleSet, const MCHAR* szFilePath );

bool IsParticleSource( INode* pNode, TimeValue t );

bool IsGeometrySource( INode* pNode, TimeValue t );

#if defined( FUMEFX_SDK_AVAILABLE )
bool IsFumeFXSource( INode* pNode, TimeValue t );
#endif

class ReflowGlobalInterface : public frantic::max3d::fnpublish::StaticInterface<ReflowGlobalInterface> {
  public:
    enum { kSourceType, kLoggingLevel, kVelocityType, kViewportType };

    struct LoggingLevel {
        enum option {
            none = frantic::logging::level::none,
            error = frantic::logging::level::error,
            warning = frantic::logging::level::warning,
            progress = frantic::logging::level::progress,
            stats = frantic::logging::level::stats,
            debug = frantic::logging::level::debug
        };
    };

    struct SourceType {
        enum option {
            invalid,
            particles, // These sources use existing particles to generate seed positions.
            geometry,  // These sources use geometry to generate seed positions.
            fumefx     // These sources use FumeFX voxel data to generate seed positions
        };
    };

    struct VelocityType {
        enum option {
            invalid,
            particles, // Splats particle velocities to a grid
            fumefx,    // Reads Velocity channel from FumeFX sim
            phoenix,   // Reads Velocity channel from Phoenix sim
            spacewarp, // Uses force output from a spacewarp
            field      // Reads velocity from a Stoke 2.0 Field object (ex. FieldSim, FieldMagma, FieldLoader, etc.)
        };
    };

    struct ViewportType {
        enum option { legacy, nitrous };
    };

  public:
    ReflowGlobalInterface();

    virtual ~ReflowGlobalInterface();

    /**
     * Stoke is expected to be contained in a directory structure such as 'C:\Program
     * Files\Thinkbox\Stoke\3dsMax2013\StokeMX.dlo'. The 'HOME' directory is the root directory that contains all of
     * Stoke's binaries and assets. In the example it is 'C:\Program Files\Thinkbox\Stoke\'.
     * @return The path to the 'HOME' directory.
     */
    MSTR GetRootDirectory();

    /**
     * @return attributions for third-party software used in Stoke
     */
    MSTR GetAttributions();

    /**
     * Retrieve the version information of Stoke encoded as a string of the form Major.Minor.Pach.SVNRevision
     */
    MSTR GetVersion();

    /**
     * Our logging systems supports 5 differernt logging levels. See <frantic/logging/logging_level.hpp>
     * @param loggingLevel The new logging level to use.
     */
    void SetLoggingLevel( LoggingLevel::option loggingLevel );

    /**
     * @return The current logging level.
     */
    LoggingLevel::option GetLoggingLevel();

    /**
     * @param szAdvectorName The name of the type of advector to make
     * @return A new IAdvector instance, of the type specified.
     */
    FPInterface* CreateAdvector( const MCHAR* szAdvectorType );

    /**
     * Creates a new instance of IDAllocator that is responsible for assigning IDs to new particle created by
     * IParticleGenerator objects.
     * @return The newe object.
     */
    FPInterface* CreateIDAllocator();

    /**
     * @return A new instance of IReflowField created from the passed INode
     */
    FPInterface* CreateReflowField( INode* pFieldNode );
    FPInterface* CreateDummyReflowField();

    FPInterface* CreateParticleField( INode* pParticleNode, float spacing, const Point3& minBounds,
                                      const Point3& maxBounds, int boundsPadding, bool removeDivergence );

    FPInterface* CreateAdditiveVelocityField( const Tab<FPInterface*>& pFields );

    /**
     * Creates a new ParticleGenerator object from a source that contains particle data.
     * @param pNode The node that produces particles.
     * @param jitterRadius A random offset to apply to all particles at creation time.
     * @param ignoreIDChannel If true, all particles will be eligible seeds. If false, only "new" particles on the
     * current frame are eligible seeds.
     * @return A new instance of IParticleGenerator that generates particles from seed locations specified by a PRT
     * object.
     */
    FPInterface* CreateParticleGenerator( INode* pNode, float jitterRadius = 0.f, bool ignoreIDChannel = false );

    /**
     * Creates a new ParticleGenerator object from a source that contains geometry data.
     * @param pNode The node that produces geometry.
     * @return A new instance of IParticleGenerator that generates particles from the surface or volume of a mesh
     * object.
     */
    FPInterface* CreateGeometryGenerator( INode* pPRTNode, const MCHAR* szMode, float volumeSpacing,
                                          const MCHAR* szSelectionType, float vertexJitterRadius );

    /**
     * Creates a new ParticleGenerator object from a FumeFX simulation.
     * @param pNode The node that contains a FumeFX simulation object.
     * @return A new instance of IParticleGenerator that generates particles from the 'source' tagged voxels of the
     * FumeFX sim.
     */
#if defined( FUMEFX_SDK_AVAILABLE )
    FPInterface* CreateFumeFXGenerator( INode* pPRTNode );
#endif

    /**
     * Creates a new empty particle set with the standard channels, plus the channels specified.
     * @param pExtraChannelDexcriptors A list of channel descriptions. In the format of "Name Type[Arity]", ex.
     * "LifeSpan float32[1]".
     * @return The new particle set.
     */
    FPInterface* CreateParticleSet( const Tab<MCHAR_PTR>& pExtraChannelDescriptors );

    /**
     * Advects all particles in a particle set, using the specified advection scheme and velocity field.
     * @param pParticleSet The IParticleSet instance to advect.
     * @param pAdvectorInterface The instance of IAdvector that implements the advection scheme.
     * @param pVelocityFieldInterface The instance of IReflowField that implements the velocity field affecting the
     * particles.
     * @param timeStepSeconds The time (in seconds) that the particles are affected by the velocity field.
     */
    void AdvectParticleSetMXS( FPInterface* pParticleSet, FPInterface* pAdvectorInterface,
                               FPInterface* pVelocityFieldInterface, float timeStepSeconds );

    /**
     * Adds timeStepSeconds to the age of every particle in pParticleSet
     * @param pParticleSet the particle set to update the ages of
     * @param timeStepSeconds the amount of time to increment the age by
     */
    void UpdateAgeChannelMXS( FPInterface* pParticleSet, float timeStepSeconds );

    /**
     * Sets the advector of all particles in a particle set, using the specified advection scheme and velocity field.
     * @param pParticleSet The IParticleSet instance to consider.
     * @param pAdvectorInterface The instance of IAdvector that implements the advection scheme.
     * @param pVelocityFieldInterface The instance of IReflowField that implements the velocity field affecting the
     * particles.
     * @param timeStepSeconds The time (in seconds) that the particles are affected by the velocity field.
     */
    void UpdateAdvectionOffsetChannelMXS( FPInterface* pParticleSetInterface, FPInterface* pAdvectorInterface,
                                          FPInterface* pVelocityFieldInterface, float timeStepSeconds );

    /**
     * Advects all particles in a particle set, using the advector of the particle.
     * @param pParticleSet The IParticleSet instance to advect.
     */
    void ApplyAdvectionOffsetChannelMXS( FPInterface* pParticleSetInterface );

    /**
     * Modifies the given particle set based on the given Magma flow.
     * @param pParticleSet The IParticleSet instance to modify.
     * @param pMagmaHolder The Magma flow to implement the modification.
     * @param currentTime The Time (in seconds) that the particles are modified at.
     */
    void MagmaEvaluateParticleSet( FPInterface* pParticleSet, ReferenceTarget* pMagmaHolder,
                                   frantic::max3d::fnpublish::TimeParameter t, FPInterface* pErrorReporter );

    /**
     * Saves a particle set to file.
     * @param pParticleSetObject The IParticleSet instance to save to disk, cast as a FPInterface*.
     * @param szFilePath Path to file to save to disk
     */
    void WriteParticleSet( FPInterface* pParticleSetObject, const MCHAR* szFilePath );

    /**
     * Some 3ds Max objects have different viewport and render settings. This function (and its pair EndRenderMode) will
     * set a collection of nodes and all of their dependencies to be in render mode.
     * @note The user MUST call EndRenderMode with the same node list to reverse the effects of this call. Failure to do
     * so will cause undefined behavior.
     * @param nodes The list of nodes to put into render mode. All of the dependecies of these nodes will also be put
     * into render mode.
     */
    void BeginRenderMode( const Tab<INode*>& nodes, frantic::max3d::TimeWrapper t );

    /**
     * The pair of 'BeginRenderMode', which reverts the changes.
     * @param nodes The list of nodes to exit render mode. This must be the same list as passed to 'BeginRenderMode'.
     */
    void EndRenderMode( const Tab<INode*>& nodes, frantic::max3d::TimeWrapper t );

    /**
     * Returns an enumeration describing the appropriate factory function from ReflowGlobalInterface to use when
     * creating a particle generator from the specified source.
     * @param pNode The node to inspect.
     * @return The type of generator this node makes.
     */
    SourceType::option GetSourceType( INode* pNode, frantic::max3d::TimeWrapper t );

    /**
     * Returns an enumeration describing the appropriate velocity field factory function from ReflowGlobalInterface to
     * use when creating a velocity generator from the specified source. \param pNode The node to inspect. \return The
     * type of generator this node makes.
     */
    VelocityType::option GetVelocityType( INode* pNode, frantic::max3d::TimeWrapper t );

    /**
     * Sets the temporary directory used when writing particle files. A particle file is first written to the temp
     * directory then moved to the target destination. \param szTempDir The path to the temporary directory to use. If
     * NULL, the default directory will be used.
     */
    void SetTempDirectory( const TCHAR* szTempDir );

    /**
     * We can set a limit to the maximum number of debugger iterations in order to prevent memory overload.
     * \param maxIterations The maximum number of iterations that the debugger will record.
     */
    void SetMaxDebugIterations( int maxIterations );

    /**
     * \return The maximum number of iterations for the debugger to process.
     */
    int GetMaxDebugIterations();

    /**
     * We can limit the maximum number of markers displayed in the viewport for any single object (ie. the "reduce"
     * option). \param maxCount The maximum number of markers each Stoke object will display.
     */
    void SetMaxMarkerCount( int maxCount );

    /**
     * \return The current maximum number of markers that can be displayed by a single object
     */
    int GetMaxMarkerCount();

    /**
     * \return The viewport system currently enabled in 3ds Max.
     */
    ViewportType::option GetViewportType();

    /**
     * \return Whether variable sized markers are supported in the current viewport system.
     */
    bool GetPointSizeSupported();

    /**
     * Indicates if the result of evaluating this node produces an Ember field.
     */
    bool IsEmberField( INode* pNode, frantic::max3d::fnpublish::TimeWrapper t );

    FPInterface* CreateMXSField( INode* pNode, int stackIndex, frantic::max3d::fnpublish::TimeWrapper t );

    ReferenceTarget* CreateSimEmberHolder();

#if defined( FUMEFX_SDK_AVAILABLE )
    void WriteFXDFile( const MCHAR* szFilePath, INode* pNode, const Point3& min, const Point3& max, float spacing,
                       int stackIndex, frantic::max3d::fnpublish::TimeWrapper t );
#endif

#if defined( FIELD3D_AVAILABLE )
    void WriteField3DFile( const MCHAR* szFilePath, INode* pNode, const Point3& min, const Point3& max, float spacing,
                           int stackIndex, frantic::max3d::fnpublish::TimeWrapper t );
#endif

    void WriteOpenVDBFile( const MCHAR* szFilePath, INode* pNode, const Point3& min, const Point3& max, float spacing,
                           int stackIndex, frantic::max3d::fnpublish::TimeWrapper t );
};
} // namespace max3d
} // namespace stoke

stoke::max3d::ReflowGlobalInterface* GetStokeInterface();

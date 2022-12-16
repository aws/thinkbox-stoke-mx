// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource.h"

#include <stoke/max3d/EmberPipeObject.hpp>

#include <krakatoa/raytrace_renderer/raytrace_renderer.hpp>
#include <krakatoa/shader_functions.hpp>

#include <frantic/max3d/GenericReferenceTarget.hpp>
#include <frantic/max3d/paramblock_access.hpp>
#include <frantic/max3d/paramblock_builder.hpp>

#include <frantic/graphics/camera.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/volumetrics/field_interface.hpp>

#include <max.h>

#include <tbb/task_scheduler_init.h>

using frantic::graphics::color3f;
using frantic::graphics::vector3f;

#define EmberAtmospheric_NAME "StokeAtmospheric"
#define EmberAtmospheric_CLASSID Class_ID( 0x39e5349e, 0x9eb654e )
#define EmberAtmospheric_VERSION 1

enum {
    kFieldNodes,
    kVoxelLength,
    kUseEmission,
    kUseAbsorption,
    kRaymarchStepMin,
    kRaymarchStepMax,
    kCameraDensityScale,
    kCameraDensityScaleExponent,
    kUseCameraEmissionScale,
    kCameraEmissionScale,
    kCameraEmissionScaleExponent,
    kUseLightDensityScale,
    kLightDensityScale,
    kLightDensityScaleExponent,
};

typedef std::vector<boost::tuple<std::size_t, double, double>> interval_container;

class field_collection {
  public:
    field_collection();

    void add_field( const boost::shared_ptr<frantic::volumetrics::field_interface>& field,
                    const frantic::graphics::boundbox3f& bounds );

    void clear();

    std::size_t get_num_fields() const;

    struct sample {
        float density;
        frantic::graphics::color3f color, extinction, emission;

        sample()
            : density( 0.f ) {}
    };

    void calculate_ray_intervals( const frantic::graphics::ray3f& r, double tMin, double tMax,
                                  interval_container& outIntervals ) const;

    void sample_field( std::size_t fieldIndex, const frantic::graphics::vector3f& pos, sample& outSample ) const;

    void calculate_extinction( const frantic::graphics::ray3f& r, double tMin, double tMax, double stepSize,
                               frantic::graphics::color3f& outExtinction ) const;

    void calculate_extinction( const frantic::graphics::ray3f& r, double tMin, double tMax, double stepSize,
                               float& outExtinction ) const;

  private:
    struct field_data {
        boost::shared_ptr<frantic::volumetrics::field_interface> field;
        frantic::graphics::boundbox3f bounds;

        frantic::channels::channel_cvt_accessor<float> densityAccessor;
        frantic::channels::channel_cvt_accessor<frantic::graphics::color3f> colorAccessor;
        frantic::channels::channel_cvt_accessor<frantic::graphics::color3f> absorptionAccessor;
        frantic::channels::channel_cvt_accessor<frantic::graphics::color3f> emissionAccessor;
    };

    std::vector<field_data> m_fieldData;
    std::size_t m_maxResultSize;
};

field_collection::field_collection()
    : m_maxResultSize( 0 ) {}

void field_collection::add_field( const boost::shared_ptr<frantic::volumetrics::field_interface>& field,
                                  const frantic::graphics::boundbox3f& bounds ) {
    if( !field || bounds.is_empty() )
        return;

    const frantic::channels::channel_map& map = field->get_channel_map();

    field_data newField;
    newField.field = field;
    newField.bounds = bounds;

    newField.densityAccessor.reset( 1.f );
    if( map.has_channel( _T("Density") ) )
        newField.densityAccessor = map.get_cvt_accessor<float>( _T("Density") );

    newField.colorAccessor.reset( frantic::graphics::color3f::white() );
    if( map.has_channel( _T("Color") ) )
        newField.colorAccessor = map.get_cvt_accessor<frantic::graphics::color3f>( _T("Color") );

    newField.absorptionAccessor.reset(); // frantic::graphics::color3f::white() );
    if( map.has_channel( _T("Absorption") ) )
        newField.absorptionAccessor = map.get_cvt_accessor<frantic::graphics::color3f>( _T("Absorption") );

    newField.emissionAccessor.reset( frantic::graphics::color3f::black() );
    if( map.has_channel( _T("Emission") ) )
        newField.emissionAccessor = map.get_cvt_accessor<frantic::graphics::color3f>( _T("Emission") );

    m_fieldData.push_back( newField );
    m_maxResultSize = std::max( m_maxResultSize, map.structure_size() );
}

void field_collection::clear() {
    m_fieldData.clear();
    m_maxResultSize = 0;
}

std::size_t field_collection::get_num_fields() const { return m_fieldData.size(); }

void field_collection::calculate_ray_intervals( const frantic::graphics::ray3f& r, double tMin, double tMax,
                                                interval_container& outIntervals ) const {
    outIntervals.clear();

    // TODO: This is intersecting against all fields. I am assuming we have few fields (ie. < 10) so it doesn't make as
    // much sense to use an acceleration structure. This assumption
    //       may be proved invalid at some point though.

    std::size_t i = 0;
    for( std::vector<field_data>::const_iterator it = m_fieldData.begin(), itEnd = m_fieldData.end(); it != itEnd;
         ++it, ++i ) {
        double t0 = tMin, t1 = tMax;
        if( r.clamp_to_box( it->bounds, t0, t1 ) ) {
            assert( t0 <= t1 );

            // TODO: Some fields might provide tighter bounds on their defined regions. We should incorporate that at
            // some point.

            outIntervals.push_back( boost::make_tuple( i, t0, t1 ) );
        }
    }
}

void field_collection::sample_field( std::size_t fieldIndex, const frantic::graphics::vector3f& pos,
                                     sample& outSample ) const {
    const field_data& fd = m_fieldData[fieldIndex];

    char* buffer = static_cast<char*>( alloca( m_maxResultSize ) );

    if( fd.field->evaluate_field( buffer, pos ) ) {
        outSample.density = fd.densityAccessor.get( buffer );
        outSample.color = fd.colorAccessor.get( buffer );
        outSample.extinction = fd.absorptionAccessor.is_valid() ? fd.absorptionAccessor.get( buffer ) + outSample.color
                                                                : frantic::graphics::color3f::white();
        outSample.emission = fd.emissionAccessor.get( buffer );
    } else {
        outSample.density = 0.f;
        outSample.color = frantic::graphics::color3f::black();
        outSample.extinction = frantic::graphics::color3f::black();
        outSample.emission = frantic::graphics::color3f::black();
    }
}

void field_collection::calculate_extinction( const frantic::graphics::ray3f& r, double tMin, double tMax,
                                             double stepSize, frantic::graphics::color3f& outExtinction ) const {
    const double tStep = stepSize / static_cast<double>( r.direction().get_magnitude() );

    outExtinction = frantic::graphics::color3f::black();

    char* buffer = static_cast<char*>( alloca( m_maxResultSize ) );

    frantic::graphics::color3f prevExtinction;

    for( std::vector<field_data>::const_iterator it = m_fieldData.begin(), itEnd = m_fieldData.end(); it != itEnd;
         ++it ) {
        double t0 = tMin, t1 = tMax;
        if( r.clamp_to_box( it->bounds, t0, t1 ) ) {
            if( it->field->evaluate_field( buffer, r.at( static_cast<float>( t0 ) ) ) ) {
                if( it->absorptionAccessor.is_valid() )
                    prevExtinction = it->densityAccessor( buffer ) *
                                     ( it->colorAccessor( buffer ) + it->absorptionAccessor( buffer ) );
                else
                    prevExtinction = frantic::graphics::color3f( it->densityAccessor( buffer ) );
            } else {
                prevExtinction = frantic::graphics::color3f::black();
            }

            frantic::graphics::color3f nextExtinction;

            double tCur = std::min( t0 + tStep, t1 );
            while( tCur <= t1 ) {
                if( it->field->evaluate_field( buffer, r.at( static_cast<float>( tCur ) ) ) ) {
                    if( it->absorptionAccessor.is_valid() )
                        nextExtinction = it->densityAccessor( buffer ) *
                                         ( it->colorAccessor( buffer ) + it->absorptionAccessor( buffer ) );
                    else
                        nextExtinction = frantic::graphics::color3f( it->densityAccessor( buffer ) );
                } else {
                    nextExtinction = frantic::graphics::color3f::black();

                    // TODO: Could track whether the previous sample was also "empty" in order to skip evaluating the
                    // integral completely.
                }

                outExtinction +=
                    ( prevExtinction +
                      nextExtinction ); // Simple application of trapezoid rule to approximate the integral.

                prevExtinction = nextExtinction;

                tCur += tStep;
            }

            // TODO: Handle the last non-stepSize step appropriately.
        }
    }

    outExtinction *= static_cast<float>( 0.5 * stepSize ); // Factored out from trapezoid rule of inner loop.
}

void field_collection::calculate_extinction( const frantic::graphics::ray3f& r, double tMin, double tMax,
                                             double stepSize, float& outExtinction ) const {
    const double tStep = stepSize / static_cast<double>( r.direction().get_magnitude() );

    outExtinction = 0.f;

    char* buffer = static_cast<char*>( alloca( m_maxResultSize ) );

    float prevExtinction = 0.f;

    for( std::vector<field_data>::const_iterator it = m_fieldData.begin(), itEnd = m_fieldData.end(); it != itEnd;
         ++it ) {
        double t0 = tMin, t1 = tMax;
        if( r.clamp_to_box( it->bounds, t0, t1 ) ) {
            if( it->field->evaluate_field( buffer, r.at( static_cast<float>( t0 ) ) ) ) {
                prevExtinction = it->densityAccessor( buffer );
            } else {
                prevExtinction = 0.f;
            }

            float nextExtinction = 0.f;

            double tCur = std::min( t0 + tStep, t1 );
            while( tCur <= t1 ) {
                if( it->field->evaluate_field( buffer, r.at( static_cast<float>( tCur ) ) ) ) {
                    nextExtinction = it->densityAccessor( buffer );
                } else {
                    nextExtinction = 0.f;

                    // TODO: Could track whether the previous sample was also "empty" in order to skip evaluating the
                    // integral completely.
                }

                outExtinction +=
                    ( prevExtinction +
                      nextExtinction ); // Simple application of trapezoid rule to approximate the integral.

                prevExtinction = nextExtinction;

                tCur += tStep;
            }

            // TODO: Handle the last non-stepSize step appropriately.
        }
    }

    outExtinction *= static_cast<float>( 0.5 * stepSize ); // Factored out from trapezoid rule of inner loop.
}

struct pair_first_less {
    template <class T1, class T2>
    bool operator()( const std::pair<T1, T2>& lhs, const std::pair<T1, T2>& rhs ) const {
        return lhs.first < rhs.first;
    }

    template <class T1, class T2>
    bool operator()( const std::pair<T1, T2>& lhs, T1 rhs ) const {
        return lhs.first < rhs;
    }
};

struct light_data : public LightRayTraversal {
    frantic::graphics::vector3f m_lightPos;
    std::vector<std::pair<float, frantic::graphics::color3f>> m_intervals;
    std::vector<std::pair<float, frantic::graphics::color3f>>::iterator m_intervalIt;

  public:
    frantic::graphics::ray3f get_ray_to_light( const frantic::graphics::vector3f& worldOrigin ) {
        return frantic::graphics::ray3f( worldOrigin,
                                         m_lightPos - worldOrigin ); // TODO: Handle direct lights properly.
    }

    // TODO: We could make this piecewise linear instead of constant.
    frantic::graphics::color3f light_at( float t ) {
        // TODO: Come up with a cache system that lets us avoid searching the interval list each time. Perhaps we could
        // require the start & end of the query interval instead
        m_intervalIt = std::lower_bound( m_intervals.begin(), m_intervals.end(), t, pair_first_less() );

        // TODO: Should we clamp to the end, or return black?
        if( m_intervalIt == m_intervals.end() )
            return frantic::graphics::color3f();

        return m_intervalIt->second;
    }

    virtual BOOL Step( float t0, float t1, Color illum, float distAtten ) {
        // assert( m_intervals.empty() || t0 >= m_intervals.back().first );

        // If this is the first interval, or we have a gap in reported intervals, insert a black interval. We assume
        // Step is called with non-overlapping, monotonically increasing [t0, t1).
        if( m_intervals.empty() || t0 - m_intervals.back().first > 1e-4f ) {
            m_intervals.push_back( std::make_pair( t0, frantic::graphics::color3f::black() ) );
        } else if( m_intervals.back().second == frantic::max3d::from_max_t( illum ) ) {
            // If the interval is just being extended with the same value only update the interval length, do not store
            // another interval.
            m_intervals.back().first = t1;
            return TRUE;
        }

        // Intervals are recorded by their end position, and are piece-wise constant.
        m_intervals.push_back( std::make_pair( t1, frantic::max3d::from_max_t( illum ) ) );

        return TRUE;
    }
};

ClassDesc2* GetEmberAtmosphericDesc();

// Implementation in MaxKrakatoaUtils.cpp
extern void SetupLightsRecursive( std::vector<boost::shared_ptr<frantic::rendering::lights::lightinterface>>& outLights,
                                  INode* pNode, std::set<INode*>& doneNodes, TimeValue t,
                                  frantic::logging::progress_logger& progress, float motionInterval, float motionBias,
                                  bool saveAtten );

class EmberAtmospheric : public frantic::max3d::GenericReferenceTarget<Atmospheric, EmberAtmospheric> {
  public:
    EmberAtmospheric();

    // From ReferenceMaker
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

// From SpecialEffect
#if MAX_VERSION_MAJOR < 24
    virtual MSTR GetName();
#else
    virtual MSTR GetName( bool localized ) const;
#endif

    virtual void Update( TimeValue t, Interval& valid );

    // From Atmospheric
    virtual AtmosParamDlg* CreateParamDialog( IRendParams* ip );
    virtual void Shade( ShadeContext& sc, const Point3& p0, const Point3& p1, Color& color, Color& trans,
                        BOOL isBG = FALSE );

  protected:
    virtual ClassDesc2* GetClassDesc() { return GetEmberAtmosphericDesc(); }

  private:
    // Calculates the lighting incident on a location.
    bool calculate_lighting_at( const vector3f& worldPos, ShadeContext& sc, const field_collection::sample& posSample,
                                frantic::graphics::color3f& outLight ) const;
    bool calculate_lighting_at2( const vector3f& worldPos, const field_collection::sample& posSample,
                                 frantic::graphics::color3f& outLight, std::vector<light_data>& lights,
                                 double t ) const;

    // Calculate the lighting reflected towards the eye (and opacity) along a ray.
    frantic::graphics::color6f raymarch( const frantic::graphics::ray3f& r, double t0, double t1, ShadeContext& sc );

    // Given endpoint samples of a ray segment, recursively subdivides the segment unti the integral calculation
    // converges. This handles the non-linear effects of the lights and shadows in the scene.
    void subdivide_recursive( const frantic::graphics::ray3f& r, double tLeft, double tRight,
                              interval_container::const_iterator intervalsStart,
                              interval_container::const_iterator intervalsEnd,
                              // ShadeContext& sc,
                              std::vector<light_data>& lightData, frantic::graphics::color6f& fullVal,
                              const field_collection::sample& prevSample, const field_collection::sample& nextSample,
                              const frantic::graphics::color3f& prevLight,
                              const frantic::graphics::color3f& nextLight );

    // Calculate the opacity along a ray.
    frantic::graphics::alpha3f raymarch_opacity( const frantic::graphics::ray3f& r, double t0, double t1 );

  private:
    // Variables set via. Update
    field_collection m_fields;

    std::vector<ObjLightDesc*> m_lights;

    Interval m_validInterval;

    float m_maxStep, m_minStep;
    float m_cameraDensityScale, m_cameraEmissionScale, m_lightDensityScale;

    bool m_doEmission, m_doAbsorption;
};

class EmberAtmosphericDesc : public ClassDesc2 {
  public:
    frantic::max3d::ParamBlockBuilder m_pbDesc;

  public:
    EmberAtmosphericDesc();

    int IsPublic() { return TRUE; }
    void* Create( BOOL /*loading*/ ) { return new EmberAtmospheric; }
    const TCHAR* ClassName() { return _T( EmberAtmospheric_NAME ); }
    SClass_ID SuperClassID() { return ATMOSPHERIC_CLASS_ID; }
    Class_ID ClassID() { return EmberAtmospheric_CLASSID; }
    const TCHAR* Category() { return _T("Stoke"); }

    const TCHAR* InternalName() {
        return _T( EmberAtmospheric_NAME );
    }                                           // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; } // returns owning module handle
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() { return _T( EmberAtmospheric_NAME ); }
#endif
};

ParamMap2UserDlgProc* GetEmberAtmosphericDlgProc();

EmberAtmosphericDesc::EmberAtmosphericDesc()
    : m_pbDesc( 0, _T("Parameters"), 0, EmberAtmospheric_VERSION ) {
    m_pbDesc.OwnerClassDesc( this );
    m_pbDesc.OwnerRefNum( 0 );

    m_pbDesc.RolloutTemplate( 0, IDD_ATMOSPHERIC, IDS_ATMSOPHERIC, GetEmberAtmosphericDlgProc() );

    m_pbDesc.Parameter<INode*[]>( kFieldNodes, _T("FieldNodes"), 0 );

    m_pbDesc.Parameter<bool>( kUseEmission, _T("UseEmission"), 0 ).DefaultValue( false );

    m_pbDesc.Parameter<bool>( kUseAbsorption, _T("UseAbsorption"), 0 ).DefaultValue( false );

    m_pbDesc.Parameter<float>( kRaymarchStepMin, _T("RaymarchStepMin"), 0 ).DefaultValue( .5f ).Range( 1e-5f, FLT_MAX );

    m_pbDesc.Parameter<float>( kRaymarchStepMax, _T("RaymarchStepMax"), 0 ).DefaultValue( 2.f ).Range( 1e-5f, FLT_MAX );

    m_pbDesc.Parameter<float>( kCameraDensityScale, _T("CameraDensityScale"), 0 )
        .DefaultValue( 5.f )
        .Range( 1e-5f, FLT_MAX );

    m_pbDesc.Parameter<int>( kCameraDensityScaleExponent, _T("CameraDensityScaleExponent"), 0 )
        .DefaultValue( -1 )
        .Range( INT_MIN, INT_MAX );

    m_pbDesc.Parameter<bool>( kUseCameraEmissionScale, _T("UseCameraEmissionScale"), 0 ).DefaultValue( false );

    m_pbDesc.Parameter<float>( kCameraEmissionScale, _T("CameraEmissionScale"), 0 )
        .DefaultValue( 5.f )
        .Range( 1e-5f, FLT_MAX );

    m_pbDesc.Parameter<int>( kCameraEmissionScaleExponent, _T("CameraEmissionScaleExponent"), 0 )
        .DefaultValue( -1 )
        .Range( INT_MIN, INT_MAX );

    m_pbDesc.Parameter<bool>( kUseLightDensityScale, _T("UseLightDensityScale"), 0 ).DefaultValue( false );

    m_pbDesc.Parameter<float>( kLightDensityScale, _T("LightDensityScale"), 0 )
        .DefaultValue( 5.f )
        .Range( 1e-5f, FLT_MAX );

    m_pbDesc.Parameter<int>( kLightDensityScaleExponent, _T("LightDensityScaleExponent"), 0 )
        .DefaultValue( -1 )
        .Range( INT_MIN, INT_MAX );
}

ClassDesc2* GetEmberAtmosphericDesc() {
    static EmberAtmosphericDesc theEmberAtmosphericDesc;
    return &theEmberAtmosphericDesc;
}

class EmberAtmosphericDlgProc : public ParamMap2UserDlgProc {
  public:
    EmberAtmosphericDlgProc();
    virtual ~EmberAtmosphericDlgProc();
    virtual INT_PTR DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
    virtual void DeleteThis() { /*delete this;*/
    }
};

EmberAtmosphericDlgProc::EmberAtmosphericDlgProc() {}

EmberAtmosphericDlgProc::~EmberAtmosphericDlgProc() {}

INT_PTR EmberAtmosphericDlgProc::DlgProc( TimeValue /*t*/, IParamMap2* map, HWND /*hWnd*/, UINT msg, WPARAM wParam,
                                          LPARAM /*lParam*/ ) {
    IParamBlock2* pblock = map->GetParamBlock();
    if( !pblock )
        return FALSE;

    switch( msg ) {
    case WM_COMMAND:
        if( HIWORD( wParam ) != BN_CLICKED )
            break;

        if( LOWORD( wParam ) == IDC_ATMOSPHERIC_OPEN_BUTTON ) {
            static const MCHAR* script =
                _T("try(")
                _T("global StokeAtmosphericEffect_CurrentEffect = arg\n")
                _T("fileIn (StokeGlobalInterface.HomeDirectory + \"scripts\\StokeAtmosphericUI.ms\")\n")
                _T(")catch()");

            frantic::max3d::mxs::expression( script )
                .bind( _T("arg"), static_cast<ReferenceTarget*>( pblock->GetOwner() ) )
                .evaluate<void>();

            return TRUE;
        }
        break;
    }

    return FALSE;
}

ParamMap2UserDlgProc* GetEmberAtmosphericDlgProc() {
    static EmberAtmosphericDlgProc theEmberAtmosphericDlgProc;
    return &theEmberAtmosphericDlgProc;
}

EmberAtmospheric::EmberAtmospheric() { GetEmberAtmosphericDesc()->MakeAutoParamBlocks( this ); }

RefResult EmberAtmospheric::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle /*hTarget*/,
                                              PartID& /*partID*/, RefMessage /*message*/, BOOL /*propagate*/ ) {
    return REF_SUCCEED;
}

#if MAX_VERSION_MAJOR < 24
MSTR EmberAtmospheric::GetName() { return _T("Stoke Atmospheric"); }
#else
MSTR EmberAtmospheric::GetName( bool localized ) const { return _T("Stoke Atmospheric"); }
#endif

void collect_shadow_casters( INode* curNode, TimeValue t,
                             /*std::set<INode*>& visitedNodes, */ std::set<INode*>& shadowCasters ) {
    for( int i = 0, iEnd = curNode->NumberOfChildren(); i < iEnd; ++i ) {
        INode* child = curNode->GetChildNode( i );

        // BOOL isHidden = child->IsNodeHidden(TRUE);
        // BOOL isHidden2 = child->IsHidden(0, TRUE);
        // BOOL isHidden3 = child->IsHidden();
        // BOOL castsShadows = child->CastShadows();
        // BOOL isREnderable = child->Renderable();

        if( child && !child->IsNodeHidden( TRUE ) && child->CastShadows() && child->Renderable() ) {
            ObjectState os = child->EvalWorldState( t );

            if( os.obj && os.obj->IsRenderable() && os.obj->SuperClassID() == GEOMOBJECT_CLASS_ID ) {
                SClass_ID sclassID = os.obj->SuperClassID();
                BOOL isRenderableObj = os.obj->IsRenderable();

                shadowCasters.insert( child );

                if( frantic::logging::is_logging_debug() ) {
                    M_STD_STRING name = child->GetName();
                    frantic::logging::debug << _T("KRAtmospheric found shadow caster: ") << name << std::endl;
                }
            }
        }

        collect_shadow_casters( child, t, shadowCasters );
    }
}

namespace {
void GetLightsRecursive( INode* node, TimeValue t, std::vector<ObjLightDesc*>& outLights,
                         std::set<INode*>& visitedNodes ) {
    if( !node || visitedNodes.find( node ) != visitedNodes.end() )
        return;

    visitedNodes.insert( node );

    // Since lights typically don't flow up the stack, I could probably get away with GetObjectRef.
    ObjectState os = node->EvalWorldState( t );
    if( os.obj && os.obj->SuperClassID() == LIGHT_CLASS_ID ) {
        LightObject* pLightObj = static_cast<LightObject*>( os.obj->GetInterface( I_LIGHTOBJ ) );
        if( !pLightObj ) {
            FF_LOG( warning ) << _T("Not a valid light object: ") << node->GetName() << std::endl;
        } else if( RenderData* pRenderData = node->GetRenderData() ) {
            outLights.push_back( static_cast<ObjLightDesc*>( pRenderData ) );
        }
    }

    for( int i = 0, iEnd = node->NumberOfChildren(); i < iEnd; ++i )
        GetLightsRecursive( node->GetChildNode( i ), t, outLights, visitedNodes );
}

void GetLights( TimeValue t, std::vector<ObjLightDesc*>& outLights ) {
    if( INode* sceneRoot = GetCOREInterface()->GetRootNode() ) {
        std::set<INode*> visitedNodes;

        for( int i = 0, iEnd = sceneRoot->NumberOfChildren(); i < iEnd; ++i )
            GetLightsRecursive( sceneRoot, t, outLights, visitedNodes );
    }
}
} // namespace

volatile long counter = 0;

void EmberAtmospheric::Update( TimeValue t, Interval& valid ) {
    m_fields.clear();
    m_lights.clear();

    counter = 0;

    for( int i = 0, iEnd = m_pblock->Count( kFieldNodes ); i < iEnd; ++i ) {
        INode* pNode = m_pblock->GetINode( kFieldNodes, t, i );
        if( !pNode )
            continue;

        ObjectState os = pNode->EvalWorldState( t );

        boost::shared_ptr<ember::max3d::EmberPipeObject> pObj =
            frantic::max3d::safe_convert_to_type<ember::max3d::EmberPipeObject>( os.obj, t, EmberPipeObject_CLASSID );
        if( !pObj )
            continue;

        boost::shared_ptr<frantic::volumetrics::field_interface> pField = pObj->create_field( pNode, t, valid );

        assert( pField );

        frantic::graphics::boundbox3f bounds( frantic::max3d::from_max_t( pObj->GetBounds().pmin ),
                                              frantic::max3d::from_max_t( pObj->GetBounds().pmax ) );
        if( !pObj->GetInWorldSpace() ) {
            frantic::graphics::transform4f tm = frantic::max3d::from_max_t( pNode->GetNodeTM( t ) );

            bounds.minimum() = tm * bounds.minimum();
            bounds.maximum() = tm * bounds.maximum();
        }

        m_fields.add_field( pField, bounds );
    }

    if( m_fields.get_num_fields() == 0 )
        FF_LOG( warning ) << _T("No fields found for Stoke Atmospheric!") << std::endl;

    float cameraDensityScale, cameraEmissionScale, lightDensityScale;
    int cameraDensityExponent, cameraEmissionExponent, lightDensityExponent;
    BOOL useEmissionScale, useLightDensityScale;

    m_pblock->GetValue( kCameraDensityScale, t, cameraDensityScale, valid );
    m_pblock->GetValue( kCameraDensityScaleExponent, t, cameraDensityExponent, valid );

    m_pblock->GetValue( kUseLightDensityScale, t, useLightDensityScale, valid );
    m_pblock->GetValue( kLightDensityScale, t, lightDensityScale, valid );
    m_pblock->GetValue( kLightDensityScaleExponent, t, lightDensityExponent, valid );

    m_pblock->GetValue( kUseCameraEmissionScale, t, useEmissionScale, valid );
    m_pblock->GetValue( kCameraEmissionScale, t, cameraEmissionScale, valid );
    m_pblock->GetValue( kCameraEmissionScaleExponent, t, cameraEmissionExponent, valid );

    m_cameraDensityScale =
        static_cast<float>( std::max( 0., static_cast<double>( cameraDensityScale ) *
                                              std::pow( 10., static_cast<double>( cameraDensityExponent ) ) ) );

    m_lightDensityScale =
        ( useLightDensityScale != FALSE )
            ? static_cast<float>( std::max( 0., static_cast<double>( lightDensityScale ) *
                                                    std::pow( 10., static_cast<double>( lightDensityExponent ) ) ) )
            : this->m_cameraDensityScale;

    m_cameraEmissionScale =
        ( useEmissionScale != FALSE )
            ? static_cast<float>( std::max( 0., static_cast<double>( cameraEmissionScale ) *
                                                    std::pow( 10., static_cast<double>( cameraEmissionExponent ) ) ) )
            : this->m_cameraDensityScale;

    BOOL useEmission, useAbsorption;
    m_pblock->GetValue( kUseEmission, t, useEmission, valid );
    m_pblock->GetValue( kUseAbsorption, t, useAbsorption, valid );

    m_doEmission = ( useEmission != FALSE );
    m_doAbsorption = ( useAbsorption != FALSE );

    m_pblock->GetValue( kRaymarchStepMin, t, m_minStep, valid );
    m_pblock->GetValue( kRaymarchStepMax, t, m_maxStep, valid );

    if( m_minStep > m_maxStep )
        m_maxStep = m_minStep;

    // A non-positive step is a bad idea. I'm choosing a large step so the result looks bad on purpose.
    if( m_minStep <= 0 || m_maxStep <= 0 )
        m_minStep = m_maxStep = 10.f;

    GetLights( t, m_lights );

    m_validInterval = valid;
}

AtmosParamDlg* EmberAtmospheric::CreateParamDialog( IRendParams* ip ) {
    return GetEmberAtmosphericDesc()->CreateParamDialogs( ip, this );
}

namespace {
/**
 * This function does recursive Simpson's rule integration of of a linearly interpolated density function and light
 * function. It recursively divides the interval until it an absolute error tolerance is exceeded. This recurses on two
 * intervals to only require extensive recursion on parts of the interval that require it, since a exp() function tends
 * to bias towards a small region at one of the interval.
 *
 * This solves the equation: integral(0,1)[ (lightA + x (lightB - lightA)) * e^-integral(0,x){extinctionA + y
 * (extinctionB - extinctionA) dy} dx ] by using integral(0,y){extinctionA + y (extinctionB - extinctionA) dy} = y *
 * extinctionA + 0.5 y^2 (extinctionB - extinctionA)
 *
 * @param lightA the amount of light reflected at the start of the interval
 * @param lightB the amount of light reflected at the end of the interval
 * @param extinctionA the coefficient of extinction at the start of the interval
 * @param extinctionB the coefficient of exrinction at the end of the interval
 * @return the total amount of reflected light, and the alpha transparency associated with this interval. These values
 * can be considered pre-multiplied color values for the purposes of alpha-blending.
 */
inline std::pair<float, float> integrate_scattered_light( float lightA, float lightB, float extinctionA,
                                                          float extinctionB ) {
    struct impl {
        float lightA, lightB;
        float extinctionA, extinctionB;

        typedef float value_type;
        typedef boost::call_traits<value_type>::param_type param_type;

        std::pair<value_type, float> apply() {
            float endT = std::exp( -0.5f * ( extinctionA + extinctionB ) );

            value_type middleLight = std::exp( -( 0.5f * extinctionA + 0.125f * ( extinctionB - extinctionA ) ) ) *
                                     0.5f * ( lightA + lightB );
            value_type endLight = endT * lightB;

            value_type sum = lightA + endLight + 4 * middleLight;

            return std::pair<value_type, float>( apply_recursive( 0, 0, 1.f, lightA, middleLight, endLight, sum ) / 6.f,
                                                 1.f - endT );
        }

        value_type apply_recursive( int depth, float a, float b, param_type left, param_type middle, param_type right,
                                    param_type sum ) {
            float tLeft = a + 0.25f * ( b - a );
            float tLeft2_2 = 0.5f * tLeft * tLeft;

            value_type leftLight = std::exp( -( tLeft * extinctionA + tLeft2_2 * ( extinctionB - extinctionA ) ) ) *
                                   ( lightA + tLeft * ( lightB - lightA ) );
            value_type leftSum = left + middle + 4 * leftLight;

            float tRight = a + 0.75f * ( b - a );
            float tRight2_2 = 0.5f * tRight * tRight;

            value_type rightLight = std::exp( -( tRight * extinctionA + tRight2_2 * ( extinctionB - extinctionA ) ) ) *
                                    ( lightA + tRight * ( lightB - lightA ) );
            value_type rightSum = middle + right + 4 * rightLight;

            value_type totalSum = 0.5f * ( leftSum + rightSum );

            if( depth > 10 || fabsf( sum - totalSum ) < 1e-3f )
                return totalSum;

            float result;

            float c = 0.5f * ( a + b );
            result = apply_recursive( depth + 1, a, c, left, leftLight, middle, leftSum );
            result += apply_recursive( depth + 1, c, b, middle, rightLight, right, rightSum );

            return 0.5f * result;
        }
    } theImpl;

    theImpl.lightA = lightA;
    theImpl.lightB = lightB;
    theImpl.extinctionA = extinctionA;
    theImpl.extinctionB = extinctionB;

    return theImpl.apply();
}
} // namespace

class AtmosShadeContext : public ShadeContext {
    ShadeContext* pDelegate;
    Point3 newP;

  public:
    AtmosShadeContext( ShadeContext& sc, const Point3& p )
        : newP( p )
        , pDelegate( &sc ) {
        mode = sc.mode;
        doMaps = sc.doMaps;
        filterMaps = sc.filterMaps;
        shadow = sc.shadow;
        backFace = sc.backFace;
        mtlNum = sc.mtlNum;
        ambientLight = sc.ambientLight;
        nLights = sc.nLights;
        rayLevel = sc.rayLevel;
        xshadeID = sc.xshadeID;
        atmosSkipLight = sc.atmosSkipLight;
        globContext = sc.globContext;
    }

    virtual Class_ID ClassID() { return /*pDelegate->ClassID()*/ Class_ID( 346, 456 ); }
    virtual BOOL InMtlEditor() { return pDelegate->InMtlEditor(); }
    virtual int Antialias() { return pDelegate->Antialias(); }
    virtual int ProjType() { return pDelegate->ProjType(); }
    virtual LightDesc* Light( int n ) { return pDelegate->Light( n ); }
    virtual TimeValue CurTime() { return pDelegate->CurTime(); }
    virtual int NodeID() { return pDelegate->NodeID(); }
    virtual INode* Node() { return pDelegate->Node(); }
    virtual Object* GetEvalObject() { return pDelegate->GetEvalObject(); }
    virtual Point3 BarycentricCoords() { return pDelegate->BarycentricCoords(); }
    virtual int FaceNumber() { return pDelegate->FaceNumber(); }
    virtual Point3 Normal() { return pDelegate->Normal(); }
    virtual void SetNormal( Point3 p ) { pDelegate->SetNormal( p ); }
    virtual Point3 OrigNormal() { return pDelegate->OrigNormal(); }
    virtual Point3 GNormal() { return pDelegate->GNormal(); }
    virtual float Curve() { return pDelegate->Curve(); }
    virtual Point3 V() { return pDelegate->V(); }
    virtual void SetView( Point3 p ) { pDelegate->SetView( p ); }
    virtual Point3 OrigView() { return pDelegate->OrigView(); }
    virtual Point3 ReflectVector() { return pDelegate->ReflectVector(); }
    virtual Point3 RefractVector( float ior ) { return pDelegate->RefractVector( ior ); }
    virtual void SetIOR( float ior ) { pDelegate->SetIOR( ior ); }
    virtual float GetIOR() { return pDelegate->GetIOR(); }
    virtual Point3 CamPos() { return pDelegate->CamPos(); }
    virtual Point3 P() { return newP; }
    virtual Point3 DP() { return pDelegate->DP(); }
    virtual void DP( Point3& dpdx, Point3& dpdy ) { pDelegate->DP( dpdx, dpdy ); }
    virtual Point3 PObj() { return pDelegate->PObj(); }
    virtual Point3 DPObj() { return pDelegate->DPObj(); }
    virtual Box3 ObjectBox() { return pDelegate->ObjectBox(); }
    virtual Point3 PObjRelBox() { return pDelegate->PObjRelBox(); }
    virtual Point3 DPObjRelBox() { return pDelegate->DPObjRelBox(); }
    virtual void ScreenUV( Point2& uv, Point2& duv ) { pDelegate->ScreenUV( uv, duv ); }
    virtual IPoint2 ScreenCoord() { return pDelegate->ScreenCoord(); }
    virtual Point2 SurfacePtScreen() { return pDelegate->SurfacePtScreen(); }
    virtual Point3 UVW( int channel = 0 ) { return pDelegate->UVW( channel ); }
    virtual Point3 DUVW( int channel = 0 ) { return pDelegate->DUVW( channel ); }
    virtual void DPdUVW( Point3 dP[3], int channel = 0 ) { return pDelegate->DPdUVW( dP, channel ); }
    virtual int BumpBasisVectors( Point3 dP[2], int axis, int channel = 0 ) {
        return pDelegate->BumpBasisVectors( dP, axis, channel );
    }
    virtual BOOL IsSuperSampleOn() { return pDelegate->IsSuperSampleOn(); }
    virtual BOOL IsTextureSuperSampleOn() { return pDelegate->IsTextureSuperSampleOn(); }
    virtual int GetNSuperSample() { return pDelegate->GetNSuperSample(); }
    virtual float GetSampleSizeScale() { return pDelegate->GetSampleSizeScale(); }
    virtual Point3 UVWNormal( int channel = 0 ) { return pDelegate->UVWNormal( channel ); }
    virtual float RayDiam() { return pDelegate->RayDiam(); }
    virtual float RayConeAngle() { return pDelegate->RayConeAngle(); }
    virtual AColor EvalEnvironMap( Texmap* map, Point3 view ) { return pDelegate->EvalEnvironMap( map, view ); }
    virtual void GetBGColor( Color& bgcol, Color& transp, BOOL fogBG = TRUE ) {
        pDelegate->GetBGColor( bgcol, transp, fogBG );
    }
    virtual float CamNearRange() { return pDelegate->CamNearRange(); }
    virtual float CamFarRange() { return pDelegate->CamFarRange(); }
    virtual Point3 PointTo( const Point3& p, RefFrame ito ) { return PointTo( p, ito ); }
    virtual Point3 PointFrom( const Point3& p, RefFrame ifrom ) { return PointFrom( p, ifrom ); }
    virtual Point3 VectorTo( const Point3& p, RefFrame ito ) { return pDelegate->VectorTo( p, ito ); }
    virtual Point3 VectorFrom( const Point3& p, RefFrame ifrom ) { return pDelegate->VectorFrom( p, ifrom ); }
};

// bool calculate_lighting_at( const vector3f& worldPos, const field_collection::sample& posSample,
// frantic::graphics::color3f& outLight, std::vector<light_data>& lights, float lightParam ){ 	outLight =
//frantic::graphics::color3f::black();
//
//	if( posSample.density <= 1e-5f )
//		return false;
//
//	for( std::vector<light_data>::iterator it = lights.begin(), itEnd = lights.end(); it != itEnd; ++it ){
//		frantic::graphics::color3f light = it->light_at( lightParam );
//		if( light.r > 0.f || light.g > 0.f || light.b > 0.f ){
//			frantic::graphics::ray3f rLight(worldPos, it->m_lightPos - worldPos);
//
//			frantic::graphics::color3f totalExtinction;
//			m_fields.calculate_extinction( rLight, 1.0, m_maxStep, totalExtinction );
//
//			outLight.r += light.r * exp( -m_lightDensityScale * totalExtinction.r );
//			outLight.g += light.g * exp( -m_lightDensityScale * totalExtinction.g );
//			outLight.b += light.b * exp( -m_lightDensityScale * totalExtinction.b );
//		}
//	}
// }

inline bool EmberAtmospheric::calculate_lighting_at( const vector3f& worldPos, ShadeContext& sc,
                                                     const field_collection::sample& posSample,
                                                     frantic::graphics::color3f& outLight ) const {
    outLight = frantic::graphics::color3f::black();

    if( posSample.density <= 1e-5f )
        return false;

    /*AtmosShadeContext atmosSC( sc, sc.PointFrom( frantic::max3d::to_max_t(worldPos), REF_WORLD ) );

    for( int i = 0; i < sc.nLights; ++i ){
            if( LightDesc* pLight = sc.Light(i) ){
                    Point3 pL = pLight->LightPosition();

                    Color l;
                    Point3 dL;
                    float nl, dc;

                    if( pLight->Illuminate( atmosSC, pL - sc.P(), l, dL, nl, dc ) ){
                            frantic::graphics::vector3f lightPos = frantic::max3d::from_max_t( sc.PointTo(pL, REF_WORLD)
    ); frantic::graphics::ray3f rLight(worldPos, lightPos - worldPos);

                            frantic::graphics::color3f totalExtinction;
                            m_fields.calculate_extinction( rLight, 1.0, m_maxStep, totalExtinction );

                            outLight.r += l.r * exp( -m_lightDensityScale * totalExtinction.r );
                            outLight.g += l.g * exp( -m_lightDensityScale * totalExtinction.g );
                            outLight.b += l.b * exp( -m_lightDensityScale * totalExtinction.b );
                    }
            }
    }*/

    return true;
}

bool EmberAtmospheric::calculate_lighting_at2( const vector3f& worldPos, const field_collection::sample& posSample,
                                               frantic::graphics::color3f& outLight, std::vector<light_data>& lights,
                                               double t ) const {
    outLight = frantic::graphics::color3f::black();

    if( posSample.density <= 1e-5f )
        return false;

    for( std::vector<light_data>::iterator it = lights.begin(), itEnd = lights.end(); it != itEnd; ++it ) {
        frantic::graphics::color3f light = it->light_at( static_cast<float>( t ) );
        if( light.r > 0 || light.g > 0 || light.b > 0 ) {

            frantic::graphics::ray3f rLight = it->get_ray_to_light( worldPos );

            if( m_doAbsorption ) {
                frantic::graphics::color3f totalExtinction;
                m_fields.calculate_extinction( rLight, 0.0, 1.0, m_maxStep, totalExtinction );

                outLight.r += light.r * exp( -m_lightDensityScale * totalExtinction.r );
                outLight.g += light.g * exp( -m_lightDensityScale * totalExtinction.g );
                outLight.b += light.b * exp( -m_lightDensityScale * totalExtinction.b );
            } else {
                float totalDensity = 0.f;
                m_fields.calculate_extinction( rLight, 0.0, 1.0, m_maxStep, totalDensity );

                outLight += light * exp( -m_lightDensityScale * totalDensity );
            }
        }
    }

    return true;
}

namespace {
struct interval_start_greater {
  public:
    bool operator()( const boost::tuple<std::size_t, double, double>& lhs,
                     const boost::tuple<std::size_t, double, double>& rhs ) const {
        return lhs.get<1>() > rhs.get<1>();
    }
};

struct interval_start_lessequal_const {
    double m_value;

  public:
    interval_start_lessequal_const( double value )
        : m_value( value ) {}

    bool operator()( const boost::tuple<std::size_t, double, double>& lhs ) const { return lhs.get<1>() <= m_value; }
};

struct interval_end_greater {
  public:
    bool operator()( const boost::tuple<std::size_t, double, double>& lhs,
                     const boost::tuple<std::size_t, double, double>& rhs ) const {
        return lhs.get<2>() > rhs.get<2>();
    }
};

// We store the intervals in a pair of heaps in the same vector. The active intervals are stored at the beginning of the
// vector in order of closest end. The inactive intervals are stored at the end of the vector in order of closest start.
// In between is garbage.
void init_intervals( interval_container& intervals,
                     std::pair<interval_container::iterator, interval_container::reverse_iterator>& outEnds,
                     double tStart ) {
    std::vector<boost::tuple<std::size_t, double, double>>::iterator it =
        std::partition( intervals.begin(), intervals.end(), interval_start_lessequal_const( tStart ) );

    outEnds.first = it;
    outEnds.second = intervals.rbegin() + ( intervals.end() - it );

    std::make_heap( intervals.begin(), outEnds.first, interval_end_greater() );
    std::make_heap( intervals.rbegin(), outEnds.second,
                    interval_start_greater() ); // A reverse heap stores the heap from the end of the vector.
}

void advance_intervals( interval_container& intervals,
                        std::pair<interval_container::iterator, interval_container::reverse_iterator>& inoutEnds,
                        double t0, double t1 ) {
    // Pop all intervals that are now swept past.
    std::vector<boost::tuple<std::size_t, double, double>>::iterator it = intervals.begin();
    while( it != inoutEnds.first && it->get<2>() < t0 ) {
        std::pop_heap( it, inoutEnds.first, interval_end_greater() );

        --inoutEnds.first;
    }

    // Move all intervals that are now active.
    std::vector<boost::tuple<std::size_t, double, double>>::reverse_iterator itR = intervals.rbegin();
    while( itR != inoutEnds.second && itR->get<1>() <= t1 ) {
        // Must pop the heap before moving to the other heap area. Otherwise we might overwrite valid data.
        std::pop_heap( itR, inoutEnds.second, interval_start_greater() );

        // Calling pop_heap removed an item from the range so we decrement the end iterator. It will also happen that
        // the new iterator points at the popped item.
        --inoutEnds.second;

        // TODO: Maybe check for self-assignment? Happens if we are only activating an interval when we haven't removed
        // any.
        *inoutEnds.first = *inoutEnds.second;

        // We just added a new item to the active range so extend the range and update the heap.
        ++inoutEnds.first;

        // Update the heap of active intervals.
        std::push_heap( it, inoutEnds.first, interval_end_greater() );
    }
}
} // namespace

void EmberAtmospheric::subdivide_recursive( const frantic::graphics::ray3f& r, double tLeft, double tRight,
                                            interval_container::const_iterator intervalsStart,
                                            interval_container::const_iterator intervalsEnd,
                                            std::vector<light_data>& lightData, frantic::graphics::color6f& fullVal,
                                            const field_collection::sample& prevSample,
                                            const field_collection::sample& nextSample,
                                            const frantic::graphics::color3f& prevLight,
                                            const frantic::graphics::color3f& nextLight ) {
    field_collection::sample midSample;
    frantic::graphics::color3f midLight;

    double tCur = 0.5f * ( tLeft + tRight );

    vector3f worldPos = r.at( static_cast<float>( tCur ) );

    for( interval_container::const_iterator it = intervalsStart; it != intervalsEnd; ++it ) {
        field_collection::sample s;

        m_fields.sample_field( it->get<0>(), worldPos, s );

        midSample.density += s.density;
        midSample.color += s.density * s.color;
        if( m_doAbsorption )
            midSample.extinction += s.density * s.extinction;
        if( m_doEmission )
            midSample.emission += s.emission;
    }

    // bool midValid = this->calculate_lighting_at( worldPos, sc, midSample, midLight );
    bool midValid = this->calculate_lighting_at2( worldPos, midSample, midLight, lightData, tCur );
    if( midValid ) {
        // midLight += ambientLight; // TODO: Include an ambient occlusion term.
        midLight *= m_cameraDensityScale * prevSample.color;
        if( m_doEmission )
            midLight += m_cameraEmissionScale * prevSample.emission;
    }

    frantic::graphics::color6f left, right;

    float dist = 0.5f * static_cast<float>( r.direction().get_magnitude() * ( tRight - tLeft ) );

    if( m_doAbsorption ) {
        float distScaled = dist * m_cameraDensityScale;
        frantic::graphics::color3f extPrev = distScaled * prevSample.extinction;
        frantic::graphics::color3f extMid = distScaled * midSample.extinction;
        frantic::graphics::color3f extNext = distScaled * nextSample.extinction;

        boost::tie( left.c.r, left.a.ar ) = integrate_scattered_light( prevLight.r, midLight.r, extPrev.r, extMid.r );
        boost::tie( left.c.g, left.a.ag ) = integrate_scattered_light( prevLight.g, midLight.g, extPrev.g, extMid.g );
        boost::tie( left.c.b, left.a.ab ) = integrate_scattered_light( prevLight.b, midLight.b, extPrev.b, extMid.b );

        boost::tie( right.c.r, right.a.ar ) = integrate_scattered_light( midLight.r, nextLight.r, extMid.r, extNext.r );
        boost::tie( right.c.g, right.a.ag ) = integrate_scattered_light( midLight.g, nextLight.g, extMid.g, extNext.g );
        boost::tie( right.c.b, right.a.ab ) = integrate_scattered_light( midLight.b, nextLight.b, extMid.b, extNext.b );
    } else {
        float distScaled = dist * m_cameraDensityScale;
        float densityScaledPrev = distScaled * prevSample.density;
        float densityScaledMid = distScaled * midSample.density;
        float densityScaledNext = distScaled * nextSample.density;

        boost::tie( left.c.r, left.a.ar ) =
            integrate_scattered_light( prevLight.r, midLight.r, densityScaledPrev, densityScaledMid );
        boost::tie( left.c.g, left.a.ag ) =
            integrate_scattered_light( prevLight.g, midLight.g, densityScaledPrev, densityScaledMid );
        boost::tie( left.c.b, left.a.ab ) =
            integrate_scattered_light( prevLight.b, midLight.b, densityScaledPrev, densityScaledMid );

        boost::tie( right.c.r, right.a.ar ) =
            integrate_scattered_light( midLight.r, nextLight.r, densityScaledMid, densityScaledNext );
        boost::tie( right.c.g, right.a.ag ) =
            integrate_scattered_light( midLight.g, nextLight.g, densityScaledMid, densityScaledNext );
        boost::tie( right.c.b, right.a.ab ) =
            integrate_scattered_light( midLight.b, nextLight.b, densityScaledMid, densityScaledNext );
    }

    left.c *= dist;
    right.c *= dist;

    frantic::graphics::color6f accum = left;
    accum.blend_over( right );

    if( dist <= m_minStep || ( fullVal.c - accum.c ).max_abs_component() < 1e-3f ) {
        fullVal = accum;
    } else {
        subdivide_recursive( r, tLeft, tCur, intervalsStart, intervalsEnd, lightData, left, prevSample, midSample,
                             prevLight, midLight );
        subdivide_recursive( r, tCur, tRight, intervalsStart, intervalsEnd, lightData, right, midSample, nextSample,
                             midLight, nextLight );

        left.blend_over( right );

        fullVal = left;
    }
}

frantic::graphics::color6f EmberAtmospheric::raymarch( const frantic::graphics::ray3f& r, double t0, double t1,
                                                       ShadeContext& sc ) {
    interval_container intervals; // This is a memory allocation that we would like to avoid.

    m_fields.calculate_ray_intervals( r, t0, t1, intervals );

    if( intervals.empty() )
        return frantic::graphics::color6f( frantic::graphics::color3f::black(), frantic::graphics::alpha3f( 0.f ) );

    const double dirMag = r.direction().get_magnitude();
    const double tStep = 2.0 * static_cast<double>( m_maxStep ) /
                         dirMag; // Double, because we always subdivide at least once to see if we are converging.

    // Find the min & max of the overall intervals.
    double minStart = std::numeric_limits<float>::infinity();
    double maxEnd = -std::numeric_limits<float>::infinity();
    for( std::vector<boost::tuple<std::size_t, double, double>>::iterator it = intervals.begin(),
                                                                          itEnd = intervals.end();
         it != itEnd; ++it ) {
        // Give a little padding to the bounds so we don't miss anything at the edges.
        it->get<1>() -= 0.5 * static_cast<double>( m_minStep ) / dirMag;
        it->get<2>() += 0.5 * static_cast<double>( m_minStep ) / dirMag;

        if( it->get<1>() < minStart )
            minStart = it->get<1>();
        if( it->get<2>() > maxEnd )
            maxEnd = it->get<2>();
    }

    // Update t0 & t1 to the intersect with the intervals we received.
    if( minStart > t0 )
        t0 = minStart;
    if( maxEnd < t1 )
        t1 = maxEnd;

    // If we created an empty interval return black. This should probably never happen unless we messed up.
    if( t1 <= t0 )
        return frantic::graphics::color6f( frantic::graphics::color3f::black(), frantic::graphics::alpha3f( 0.f ) );

    std::pair<interval_container::iterator, interval_container::reverse_iterator> rangeEnds;

    init_intervals( intervals, rangeEnds, minStart );

    // We should have at least one active interval.
    assert( intervals.begin() != rangeEnds.first );

    // Calculate the lighting intervals.
    std::vector<light_data> lightData;

    lightData.resize( m_lights.size() );

    std::vector<light_data>::iterator itOut = lightData.begin();

    for( std::vector<ObjLightDesc*>::const_iterator it = m_lights.begin(), itEnd = m_lights.end(); it != itEnd;
         ++it, ++itOut ) {
        itOut->m_lightPos = frantic::max3d::from_max_t( ( *it )->lightToWorld.GetTrans() );

        Ray maxRay;
        maxRay.p = frantic::max3d::to_max_t( r.at( t0 ) );
        maxRay.dir = frantic::max3d::to_max_t( r.direction() );

        int nSamples =
            static_cast<int>( std::ceil( 2.0 * ( t1 - t0 ) / tStep ) ); // 2 is because tStep is doubled above.

        ( *it )->TraverseVolume( sc, maxRay, nSamples, static_cast<float>( t1 - t0 ), 1.f, 1.f, TRAVERSE_LOWFILTSHADOWS,
                                 &*itOut );

        itOut->m_intervalIt = itOut->m_intervals.begin();

        if( !itOut->m_intervals.empty() ) {
            for( std::vector<std::pair<float, frantic::graphics::color3f>>::iterator
                     itIvl = itOut->m_intervals.begin() + 1,
                     itIvlEnd = itOut->m_intervals.end();
                 itIvl != itIvlEnd; ++itIvl )
                itIvl->first += static_cast<float>( t0 );
        }

        /*if( !itOut->m_intervals.empty() ){
                long curID = InterlockedIncrement( &counter );

                frantic::tstring prtFile( _T("C:\\Users\\Darcy\\Test_") + frantic::tstring( (*it)->inode->GetName() ) +
        _T("_part") + frantic::strings::zero_pad(curID,4) + _T("of1000_0000.prt") );

                frantic::channels::channel_map map;
                map.define_channel<frantic::graphics::vector3f>( _T("Position") );
                map.define_channel<frantic::graphics::vector3f>( _T("Normal") );
                map.define_channel<frantic::graphics::color3f>( _T("Color") );
                map.define_channel<float>( _T("Length") );
                map.end_channel_definition();

                frantic::particles::particle_array parts( map );

                if( frantic::files::file_exists( prtFile ) )
                        parts.insert_particles( frantic::particles::particle_file_istream_factory( prtFile, map ) );

                frantic::channels::channel_accessor<frantic::graphics::vector3f> posAcc =
        map.get_accessor<frantic::graphics::vector3f>( _T("Position") );
                frantic::channels::channel_accessor<frantic::graphics::vector3f> normAcc =
        map.get_accessor<frantic::graphics::vector3f>( _T("Normal") );
                frantic::channels::channel_accessor<frantic::graphics::color3f> colAcc =
        map.get_accessor<frantic::graphics::color3f>( _T("Color") ); frantic::channels::channel_accessor<float> lenAcc =
        map.get_accessor<float>( _T("Length") );

                char* buffer = static_cast<char*>( alloca( map.structure_size() ) );

                float last = -1.f;
                for( std::vector< std::pair<float, frantic::graphics::color3f> >::const_iterator itIvl =
        itOut->m_intervals.begin(), itIvlEnd = itOut->m_intervals.end(); itIvl != itIvlEnd; ++itIvl ){ posAcc.get(
        buffer ) = r.at( itIvl->first ); normAcc.get( buffer ) = r.at( last ) - posAcc.get( buffer ); colAcc.get( buffer
        ) = itIvl->second; lenAcc.get( buffer ) = itIvl->first - last;

                        parts.push_back( buffer );

                        last = itIvl->first;
                }

                parts.write_particles( frantic::particles::particle_file_ostream_factory( prtFile, map, map ) );
        }*/
    }

    field_collection::sample nextSample, prevSample;
    frantic::graphics::color3f prevLight, nextLight;
    frantic::graphics::color6f result;

    frantic::graphics::vector3f p0 = r.at( static_cast<float>( t0 ) );

    for( interval_container::iterator it = intervals.begin(); it != rangeEnds.first; ++it ) {
        field_collection::sample s;

        m_fields.sample_field( it->get<0>(), p0, s );

        prevSample.density += s.density;
        prevSample.color += s.density * s.color;
        if( m_doAbsorption )
            prevSample.extinction += s.density * s.extinction;
        if( m_doEmission )
            prevSample.emission += s.emission;
    }

    // bool prevValid = calculate_lighting_at( p0, sc, prevSample, prevLight );
    bool prevValid = calculate_lighting_at2( p0, prevSample, prevLight, lightData, t0 );
    if( prevValid ) {
        // prevLight += ambientLight; // TODO: Include an ambient occlusion term.
        prevLight *= m_cameraDensityScale * prevSample.color;
        prevLight += m_cameraEmissionScale * prevSample.emission;
    }

    double tPrev = t0;
    double tCur = t0 + tStep;
    if( tCur > t1 )
        tCur = t1;

    while( tCur <= t1 ) {
        advance_intervals( intervals, rangeEnds, tPrev, tCur ); // Advance the 'active' intervals

        float dist = static_cast<float>( dirMag * ( tCur - tPrev ) );
        vector3f worldPos = r.at( static_cast<float>( tCur ) );

        nextSample.density = 0.f;
        nextSample.color.set( 0, 0, 0 );
        nextSample.extinction.set( 0, 0, 0 );
        nextSample.emission.set( 0, 0, 0 );

        for( interval_container::iterator it = intervals.begin(); it != rangeEnds.first; ++it ) {
            field_collection::sample s;

            m_fields.sample_field( it->get<0>(), worldPos, s );

            nextSample.density += s.density;
            nextSample.color += s.density * s.color;
            if( m_doAbsorption )
                nextSample.extinction += s.density * s.extinction;
            if( m_doEmission )
                nextSample.emission += s.emission;
        }

        // bool nextValid = calculate_lighting_at( worldPos, sc, nextSample, nextLight );
        bool nextValid = calculate_lighting_at2( worldPos, nextSample, nextLight, lightData, tCur );

        if( nextValid || prevValid ) {
            // nextLight += sc.ambientLight; // TODO: Include an ambient occlusion term.
            nextLight *= m_cameraDensityScale * prevSample.color;
            if( m_doEmission )
                nextLight += m_cameraEmissionScale * prevSample.emission;

            frantic::graphics::color6f cur;

            // Explicitly sample the end of the current segment so we can resuse the the field values and incident
            // lighting information. Compute a first approximation of the integral over the segment, then recusively
            // subdivide until convergence.

            if( m_doAbsorption ) {
                float distScaled = dist * m_cameraDensityScale;
                frantic::graphics::color3f extPrev = distScaled * prevSample.extinction;
                frantic::graphics::color3f extNext = distScaled * nextSample.extinction;

                boost::tie( cur.c.r, cur.a.ar ) =
                    integrate_scattered_light( prevLight.r, nextLight.r, extPrev.r, extNext.r );
                boost::tie( cur.c.g, cur.a.ag ) =
                    integrate_scattered_light( prevLight.g, nextLight.g, extPrev.g, extNext.g );
                boost::tie( cur.c.b, cur.a.ab ) =
                    integrate_scattered_light( prevLight.b, nextLight.b, extPrev.b, extNext.b );
            } else {
                float distScaled = dist * m_cameraDensityScale;
                float densityScaledPrev = distScaled * prevSample.density;
                float densityScaledNext = distScaled * nextSample.density;

                boost::tie( cur.c.r, cur.a.ar ) =
                    integrate_scattered_light( prevLight.r, nextLight.r, densityScaledPrev, densityScaledNext );
                boost::tie( cur.c.g, cur.a.ag ) =
                    integrate_scattered_light( prevLight.g, nextLight.g, densityScaledPrev, densityScaledNext );
                boost::tie( cur.c.b, cur.a.ab ) =
                    integrate_scattered_light( prevLight.b, nextLight.b, densityScaledPrev, densityScaledNext );
            }

            cur.c *= dist;

            // Recursively apply Simpson's rule for integration assuming the lighting, shadowing, etc. are linear on the
            // interval.
            subdivide_recursive( r, tPrev, tCur, intervals.begin(), rangeEnds.first, lightData, cur, prevSample,
                                 nextSample, prevLight, nextLight );

            result.c += result.a.occlude( cur.c );
            result.a.blend_over( cur.a );

            // TODO: Make this an appropriate value.
            //  If we are reasonably sure that this ray has become completely opaque, then we can just finish marching
            //  and discard the remainder.
            if( result.a.ar > ( 1.0 - 1e-4 ) && result.a.ag > ( 1.0 - 1e-4 ) && result.a.ab > ( 1.0 - 1e-4 ) )
                break;

            prevSample = nextSample;
            prevLight = nextLight;
            prevValid = nextValid;
        }

        // I thought about having the stepsize grow as alpha grows.
        // tStep = std::max((double)m_minStepSize, (double)(accumAlpha.ar * m_maxStepSize) / dirMag);

        // If there are no active intervals, determine where to jump to!
        if( intervals.begin() == rangeEnds.first &&
            intervals.rbegin() != rangeEnds.second ) { // ie. No active intervals
            double nextInterval = intervals.back().get<1>();
            double numSteps = std::ceil( ( nextInterval - tCur ) / tStep );

            if( numSteps > 2 )
                tCur += ( tStep * numSteps ) - tStep; // Jump over all the empty regions since they don't matter.
        }

        tPrev = tCur;
        tCur += tStep;
        if( tCur > t1 ) {
            if( t1 - tPrev > 1e-5 )
                tCur = t1;
        }
    }

    return result;
}

frantic::graphics::alpha3f EmberAtmospheric::raymarch_opacity( const frantic::graphics::ray3f& r, double t0,
                                                               double t1 ) {
    frantic::graphics::color3f extinction;
    m_fields.calculate_extinction( r, t0, t1, m_maxStep, extinction );

    return frantic::graphics::alpha3f( 1.f - std::exp( -m_lightDensityScale * extinction.r ),
                                       1.f - std::exp( -m_lightDensityScale * extinction.g ),
                                       1.f - std::exp( -m_lightDensityScale * extinction.b ) );
}

void EmberAtmospheric::Shade( ShadeContext& sc, const Point3& p0, const Point3& p1, Color& color, Color& trans,
                              BOOL isBG ) {
    frantic::graphics::vector3fd start, dir;
    double t0 = 0.0, t1 = 1.0;

    start = frantic::max3d::from_max_t( sc.PointTo( p0, REF_WORLD ) );

    if( isBG ) {
        dir = frantic::max3d::from_max_t( sc.VectorTo( sc.V(), REF_WORLD ) );
        t1 = std::numeric_limits<float>::max();
    } else {
        dir = frantic::max3d::from_max_t( sc.PointTo( p1, REF_WORLD ) );
        dir -= start;

        // Some contexts (The reflection map in Max 2015 for example) have p1 be a point arbitrarily far along the line
        // segment of interest. In order to avoid numerical issues, we normalize the direction and adjuct t1.
        t1 = dir.get_magnitude();
        dir /= t1;
    }

    const frantic::graphics::vector3f startF( start );
    const frantic::graphics::vector3f dirF( dir );
    frantic::graphics::color6f result;
    frantic::graphics::ray3f r = frantic::graphics::ray3f( startF, dirF );

    if( sc.mode == SCMODE_SHADOW ) {
        result.a = this->raymarch_opacity( r, t0, t1 );
    } else {
        result = this->raymarch( r, t0, t1, sc );
    }

    color.r = result.c.r + ( 1.f - result.a.ar ) * color.r;
    color.g = result.c.g + ( 1.f - result.a.ag ) * color.g;
    color.b = result.c.b + ( 1.f - result.a.ab ) * color.b;

    trans.r = 1.f - ( result.a.ar + ( 1.f - result.a.ar ) * ( 1.f - trans.r ) );
    trans.g = 1.f - ( result.a.ag + ( 1.f - result.a.ag ) * ( 1.f - trans.g ) );
    trans.b = 1.f - ( result.a.ab + ( 1.f - result.a.ab ) * ( 1.f - trans.b ) );
}

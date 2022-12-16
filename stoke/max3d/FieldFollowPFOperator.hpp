// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

//#include <stoke/max3d/PFlowOperator/FloodVelocityLoader.hpp>

#pragma warning( push, 3 )
#include <ParticleFlow/PFSimpleOperator.h>
#pragma warning( pop )

//#include <frantic/volumetrics/levelset/dense_level_set.hpp>
//#include <frantic/volumetrics/levelset/rle_level_set.hpp>

namespace ember {
namespace max3d {

// forward declare
ClassDesc2* GetFloodVelocityLoaderPFOperatorDesc();

/**
 * This class implements a Flood Velocity particle flow operator.
 * This operater is to be used with the Flood Velocity Loader plugin.
 */
class FloodVelocityLoaderPFOperator : public PFSimpleOperator {
  public:
    FloodVelocityLoaderPFOperator();
    virtual ~FloodVelocityLoaderPFOperator();

    // from IPFAction
    virtual const ParticleChannelMask& ChannelsUsed( const Interval& time ) const;
    virtual const Interval ActivityInterval() const;

    // From IPFOperator interface
    virtual bool Proceed( IObject* pCont, PreciseTimeValue timeStart, PreciseTimeValue& timeEnd, Object* pSystem,
                          INode* pNode, INode* actionNode, IPFIntegrator* integrator );

    // from IPViewItem interface
    virtual bool HasCustomPViewIcons() { return true; }
    virtual HBITMAP GetActivePViewIcon();
    virtual HBITMAP GetInactivePViewIcon();
    virtual bool HasDynamicName( TSTR& nameSuffix );

    // From FPMixinInterface
    virtual FPInterfaceDesc* GetDescByID( Interface_ID id );

    // From PFSimpleAction
    virtual ClassDesc* GetClassDesc() const;

// From BaseObject
#if MAX_VERSION_MAJOR < 24
    virtual TYPE_STRING_TYPE GetObjectName();
#else
    virtual TYPE_STRING_TYPE GetObjectName( bool localized ) const;
#endif

    // From ReferenceTarget
    virtual RefTargetHandle Clone( RemapDir& remap );

    // From ReferenceMaker
    virtual RefResult NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL /*propagate*/ );

    // From Animatable
    virtual Class_ID ClassID();
#if MAX_VERSION_MAJOR < 24
    virtual void GetClassName( TSTR& s );
#else
    virtual void GetClassName( TSTR& s, bool localized ) const;
#endif

    virtual void BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev );
    virtual void EndEditParams( IObjParam* ip, ULONG flags, Animatable* next );

  private:
    // passes a message to the param map dialog proc
    void PassMessage( int message, LPARAM param );
};

} // namespace max3d
} // namespace ember

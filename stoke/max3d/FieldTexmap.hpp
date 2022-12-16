// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/max3d/GenericReferenceTarget.hpp>

#include <frantic/graphics/transform4f.hpp>
#include <frantic/volumetrics/field_interface.hpp>

#pragma warning( push, 3 )
#include <imtl.h>
#pragma warning( pop )

#define FieldTexmap_CLASSID Class_ID( 0x78705ce0, 0x65bb5358 )
#define FieldTexmap_NAME "FieldTexmap"
#define FieldTexmap_DISPLAYNAME "Field Texmap"
#define FieldTexmap_VERSION 1

class IParamBlock2; // Forward decl.
class XYZGen;

namespace ember {
namespace max3d {

/**
 * This texmap uses Ember technology (via a picked Ember scene node or possibly its own Ember/magma expression) to
 * implement a 3D texmap.
 */
class FieldTexmap : public frantic::max3d::GenericReferenceTarget<Texmap, FieldTexmap> {
  public:
    FieldTexmap( BOOL loading );
    virtual ~FieldTexmap();

    // From GenericReferenceTarget
    virtual int NumRefs() { return GenericReferenceTarget<Texmap, FieldTexmap>::NumRefs() + 1; }
    virtual ReferenceTarget* GetReference( int i );
    virtual void SetReference( int i, ReferenceTarget* r );

    virtual int NumSubs() { return GenericReferenceTarget<Texmap, FieldTexmap>::NumSubs() + 1; }
    virtual Animatable* SubAnim( int i );

#if MAX_VERSION_MAJOR < 24
    virtual TSTR SubAnimName( int i );
#else
    virtual TSTR SubAnimName( int i, bool localized );
#endif

    // From MtlBase
    virtual ParamDlg* CreateParamDlg( HWND hwMtlEdit, IMtlParams* imp );
    virtual BOOL SetDlgThing( ParamDlg* /*dlg*/ ) { return FALSE; }
    virtual void Update( TimeValue t, Interval& valid );
    virtual void Reset();
    virtual Interval Validity( TimeValue t );
    virtual ULONG LocalRequirements( int /*subMtlNum*/ ) { return 0; }

    virtual int NumSubTexmaps() { return 0 /*2*/; }
    virtual Texmap* GetSubTexmap( int i );
    virtual void SetSubTexmap( int i, Texmap* m );
#if MAX_VERSION_MAJOR < 24
    virtual TSTR GetSubTexmapSlotName( int i );
#else
    virtual TSTR GetSubTexmapSlotName( int i, bool localized );
#endif

    // From Texmap
    virtual AColor EvalColor( ShadeContext& sc );
    virtual float EvalMono( ShadeContext& sc );
    virtual Point3 EvalNormalPerturb( ShadeContext& sc );

    virtual BOOL SupportTexDisplay() { return FALSE; }
    virtual void ActivateTexDisplay( BOOL /*onoff*/ ) {}
    virtual DWORD_PTR GetActiveTexHandle( TimeValue /*t*/, TexHandleMaker& /*thmaker*/ ) { return 0; }
    virtual void GetUVTransform( Matrix3& uvtrans ) { uvtrans.IdentityMatrix(); }
    virtual int GetTextureTiling() { return 0; }
    virtual int GetUVWSource() { return 0; }
    virtual UVGen* GetTheUVGen() { return NULL; }

    virtual int SubNumToRefNum( int subNum ) { return subNum; }

    // From Animatable
    virtual IOResult Load( ILoad* iload );
    virtual IOResult Save( ISave* isave );

    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );

    virtual int RenderBegin( TimeValue t, ULONG flags );
    virtual int RenderEnd( TimeValue t );

    // Callbacks
    void on_node_picked( TimeValue t ); // Called when a new node is selected
    void on_node_changed(
        TimeValue t ); // Called when the picked node changes. (ie. REFMSG_CHANGE is sent w/ PART_GEOM specified).

  protected:
    virtual ClassDesc2* GetClassDesc();

  private:
    void initialize( TimeValue t );
    void cache_field( TimeValue t );

    friend class FieldTexmapDlgProc;

    void get_available_channels( TimeValue t, std::vector<frantic::tstring>& outChannels );

  private:
    XYZGen* m_xyzGen;

    bool m_inRenderMode;
    Interval m_updateInterval; // This isn't strictly necessary right now, but it allows animated params beyond the
                               // target field.

    Interval m_fieldInterval;
    boost::shared_ptr<frantic::volumetrics::field_interface> m_field;

    frantic::channels::channel_cvt_accessor<frantic::graphics::color3f> m_colorAccessor;
    frantic::channels::channel_cvt_accessor<float> m_monoAccessor;
};

} // namespace max3d
} // namespace ember

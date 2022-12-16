// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/EmberObjectBase.hpp>

#include <frantic/max3d/max_utility.hpp>

namespace ember {
namespace max3d {

EmberObjectBase::EmberObjectBase()
    : m_evalCacheValid( NEVER ) {}

EmberObjectBase::~EmberObjectBase() {
    if( m_evalCache.obj ) {
        m_evalCache.obj->UnlockChannels( GEOM_CHANNEL );
        m_evalCache.obj->UnlockObject();
        m_evalCache.DeleteObj();
    }
}

boost::shared_ptr<frantic::volumetrics::field_interface> EmberObjectBase::create_field( INode* pNode, TimeValue t,
                                                                                        Interval& valid ) {
    this->Eval( t );
    return static_cast<frantic::max3d::volumetrics::IEmberField*>(
               static_cast<EmberPipeObject*>( m_evalCache.obj )->GetInterface( EmberField_INTERFACE ) )
        ->create_field( pNode, t, valid );
}

frantic::particles::particle_istream_ptr
EmberObjectBase::CreateStream( INode* pNode, Interval& outValidity,
                               boost::shared_ptr<frantic::max3d::particles::IMaxKrakatoaPRTEvalContext> pEvalContext ) {
    if( !pNode )
        return frantic::particles::particle_istream_ptr();

    TimeValue t = pEvalContext->GetTime();

    // Evaluate the pipeline and see if the result supports particles (and isn't this object which would cause infinite
    // recursion).
    ObjectState os = pNode->EvalWorldState( t );

    if( os.obj ) {
        frantic::max3d::particles::IMaxKrakatoaPRTObject* pIKrakatoa =
            static_cast<frantic::max3d::particles::IMaxKrakatoaPRTObject*>(
                os.obj->GetInterface( MAXKRAKATOAPRTOBJECT_INTERFACE ) );

        if( pIKrakatoa && pIKrakatoa != static_cast<frantic::max3d::particles::IMaxKrakatoaPRTObject*>( this ) )
            return pIKrakatoa->CreateStream( pNode, outValidity, pEvalContext );
    }

    outValidity = FOREVER;
    return frantic::particles::particle_istream_ptr();
}

void EmberObjectBase::GetStreamNativeChannels( INode* pNode, TimeValue t,
                                               frantic::channels::channel_map& outChannelMap ) {
    if( !pNode )
        return;

    // Evaluate the pipeline and see if the result supports particles (and isn't this object which would cause infinite
    // recursion).
    ObjectState os = pNode->EvalWorldState( t );

    if( os.obj ) {
        frantic::max3d::particles::IMaxKrakatoaPRTObject* pIKrakatoa =
            static_cast<frantic::max3d::particles::IMaxKrakatoaPRTObject*>(
                os.obj->GetInterface( MAXKRAKATOAPRTOBJECT_INTERFACE ) );

        if( pIKrakatoa && pIKrakatoa != static_cast<frantic::max3d::particles::IMaxKrakatoaPRTObject*>( this ) )
            pIKrakatoa->GetStreamNativeChannels( pNode, t, outChannelMap );
    }

    if( !outChannelMap.channel_definition_complete() )
        outChannelMap.end_channel_definition();
}

// bool EmberObjectBase::AddRenderItems(
//		const MaxSDK::Graphics::MaxContext& maxContext,
//		MaxSDK::Graphics::RenderNodeHandle& hTargetNode,
//		MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer)
//{
//	//this->Eval( maxContext.GetViewport()->.GetDisplayTime() );
//
//	bool result = false;
//
//	if( m_evalCache.obj ){
//		MaxSDK::Graphics::IAddRenderItemsHelper* pImpl = static_cast<MaxSDK::Graphics::IAddRenderItemsHelper*>(
//m_evalCache.obj->GetInterface( IADD_RENDERITEMS_HELPER_INTERFACE_ID ) ); 		if( pImpl ) 			result = pImpl->AddRenderItems(
//maxContext, hTargetNode, targetRenderItemContainer );
//	}
//
//	return result;
// }

BaseInterface* EmberObjectBase::GetInterface( Interface_ID id ) {
    /*if( id == IADD_RENDERITEMS_HELPER_INTERFACE_ID )
            return static_cast<MaxSDK::Graphics::IAddRenderItemsHelper*>( this );*/
    if( BaseInterface* result = frantic::max3d::volumetrics::IEmberField::GetInterface( id ) )
        return result;
    return frantic::max3d::GenericReferenceTarget<GeomObject, EmberObjectBase>::GetInterface( id );
}

bool EmberObjectBase::RequiresSupportForLegacyDisplayMode() const { return true; }

// bool EmberObjectBase::UpdateDisplay(
//	const MaxSDK::Graphics::MaxContext& maxContext,
//	const MaxSDK::Graphics::UpdateDisplayContext& displayContext)
//{
//	this->Eval( displayContext.GetDisplayTime() );
//
//	bool result = m_evalCache.obj->UpdateDisplay( maxContext, displayContext );
//
//	mRenderItemHandles.ClearAllRenderItems();
//	mRenderItemHandles.AddRenderItems( m_evalCache.obj->GetRenderItems() );
//
//	return result;
// }

void EmberObjectBase::GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* vp, Box3& box ) {
    this->Eval( t );
    m_evalCache.obj->GetWorldBoundBox( t, inode, vp, box );

    /*float scale = 1.f;
    Mesh* iconMesh = this->GetIconMesh( scale );

    if( scale > 0.f && inode ){
            Matrix3 tm = inode->GetNodeTM( t );
            tm.Scale( Point3(scale, scale, scale) );

            box += iconMesh->getBoundingBox() * tm;
    }*/
}

void EmberObjectBase::GetLocalBoundBox( TimeValue t, INode* inode, ViewExp* vp, Box3& box ) {
    this->Eval( t );
    m_evalCache.obj->GetLocalBoundBox( t, inode, vp, box );

    /*float scale = 1.f;
    Mesh* iconMesh = this->GetIconMesh( scale );

    if( scale > 0.f ){
            Matrix3 tm( TRUE );
            tm.Scale( Point3(scale, scale, scale) );

            box += iconMesh->getBoundingBox() * tm;
    }*/
}

int EmberObjectBase::Display( TimeValue t, INode* inode, ViewExp* vpt, int flags ) {
    this->Eval( t );

    /*GraphicsWindow* gw = vpt->getGW();
    if( gw && inode ){
            float scale = 1.f;

            Mesh* iconMesh = this->GetIconMesh( scale );

            if( scale > 0.f ){
                    Matrix3 tm = inode->GetNodeTM( t );
                    tm.Scale( Point3(scale,scale,scale) );

                    gw->setTransform( tm );

                    frantic::max3d::draw_mesh_wireframe( gw, iconMesh, inode->Selected() ?
    frantic::graphics::color3f::white() : frantic::graphics::color3f::from_RGBA( inode->GetWireColor() ) );
            }
    }*/

    return m_evalCache.obj->Display( t, inode, vpt, flags );
}

int EmberObjectBase::HitTest( TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt ) {
    this->Eval( t );

    /*GraphicsWindow* gw = vpt->getGW();
    if( gw && inode ){
            float scale = 1.f;

            Mesh* iconMesh = this->GetIconMesh( scale );

            if( scale > 0.f ){
                    HitRegion hitRegion;
                    MakeHitRegion(hitRegion, type, crossing, 4, p);

                    DWORD rndLimits = gw->getRndLimits();

                    gw->setRndLimits( GW_PICK|GW_WIREFRAME );
                    gw->setHitRegion( &hitRegion );
                    gw->clearHitCode();

                    Matrix3 tm = inode->GetNodeTM( t );
                    tm.Scale( Point3(scale,scale,scale) );

                    gw->setTransform( tm );

                    BOOL result = iconMesh->select( gw, NULL, &hitRegion );

                    gw->setRndLimits( rndLimits );

                    if( result )
                            return TRUE;
            }
    }*/

    return m_evalCache.obj->HitTest( t, inode, type, crossing, flags, p, vpt );
}

ObjectState EmberObjectBase::Eval( TimeValue t ) {
    if( !m_evalCacheValid.InInterval( t ) || !m_evalCache.obj ||
        !m_evalCache.obj->ObjectValidity( t ).InInterval( t ) ) {
        if( m_evalCache.obj ) {
            m_evalCache.obj->UnlockChannels( GEOM_CHANNEL );
            m_evalCache.obj->UnlockObject();
            m_evalCache.DeleteObj();

            NotifyDependents( FOREVER, static_cast<PartID>( PART_ALL ), REFMSG_OBJECT_CACHE_DUMPED );
        }

        std::unique_ptr<EmberPipeObject> result(
            new EmberPipeObject ); // TODO: We could just reset an existing object to default settings.

        // Ask the implementation class to evaluate itself.
        this->EvalField( *result, t );

        // TODO: Make sure the EmberPipeObject was populated correctly!

        result->CopyAdditionalChannels( this, false );

        // Lock the object & geometry channels because this instance is going to held and managed by 'this'.
        result->LockChannels( GEOM_CHANNEL );
        result->LockObject();

        m_evalCache.obj = result.release();
        m_evalCacheValid =
            m_evalCache.obj->ObjectValidity( t ); // TODO: We could have the validity calculated via EvalField()

        if( !m_evalCacheValid.InInterval( t ) )
            m_evalCacheValid.SetInstant( t );
    }

    assert( m_evalCacheValid.InInterval( t ) );
    assert( m_evalCache.obj != NULL );
    assert( m_evalCache.obj->ObjectValidity( t ).InInterval( t ) );

    // return ObjectState(this);
    return m_evalCache;
}

Interval EmberObjectBase::ObjectValidity( TimeValue t ) {
    this->Eval( t );
    return m_evalCacheValid;
}

int EmberObjectBase::CanConvertToType( Class_ID obtype ) {
    return obtype == EmberPipeObject_CLASSID || Object::CanConvertToType( obtype );
}

Object* EmberObjectBase::ConvertToType( TimeValue t, Class_ID obtype ) {
    if( obtype == EmberPipeObject_CLASSID ) {
        this->Eval( t );

        DefaultRemapDir remapDir;
        return static_cast<Object*>( m_evalCache.obj->Clone( remapDir ) );
    } else if( Object::CanConvertToType( obtype ) ) {
        return Object::ConvertToType( t, obtype );
    }
    return NULL;
}

Mesh* EmberObjectBase::GetRenderMesh( TimeValue /*t*/, INode* /*inode*/, View& /*view*/, BOOL& needDelete ) {
    static Mesh* emptyMesh = NULL;
    if( !emptyMesh )
        emptyMesh = CreateNewMesh();

    needDelete = FALSE;

    return emptyMesh;
}

void EmberObjectBase::InvalidateCache() { m_evalCacheValid.SetEmpty(); }

EmberPipeObject& EmberObjectBase::GetCurrentCache() { return *static_cast<EmberPipeObject*>( m_evalCache.obj ); }

} // namespace max3d
} // namespace ember

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/EmberPipeObject.hpp>
#include <stoke/max3d/MXSField.hpp>

#include <ember/grid.hpp>
#include <ember/openvdb.hpp>
#include <ember/staggered_grid.hpp>

#pragma warning( disable : 4503 ) // "decorated name length exceeded, name was truncated" casued by OpenVDB template
                                  // instantiations.

#include <frantic/max3d/convert.hpp>
#include <frantic/max3d/exception.hpp>
#include <frantic/max3d/volumetrics/IEmberField.hpp>

#include <modstack.h>
#include <paramtype.h>

namespace ember {
namespace max3d {

MXSField::MXSField() {
    m_bounds.Init();
    m_spacing = 1.f;
}

MXSField::~MXSField() {}

void MXSField::SetField( boost::shared_ptr<frantic::volumetrics::field_interface> pField, const Box3& bounds,
                         float spacing ) {
    m_pField = pField;
    m_bounds = bounds;
    m_spacing = spacing;

    const frantic::channels::channel_map& map = pField->get_channel_map();

    m_cachedChannelList.resize( map.channel_count() );

    for( std::size_t i = 0, iEnd = map.channel_count(); i < iEnd; ++i ) {
        const frantic::channels::channel& ch = map[i];

        m_cachedChannelList[i].printf( _T("%s %s[%d]"), ch.name().c_str(),
                                       frantic::channels::channel_data_type_str( ch.data_type() ),
                                       static_cast<int>( ch.arity() ) );
    }
}

Point3 MXSField::GetBoundsMin() { return m_bounds.pmin; }

Point3 MXSField::GetBoundsMax() { return m_bounds.pmax; }

float MXSField::GetSpacing() { return m_spacing; }

Tab<TYPE_STRING_TYPE> MXSField::GetChannels() {
    Tab<TYPE_STRING_TYPE> pResult;

    pResult.SetCount( static_cast<int>( m_cachedChannelList.size() ) );
    for( std::size_t i = 0, iEnd = m_cachedChannelList.size(); i < iEnd; ++i )
        pResult[i] = m_cachedChannelList[i].data();

    return pResult;
}

void MXSField::Clear() {
    m_pField.reset();
    m_bounds.Init();
    m_spacing = 0.f;
    m_cachedChannelList.clear();
}

FPValue MXSField::SampleField( const MCHAR* szChannelName, const Point3& position ) {
    FPValue result;
    result.LoadPtr( TYPE_VALUE, NULL );

    if( !m_pField || !m_pField->get_channel_map().has_channel( szChannelName ) )
        return result;

    char* buffer = static_cast<char*>( alloca( m_pField->get_channel_map().structure_size() ) );

    if( !m_pField->evaluate_field( buffer, frantic::max3d::from_max_t( position ) ) )
        return result;

    frantic::channels::channel_general_accessor acc = m_pField->get_channel_map().get_general_accessor( szChannelName );

    switch( acc.data_type() ) {
    case frantic::channels::data_type_float32: {
        float* val = reinterpret_cast<float*>( acc.get_channel_data_pointer( buffer ) );

        if( acc.arity() == 1 ) {
            result.LoadPtr( TYPE_FLOAT, val[0] );
        } else if( acc.arity() == 3 ) {
            Point3 p( val[0], val[1], val[2] );
            result.LoadPtr( TYPE_POINT3_BV, &p );
        }
    } break;
    case frantic::channels::data_type_int32:
        break;
    default:
        break;
    }

    return result;
}

MXSField::ThisInterfaceDesc* MXSField::GetDesc() {
    static ThisInterfaceDesc theDesc( MXSField_INTERFACE, _T("MXSField"), 0 );

    if( theDesc.empty() ) {
        theDesc.read_only_property( _T("BoundsMin"), &MXSField::GetBoundsMin );
        theDesc.read_only_property( _T("BoundsMax"), &MXSField::GetBoundsMax );
        theDesc.read_only_property( _T("Spacing"), &MXSField::GetSpacing );
        theDesc.read_only_property( _T("Channels"), &MXSField::GetChannels );

        theDesc.function( _T("Clear"), &MXSField::Clear );

        theDesc.function( _T("SampleField"), &MXSField::SampleField ).param( _T("Channel") ).param( _T("Position") );
    }

    return &theDesc;
}

MXSField* CreateMXSField( INode* pNode, TimeValue t, int stackIndex ) {
    if( !pNode )
        return NULL;

    try {
        boost::shared_ptr<frantic::volumetrics::field_interface> pField;
        Box3 bounds;
        float spacing;

        boost::shared_ptr<EmberPipeObject> pPipeObj;

        if( stackIndex <= 0 ) {
            ObjectState os = pNode->EvalWorldState( t );

            pPipeObj = frantic::max3d::safe_convert_to_type<EmberPipeObject>( os.obj, t, EmberPipeObject_CLASSID );
        } else {
            Modifier* pMod = NULL;
            int derivedObjIndex = 0;

            IDerivedObject* pDerived = GetCOREInterface7()->FindModifier( *pNode, stackIndex, derivedObjIndex, pMod );

            if( pDerived ) {
                ObjectState os = pDerived->Eval( t, derivedObjIndex );

                pPipeObj = frantic::max3d::safe_convert_to_type<EmberPipeObject>( os.obj, t, EmberPipeObject_CLASSID );
            }
        }

        if( !pPipeObj )
            return NULL;

        Interval valid = FOREVER;

        pField = pPipeObj->create_field( pNode, t, valid );
        bounds = pPipeObj->GetBounds();
        spacing = pPipeObj->GetDefaultSpacing();

        std::unique_ptr<MXSField> pResult( new MXSField );
        pResult->SetField( pField, bounds, spacing );

        return pResult.release();
    } catch( ... ) {
        frantic::max3d::rethrow_current_exception_as_max_t();
    }

    return NULL;
}

} // namespace max3d
} // namespace ember

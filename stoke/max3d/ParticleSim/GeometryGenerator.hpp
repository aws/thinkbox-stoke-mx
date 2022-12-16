// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/strings/tstring.hpp>

#include <boost/any.hpp>

#include <map>

class FPInterface;
class INode;

namespace stoke {
namespace max3d {

namespace geometry_generator {
typedef std::map<frantic::tstring, boost::any> property_map;

namespace mode {
enum value_t { surface, volume, edges, vertices };

inline value_t from_string( const frantic::tstring& modeString ) {
    if( modeString == _T("Surface") )
        return surface;
    if( modeString == _T("Volume") )
        return volume;
    if( modeString == _T("Edges") )
        return edges;
    if( modeString == _T("Vertices") )
        return vertices;
    return surface; // Default.
}
} // namespace mode

namespace selection_type {
enum value_t { none, face, vertex };

inline value_t from_string( const frantic::tstring& modeString ) {
    if( modeString == _T("None") )
        return none;
    if( modeString == _T("FaceSelection") )
        return face;
    if( modeString == _T("VertexSelection") )
        return vertex;
    return none; // Default.
}
} // namespace selection_type

namespace detail {
template <class T>
inline T get_property( const property_map& properties, const frantic::tchar* szName, T defaultValue ) {
    property_map::const_iterator it = properties.find( szName );
    if( it != properties.end() )
        return boost::any_cast<const T&>( it->second );
    return defaultValue;
}
} // namespace detail

inline void set_mode( property_map& properties, mode::value_t theMode ) { properties[_T("Mode")] = theMode; }

inline mode::value_t get_mode( const property_map& properties ) {
    return detail::get_property<mode::value_t>( properties, _T("Mode"), mode::surface );
}

inline void set_selection_type( property_map& properties, selection_type::value_t respectSelection ) {
    properties[_T("SelectionType")] = respectSelection;
}

inline selection_type::value_t get_selection_type( const property_map& properties ) {
    return detail::get_property<selection_type::value_t>( properties, _T("SelectionType"), selection_type::none );
}

inline void set_volume_spacing( property_map& properties, float spacing ) { properties[_T("Spacing")] = spacing; }

inline float get_volume_spacing( const property_map& properties ) {
    return detail::get_property<float>( properties, _T("Spacing"), 10.f );
}

inline void set_vertex_jitter_radius( property_map& properties, float jitterRadius ) {
    properties[_T("JitterRadius")] = jitterRadius;
}

inline float get_vertex_jitter_radius( const property_map& properties ) {
    return detail::get_property<float>( properties, _T("JitterRadius"), 0.f );
}
} // namespace geometry_generator

FPInterface* CreateGeometryGenerator( INode* pNode, const geometry_generator::property_map& properties );

} // namespace max3d
} // namespace stoke

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/magma/max3d/nodes/magma_input_field_node.hpp>
#include <frantic/magma/max3d/nodes/magma_texmap_node.hpp>
#include <frantic/magma/nodes/magma_foreach_containing_face_node.hpp>
#include <frantic/magma/nodes/magma_foreach_face_node.hpp>
#include <frantic/magma/nodes/magma_foreach_particle_node.hpp>
#include <frantic/magma/nodes/magma_foreach_vertex_in_face_node.hpp>
#include <frantic/magma/nodes/magma_foreach_vertex_node.hpp>
#include <stoke\max3d\Genome\MaxGeometryMagmaSingleton.hpp>
#include <stoke\max3d\Magma\magma_current_mesh_node.hpp>
#include <stoke\max3d\Magma\magma_genome_extras_node.hpp>

#include <frantic/max3d/geometry/max_mesh_interface.hpp>

namespace frantic {
namespace magma {
namespace nodes {
namespace max3d {
void define_max_nodes( frantic::magma::magma_singleton& ms );
}
} // namespace nodes
} // namespace magma
} // namespace frantic

MaxGeometryMagmaSingleton::MaxGeometryMagmaSingleton()
    : frantic::magma::magma_singleton( true ) {
    frantic::magma::nodes::max3d::define_max_nodes( *this );

    // TODO: define any mesh specific nodes
    define_node_type<frantic::magma::nodes::max3d::magma_texmap_node>();
    define_node_type<frantic::magma::nodes::max3d::magma_current_mesh_node>();
    define_node_type<frantic::magma::nodes::max3d::magma_genome_extras_node>();
    define_node_type<frantic::magma::nodes::max3d::magma_field_input_node>();
    // define_node_type<frantic::magma::nodes::max3d::magma_adjacent_faces_node>();
    // define_node_type<frantic::magma::nodes::max3d::magma_adjacent_verts_node>();
    // define_node_type<frantic::magma::nodes::max3d::magma_adjacent_vert_faces_node>();
    // define_node_type<frantic::magma::nodes::max3d::magma_input_derivative_node>();

    define_node_type<frantic::magma::nodes::magma_foreach_particle_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_particle_inputs_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_particle_outputs_node>();

    define_node_type<frantic::magma::nodes::magma_foreach_vertex_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_vertex_inputs_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_vertex_outputs_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_face_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_face_inputs_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_face_outputs_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_containing_face_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_containing_face_inputs_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_containing_face_outputs_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_vertex_in_face_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_vertex_in_face_inputs_node>();
    define_node_type<frantic::magma::nodes::magma_foreach_vertex_in_face_outputs_node>();

    define_node_type<frantic::magma::nodes::magma_loop_channel_node>();
}

MaxGeometryMagmaSingleton& MaxGeometryMagmaSingleton::get_instance() {
    static MaxGeometryMagmaSingleton theMaxGeometryMagmaSingleton;
    return theMaxGeometryMagmaSingleton;
}

void MaxGeometryMagmaSingleton::get_predefined_particle_channels(
    std::vector<std::pair<frantic::tstring, frantic::magma::magma_data_type>>& outChannels ) const {
    magma_singleton::get_predefined_particle_channels( outChannels );
}

void MaxGeometryMagmaSingleton::get_predefined_vertex_channels(
    std::vector<std::pair<frantic::tstring, frantic::magma::magma_data_type>>& outChannels ) const {
    std::vector<frantic::max3d::geometry::MaxMeshInterface::channel_info> channelInfo;

    frantic::max3d::geometry::MaxMeshInterface::get_predefined_channels(
        channelInfo, frantic::geometry::mesh_channel::vertex, false );

    for( std::vector<frantic::max3d::geometry::MaxMeshInterface::channel_info>::const_iterator
             it = channelInfo.begin(),
             itEnd = channelInfo.end();
         it != itEnd; ++it ) {
        frantic::magma::magma_data_type dt;
        dt.m_elementType = it->get<1>();
        dt.m_elementCount = it->get<2>();

        outChannels.push_back( std::make_pair( it->get<0>(), dt ) );
    }
}

void MaxGeometryMagmaSingleton::get_predefined_face_channels(
    std::vector<std::pair<frantic::tstring, frantic::magma::magma_data_type>>& outChannels ) const {
    std::vector<frantic::max3d::geometry::MaxMeshInterface::channel_info> channelInfo;

    frantic::max3d::geometry::MaxMeshInterface::get_predefined_channels( channelInfo,
                                                                         frantic::geometry::mesh_channel::face, false );

    for( std::vector<frantic::max3d::geometry::MaxMeshInterface::channel_info>::const_iterator
             it = channelInfo.begin(),
             itEnd = channelInfo.end();
         it != itEnd; ++it ) {
        frantic::magma::magma_data_type dt;
        dt.m_elementType = it->get<1>();
        dt.m_elementCount = it->get<2>();

        outChannels.push_back( std::make_pair( it->get<0>(), dt ) );
    }
}

void MaxGeometryMagmaSingleton::get_predefined_face_vertex_channels(
    std::vector<std::pair<frantic::tstring, frantic::magma::magma_data_type>>& outChannels ) const {
    std::vector<frantic::max3d::geometry::MaxMeshInterface::channel_info> channelInfo;

    frantic::max3d::geometry::MaxMeshInterface::get_predefined_channels(
        channelInfo, frantic::geometry::mesh_channel::face_vertex, false );

    for( std::vector<frantic::max3d::geometry::MaxMeshInterface::channel_info>::const_iterator
             it = channelInfo.begin(),
             itEnd = channelInfo.end();
         it != itEnd; ++it ) {
        frantic::magma::magma_data_type dt;
        dt.m_elementType = it->get<1>();
        dt.m_elementCount = it->get<2>();

        outChannels.push_back( std::make_pair( it->get<0>(), dt ) );
    }
}

// These includes are needed because we have to set the ClassIDs for the extension nodes. I want to change how that
// works...
/*#include <frantic/magma/max3d/nodes/magma_curve_op_node.hpp>
#include <frantic/magma/max3d/nodes/magma_geometry_input_node.hpp>
#include <frantic/magma/max3d/nodes/magma_input_object_node.hpp>
#include <frantic/magma/max3d/nodes/magma_input_particles_node.hpp>
#include <frantic/magma/max3d/nodes/magma_input_value_node.hpp>
#include <frantic/magma/max3d/nodes/magma_script_op_node.hpp>
#include <frantic/magma/max3d/nodes/magma_transform_node.hpp>

namespace frantic{ namespace magma{ namespace nodes{ namespace max3d{
        Class_ID magma_from_space_node::max_impl::s_ClassID( 0x12b154f0, 0x697902fd );
        Class_ID magma_to_space_node::max_impl::s_ClassID( 0x6fc53fa7, 0x17ed0e87 );
        Class_ID magma_input_value_node::max_impl::s_ClassID( 0x16495b8e, 0xce470a3 );
        Class_ID magma_input_particles_node::max_impl::s_ClassID( 0x57c33580, 0x3b7c28e0 );
        Class_ID magma_input_object_node::max_impl::s_ClassID( 0x292513f1, 0x54223fc6 );
        Class_ID magma_curve_op_node::max_impl::s_ClassID( 0x6fca5a33, 0x68672cee );
        Class_ID magma_input_geometry_node::max_impl::s_ClassID( 0x511b111c, 0x76ac5e7a );
        Class_ID magma_texmap_node::max_impl::s_ClassID(0x151a7325, 0xadf25d1);
        Class_ID magma_script_op_node::max_impl::s_ClassID(0x66e47c84, 0x433d4d37);
} } } }
*/

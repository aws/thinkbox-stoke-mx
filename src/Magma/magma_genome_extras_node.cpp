// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/Magma/magma_genome_extras_node.hpp>

#include <frantic/magma/simple_compiler/base_compiler_impl.hpp>
#include <frantic/magma/simple_compiler/simple_mesh_compiler.hpp>

#include <frantic/magma/magma_node_base.hpp>
#include <frantic/magma/nodes/magma_node_impl.hpp>

namespace frantic {
namespace magma {
namespace nodes {
namespace max3d {

MAGMA_DEFINE_TYPE( "InputBoundBox", "Input", magma_genome_extras_node )
MAGMA_TYPE_ATTR( disableable, false )
MAGMA_DESCRIPTION( "Exposes information about the modifier instance" )
MAGMA_OUTPUT_NAMES( "Min", "Max", "Center" )
MAGMA_DEFINE_TYPE_END

int magma_genome_extras_node::get_num_outputs() const { return 3; }

void magma_genome_extras_node::compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler ) {
    using frantic::graphics::vector3f;

    vector3f pmin, pmax, center;
    compiler.get_context_data().get_property( _T("BoundsMin"), pmin );
    compiler.get_context_data().get_property( _T("BoundsMax"), pmax );
    compiler.get_context_data().get_property( _T("Center"), center );

    compiler.compile_constant( get_id(), pmin );
    compiler.compile_constant( get_id(), pmax );
    compiler.compile_constant( get_id(), center );
}

// MAGMA_DEFINE_TYPE( "FaceNeighbor", "Genome", magma_adjacent_faces_node )
//	MAGMA_TYPE_ATTR( disableable, false )
//	MAGMA_DESCRIPTION( "Finds a neighboring face in the mesh" )
//	MAGMA_INPUT( "Geometry", boost::blank() )
//	MAGMA_INPUT( "Object", 0 )
//	MAGMA_INPUT( "FaceIndex", 0 )
//	MAGMA_INPUT( "NeighborIndex", 0 )
// MAGMA_DEFINE_TYPE_END
//
// MAGMA_DEFINE_TYPE( "VertexNeighbor", "Genome", magma_adjacent_verts_node )
//	MAGMA_TYPE_ATTR( disableable, false )
//	MAGMA_DESCRIPTION( "Finds a neighboring vertex in the mesh" )
//	MAGMA_INPUT( "Geometry", boost::blank() )
//	MAGMA_INPUT( "Object", 0 )
//	MAGMA_INPUT( "VertexIndex", 0 )
//	MAGMA_INPUT( "NeighborIndex", 0 )
// MAGMA_DEFINE_TYPE_END
//
//
// MAGMA_DEFINE_TYPE( "VertexFaceNeighbor", "Genome", magma_adjacent_vert_faces_node )
//	MAGMA_TYPE_ATTR( disableable, false )
//	MAGMA_DESCRIPTION( "Finds a face using the vertex in the mesh" )
//	MAGMA_INPUT( "Geometry", boost::blank() )
//	MAGMA_INPUT( "Object", 0 )
//	MAGMA_INPUT( "VertexIndex", 0 )
//	MAGMA_INPUT( "NeighborIndex", 0 )
// MAGMA_DEFINE_TYPE_END
//
//
// namespace{
//	class neighbor_expression : public frantic::magma::simple_compiler::base_compiler::expression{
//		std::ptrdiff_t m_inPtrs[3], m_outPtr;
//		std::vector< frantic::magma::magma_geometry_ptr > m_geomList;
//
//	public:
//		neighbor_expression( frantic::magma::nodes::magma_input_geometry_interface& geom ){
//			geom.get_all_geometry( std::back_inserter( m_geomList ) );
//
//			//Force the adjacency information to be built in a non-threaded context.
//			for( std::vector< frantic::magma::magma_geometry_ptr >::iterator it = m_geomList.begin(), itEnd =
//m_geomList.end(); it != itEnd; ++it )
//				(*it)->get_mesh().get_face_adjacency();
//
//			/*for( std::size_t i = 0, iEnd = geom.size(); i < iEnd; ++i ){
//				frantic::magma::magma_geometry_ptr mesh = geom.get_geometry( i );
//				if( !mesh || !mesh->get_mesh().request_channel( _T("FaceNeighbors"), false, false ) )
//					throw magma_exception() << magma_exception::error_name( _T("Mesh #") +
//boost::lexical_cast<frantic::tstring>(i) + _T(" must support \"FaceNeighbors\" channel but doesn't") );
//
//				m_geomList.push_back( mesh->get_mesh().get_face_channels().get_channel( _T("FaceNeighbors") )
//); 				if( !m_geomList.back() || m_geomList.back()->get_data_type() != frantic::channels::data_type_int32 ||
//m_geomList.back()->get_channel_type() != frantic::geometry::mesh_channel::face ) 					THROW_MAGMA_INTERNAL_ERROR( i );
//			}*/
//		}
//
//		virtual void set_input( std::size_t inputIndex, std::ptrdiff_t relPtr ) { m_inPtrs[inputIndex] = relPtr;
//} 		virtual void set_output( std::ptrdiff_t relPtr ) { m_outPtr = relPtr; }
//
//		virtual const frantic::channels::channel_map& get_output_map() const { return
//frantic::magma::simple_compiler::traits<int>::get_static_map(); }
//
//		virtual void apply( frantic::magma::simple_compiler::base_compiler::state& data ) const {
//			std::size_t objIndex = static_cast<std::size_t>( data.get_temporary<int>( m_inPtrs[0] ) );
//			if( objIndex < m_geomList.size() ){
//				frantic::geometry::mesh_interface& theMesh = m_geomList[objIndex]->get_mesh();
//				frantic::geometry::face_adjacency_interface& faceAdj = theMesh.get_face_adjacency();
//
//				std::size_t faceIndex = static_cast<std::size_t>( data.get_temporary<int>( m_inPtrs[1] )
//);
//				//if( faceIndex < m_geomList[objIndex]->get_num_faces() ){
//				if( faceIndex < theMesh.get_num_faces() ){
//					std::size_t neighborIndex = static_cast<std::size_t>( data.get_temporary<int>(
//m_inPtrs[2] ) );
//					//if( neighborIndex < 3 ){
//					if( neighborIndex < faceAdj.get_num_edges(faceIndex) ){
//						//int vals[3];
//						//m_geomList[objIndex]->get_value( faceIndex, vals );
//
//						//if( static_cast<std::size_t>( vals[neighborIndex] ) <
//m_geomList[objIndex]->get_num_faces() ) 						std::size_t adjFace = faceAdj.get_edge_face(faceIndex, neighborIndex); 						if(
//adjFace < theMesh.get_num_faces() ) 							data.set_temporary( m_outPtr, adjFace ); 						else 							data.set_temporary( m_outPtr,
//(int)faceIndex ); 					}else 						data.set_temporary( m_outPtr, (int)faceIndex ); 				}else{ 					data.set_temporary( m_outPtr,
//(int)faceIndex );
//				}
//			}else{
//				data.set_temporary( m_outPtr, 0 );
//			}
//		}
//
//		virtual runtime_ptr get_runtime_ptr() const { return NULL; }
//	};
//
//	class vert_neighbor_expression : public frantic::magma::simple_compiler::base_compiler::expression{
//		std::ptrdiff_t m_inPtrs[3], m_outPtr;
//		std::vector< frantic::magma::magma_geometry_ptr > m_geomList;
//
//	public:
//		vert_neighbor_expression( frantic::magma::nodes::magma_input_geometry_interface& geom ){
//			geom.get_all_geometry( std::back_inserter( m_geomList ) );
//
//			//Force the adjacency information to be built in a non-threaded context.
//			for( std::vector< frantic::magma::magma_geometry_ptr >::iterator it = m_geomList.begin(), itEnd =
//m_geomList.end(); it != itEnd; ++it )
//				(*it)->get_mesh().get_vertex_adjacency();
//		}
//
//		virtual void set_input( std::size_t inputIndex, std::ptrdiff_t relPtr ) { m_inPtrs[inputIndex] = relPtr;
//} 		virtual void set_output( std::ptrdiff_t relPtr ) { m_outPtr = relPtr; }
//
//		virtual const frantic::channels::channel_map& get_output_map() const { return
//frantic::magma::simple_compiler::traits<int>::get_static_map(); }
//
//		virtual void apply( frantic::magma::simple_compiler::base_compiler::state& data ) const {
//			std::size_t objIndex = static_cast<std::size_t>( data.get_temporary<int>( m_inPtrs[0] ) );
//			if( objIndex < m_geomList.size() ){
//				frantic::geometry::mesh_interface& theMesh = m_geomList[objIndex]->get_mesh();
//				frantic::geometry::vertex_adjacency_interface& vertAdj = theMesh.get_vertex_adjacency();
//
//				std::size_t vertIndex = static_cast<std::size_t>( data.get_temporary<int>( m_inPtrs[1] )
//); 				if( vertIndex < theMesh.get_num_verts() ){ 					std::size_t neighborIndex = static_cast<std::size_t>(
//data.get_temporary<int>( m_inPtrs[2] ) ); 					if( neighborIndex < vertAdj.get_num_edges(vertIndex) ){ 						bool isVisible =
//false; 						std::size_t adjVert = vertAdj.get_edge_vert(vertIndex, neighborIndex, isVisible); 						if( adjVert <
//theMesh.get_num_verts() ) //Make sure the result makes sense 							data.set_temporary( m_outPtr, adjVert ); 						else
//							data.set_temporary( m_outPtr, (int)vertIndex );
//					}else
//						data.set_temporary( m_outPtr, (int)vertIndex );
//				}else{
//					data.set_temporary( m_outPtr, (int)vertIndex );
//				}
//			}else{
//				data.set_temporary( m_outPtr, 0 );
//			}
//		}
//
//		virtual runtime_ptr get_runtime_ptr() const { return NULL; }
//	};
//
//	class vert_face_neighbor_expression : public frantic::magma::simple_compiler::base_compiler::expression{
//		std::ptrdiff_t m_inPtrs[3], m_outPtr;
//		std::vector< frantic::magma::magma_geometry_ptr > m_geomList;
//
//	public:
//		vert_face_neighbor_expression( frantic::magma::nodes::magma_input_geometry_interface& geom ){
//			geom.get_all_geometry( std::back_inserter( m_geomList ) );
//
//			//Force the adjacency information to be built in a non-threaded context.
//			for( std::vector< frantic::magma::magma_geometry_ptr >::iterator it = m_geomList.begin(), itEnd =
//m_geomList.end(); it != itEnd; ++it )
//				(*it)->get_mesh().get_vertex_adjacency();
//		}
//
//		virtual void set_input( std::size_t inputIndex, std::ptrdiff_t relPtr ) { m_inPtrs[inputIndex] = relPtr;
//} 		virtual void set_output( std::ptrdiff_t relPtr ) { m_outPtr = relPtr; }
//
//		virtual const frantic::channels::channel_map& get_output_map() const { return
//frantic::magma::simple_compiler::traits<int>::get_static_map(); }
//
//		virtual void apply( frantic::magma::simple_compiler::base_compiler::state& data ) const {
//			std::size_t objIndex = static_cast<std::size_t>( data.get_temporary<int>( m_inPtrs[0] ) );
//			if( objIndex < m_geomList.size() ){
//				frantic::geometry::mesh_interface& theMesh = m_geomList[objIndex]->get_mesh();
//				frantic::geometry::vertex_adjacency_interface& vertAdj = theMesh.get_vertex_adjacency();
//
//				std::size_t vertIndex = static_cast<std::size_t>( data.get_temporary<int>( m_inPtrs[1] )
//); 				if( vertIndex < theMesh.get_num_verts() ){ 					std::size_t neighborIndex = static_cast<std::size_t>(
//data.get_temporary<int>( m_inPtrs[2] ) ); 					if( neighborIndex < vertAdj.get_num_edges(vertIndex) ){ 						std::size_t adjFace
//= vertAdj.get_edge_face(vertIndex, neighborIndex); 						if( adjFace < theMesh.get_num_faces() ) //Make sure the result
//makes sense 							data.set_temporary( m_outPtr, adjFace ); 						else 							data.set_temporary( m_outPtr, -1 ); 					}else
//						data.set_temporary( m_outPtr, -1 );
//				}else{
//					data.set_temporary( m_outPtr, -1 );
//				}
//			}else{
//				data.set_temporary( m_outPtr, -1 );
//			}
//		}
//
//		virtual runtime_ptr get_runtime_ptr() const { return NULL; }
//	};
// }
//
// void magma_adjacent_faces_node::compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler ){
//	using frantic::magma::nodes::magma_input_geometry_interface;
//
//	magma_input_geometry_interface* geom = compiler.get_geometry_interface( this->get_input( 0 ).first,
//this->get_input( 0 ).second ); 	if( !geom ) 		THROW_MAGMA_INTERNAL_ERROR( this->get_id() );
//
//	std::pair<frantic::magma::magma_interface::magma_id,int> inputs[] = { compiler.get_node_input(*this,1),
//compiler.get_node_input(*this,2), compiler.get_node_input(*this,3) }; 	magma_data_type inputTypes[] = {
//frantic::magma::simple_compiler::traits<int>::get_type(), frantic::magma::simple_compiler::traits<int>::get_type(),
//frantic::magma::simple_compiler::traits<int>::get_type() };
//
//	if( frantic::magma::simple_compiler::base_compiler* bc =
//dynamic_cast<frantic::magma::simple_compiler::base_compiler*>( &compiler ) ){
//		std::unique_ptr<frantic::magma::simple_compiler::base_compiler::expression> expr( new neighbor_expression(
//*geom ) );
//
//		std::vector< std::pair<frantic::magma::magma_interface::magma_id,int> > inputVec( inputs, inputs + 3 );
//		std::vector< magma_data_type > inputTypeVec( inputTypes, inputTypes + 3 );
//
//		bc->compile_expression( expr, this->get_id(), inputVec, inputTypeVec );
//	}else{
//		magma_node_base::compile_as_extension_type( compiler );
//	}
// }
//
// void magma_adjacent_verts_node::compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler ){
//	using frantic::magma::nodes::magma_input_geometry_interface;
//
//	magma_input_geometry_interface* geom = compiler.get_geometry_interface( this->get_input( 0 ).first,
//this->get_input( 0 ).second ); 	if( !geom ) 		THROW_MAGMA_INTERNAL_ERROR( this->get_id() );
//
//	std::pair<frantic::magma::magma_interface::magma_id,int> inputs[] = { compiler.get_node_input(*this,1),
//compiler.get_node_input(*this,2), compiler.get_node_input(*this,3) }; 	magma_data_type inputTypes[] = {
//frantic::magma::simple_compiler::traits<int>::get_type(), frantic::magma::simple_compiler::traits<int>::get_type(),
//frantic::magma::simple_compiler::traits<int>::get_type() };
//
//	if( frantic::magma::simple_compiler::base_compiler* bc =
//dynamic_cast<frantic::magma::simple_compiler::base_compiler*>( &compiler ) ){
//		std::unique_ptr<frantic::magma::simple_compiler::base_compiler::expression> expr( new
//vert_neighbor_expression( *geom ) );
//
//		std::vector< std::pair<frantic::magma::magma_interface::magma_id,int> > inputVec( inputs, inputs + 3 );
//		std::vector< magma_data_type > inputTypeVec( inputTypes, inputTypes + 3 );
//
//		bc->compile_expression( expr, this->get_id(), inputVec, inputTypeVec );
//	}else{
//		magma_node_base::compile_as_extension_type( compiler );
//	}
// }
//
// void magma_adjacent_vert_faces_node::compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler ){
//	using frantic::magma::nodes::magma_input_geometry_interface;
//
//	magma_input_geometry_interface* geom = compiler.get_geometry_interface( this->get_input( 0 ).first,
//this->get_input( 0 ).second ); 	if( !geom ) 		THROW_MAGMA_INTERNAL_ERROR( this->get_id() );
//
//	std::pair<frantic::magma::magma_interface::magma_id,int> inputs[] = { compiler.get_node_input(*this,1),
//compiler.get_node_input(*this,2), compiler.get_node_input(*this,3) }; 	magma_data_type inputTypes[] = {
//frantic::magma::simple_compiler::traits<int>::get_type(), frantic::magma::simple_compiler::traits<int>::get_type(),
//frantic::magma::simple_compiler::traits<int>::get_type() };
//
//	if( frantic::magma::simple_compiler::base_compiler* bc =
//dynamic_cast<frantic::magma::simple_compiler::base_compiler*>( &compiler ) ){
//		std::unique_ptr<frantic::magma::simple_compiler::base_compiler::expression> expr( new
//vert_face_neighbor_expression( *geom ) );
//
//		std::vector< std::pair<frantic::magma::magma_interface::magma_id,int> > inputVec( inputs, inputs + 3 );
//		std::vector< magma_data_type > inputTypeVec( inputTypes, inputTypes + 3 );
//
//		bc->compile_expression( expr, this->get_id(), inputVec, inputTypeVec );
//	}else{
//		magma_node_base::compile_as_extension_type( compiler );
//	}
// }
//
// MAGMA_DEFINE_TYPE( "InputDerivative", "Input", magma_input_derivative_node )
//	MAGMA_TYPE_ATTR( disableable, false )
//	MAGMA_DESCRIPTION( "Calculates the derivative of a quantity relative to a UV parameterization of the face." )
//	MAGMA_EXPOSE_PROPERTY( dataChannelName, frantic::tstring );
//	MAGMA_EXPOSE_PROPERTY( uvChannelName, frantic::tstring );
//	MAGMA_OUTPUT_NAMES( "d/du", "d/dv" )
// MAGMA_DEFINE_TYPE_END
//
// int magma_input_derivative_node::get_num_outputs() const {
//	return 2;
// }

namespace {
// template <class T>
// inline bool compute_duv(const T (&tri)[3], const float (&uvs)[3][2], T& outDu, T& outDv){
//	T e1 = tri[1] - tri[0];
//	T e2 = tri[2] - tri[0];

//	float u1 = uvs[1][0] - uvs[0][0];
//	float u2 = uvs[2][0] - uvs[0][0];
//	float v1 = uvs[1][1] - uvs[0][1];
//	float v2 = uvs[2][1] - uvs[0][1];

//	float determinant = u1*v2 - u2*v1;
//	if( std::abs(determinant) > 1e-5f ){ //Cannot divide by 0 determinant.
//		outDu = (v2*e1-v1*e2)/determinant;
//		outDv = (u1*e2-u2*e1)/determinant;
//		return true;
//	}else{
//		return false;
//	}
//}

// template <class T>
// struct derivative_op : simple_compiler::detail::magma_opaque{
//	const frantic::geometry::mesh_channel* m_inputChannel;
//	const frantic::geometry::mesh_channel* m_uvChannel;

//	int m_outValueDuOffset, m_outValueDvOffset;

//	inline void apply( char* stack, std::size_t faceIndex ) const {
//		T data[3];
//		float uvws[3][3];
//		float uvs[3][2];

//		for( std::size_t i = 0; i < 3; ++i ){
//			m_inputChannel->get_value( m_inputChannel->get_fv_index(faceIndex, i), &data[i] );
//			m_uvChannel->get_value( m_uvChannel->get_fv_index(faceIndex, i), uvws[i] );
//
//			//Discard the W component of the UVWs
//			uvs[i][0] = uvws[i][0];
//			uvs[i][1] = uvws[i][1];
//		}

//		T* outDU = reinterpret_cast<T*>( stack + m_outValueDuOffset );
//		T* outDV = reinterpret_cast<T*>( stack + m_outValueDvOffset );

//		if( !compute_duv( data, uvs, *outDU, *outDV ) ){
//			memset( outDU, 0, sizeof(T) );
//			memset( outDV, 0, sizeof(T) );
//		}
//	}

//	void set_input_channel( const frantic::geometry::mesh_channel* inputChannel ){
//		m_inputChannel = inputChannel;

//		if( m_inputChannel->get_data_arity() != frantic::channels::channel_data_type_traits<T>::arity() ||
//m_inputChannel->get_data_type() != frantic::channels::channel_data_type_traits<T>::data_type() )
//			THROW_MAGMA_INTERNAL_ERROR();
//	}

//	void set_uv_channel( const frantic::geometry::mesh_channel* uvChannel ){
//		m_uvChannel = uvChannel;

//		if( m_uvChannel->get_data_arity() != 3 || m_uvChannel->get_data_type() !=
//frantic::channels::data_type_float32 ){ 			magma_data_type foundType, expectedType; 			foundType.m_elementType =
//m_uvChannel->get_data_type(); 			foundType.m_elementCount = m_uvChannel->get_data_arity(); 			expectedType.m_elementType =
//frantic::channels::data_type_float32; 			expectedType.m_elementCount = 3;
//
//			throw magma_exception()
//				<< magma_exception::input_index( 1 )
//				<< magma_exception::found_type( foundType )
//				<< magma_exception::expected_type( expectedType )
//				<< magma_exception::error_name( _T("Incorrect data type") );
//		}
//	}

//	void bind_outValueDu( const simple_compiler::detail::temporary_result& output ){
//		if( output.dataType != simple_compiler::traits<T>::get_type() )
//			THROW_MAGMA_INTERNAL_ERROR();
//
//		this->m_outValueDuOffset = output.offset;
//	}

//	void bind_outValueDv( const simple_compiler::detail::temporary_result& output ){
//		if( output.dataType != simple_compiler::traits<T>::get_type() )
//			THROW_MAGMA_INTERNAL_ERROR();

//		this->m_outValueDvOffset = output.offset;
//	}

//	static void execute( char* stack, void* compilerData, simple_compiler::detail::magma_opaque* opaqueSelf ){
//		derivative_op& self = *static_cast< derivative_op* >( opaqueSelf );
//		simple_compiler::simple_mesh_compiler::mesh_data& meshData = *reinterpret_cast<
//simple_compiler::simple_mesh_compiler::mesh_data* >( compilerData );
//
//		self.apply( stack, (std::size_t)meshData.faceIndex );
//	}
//};
} // namespace

// TODO: Move into simple_mesh_compiler (no need to be Genome specific)
// void magma_input_derivative_node::compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler ){
/*if( frantic::magma::simple_compiler::simple_mesh_compiler* sc =
dynamic_cast<frantic::magma::simple_compiler::simple_mesh_compiler*>( &compiler ) ){ using
frantic::magma::simple_compiler::simple_mesh_compiler;

        try{
                if( sc->get_iteration_pattern() == simple_mesh_compiler::VERTEX )
                        throw magma_exception() << magma_exception::error_name( _T("InputDerivative not available when
iterating over vertices") );

                sc->get_mesh_interface().request_channel( get_dataChannelName(), true, false, false );
                sc->get_mesh_interface().request_channel( get_uvChannelName(), true, false, false );

                const frantic::geometry::mesh_channel* dataChannel =
sc->get_mesh_interface().get_vertex_channels().get_channel( get_dataChannelName() ); const
frantic::geometry::mesh_channel* uvChannel = sc->get_mesh_interface().get_vertex_channels().get_channel(
get_uvChannelName() );

                if( !dataChannel )
                        throw magma_exception() << magma_exception::property_name( _T("dataChannelName") ) <<
magma_exception::error_name( _T("Channel \"") + get_dataChannelName() + _T("\" not available in this Geometry Object")
); if( !uvChannel ) throw magma_exception() << magma_exception::property_name( _T("uvChannelName") ) <<
magma_exception::error_name( _T("Channel \"") + get_uvChannelName() + _T("\" not available in this Geometry Object") );

                magma_data_type dataType;
                dataType.m_elementType = dataChannel->get_data_type();
                dataType.m_elementCount = dataChannel->get_data_arity();

                simple_compiler::detail::temporary_result outputs[2];
                outputs[0] = *sc->allocate_temporary( this->get_id(), dataType );
                outputs[1] = *sc->allocate_temporary( this->get_id(), dataType );

                simple_compiler::detail::code_segment result;

                if( *magma_singleton::get_named_data_type( _T("Float") ) == dataType ){
                        std::unique_ptr< derivative_op<float> > resultData( new derivative_op<float> );

                        resultData->set_input_channel( dataChannel );
                        resultData->set_uv_channel( uvChannel );
                        resultData->bind_outValueDu( outputs[0] );
                        resultData->bind_outValueDv( outputs[1] );

                        result.code = &derivative_op<float>::execute;
                        result.data = resultData.release();
                }else if( *magma_singleton::get_named_data_type( _T("Vec3") ) == dataType ){
                        using frantic::graphics::vector3f;

                        std::unique_ptr< derivative_op<vector3f> > resultData( new derivative_op<vector3f> );

                        resultData->set_input_channel( dataChannel );
                        resultData->set_uv_channel( uvChannel );
                        resultData->bind_outValueDu( outputs[0] );
                        resultData->bind_outValueDv( outputs[1] );

                        result.code = &derivative_op<vector3f>::execute;
                        result.data = resultData.release();
                }else{
                        throw magma_exception() << magma_exception::property_name( _T("dataChannelName") ) <<
magma_exception::found_type(dataType) << magma_exception::error_name( _T("Channel \"") + get_uvChannelName() + _T("\"
was expected to be a float32[1] or float32[3].") );
                }

                sc->append_code_segment( result );
        }catch( magma_exception& e ){
                e << magma_exception::node_id( this->get_id() );
                throw;
        }
}else{*/
//		magma_node_base::compile_as_extension_type( compiler );
//}
//}

} // namespace max3d
} // namespace nodes
} // namespace magma
} // namespace frantic

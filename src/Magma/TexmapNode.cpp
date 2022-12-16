// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"
#include <ember/ember_compiler.hpp>
#include <stoke/max3d/Magma/TexmapNode.hpp>

#include <frantic/magma/max3d/magma_max_node_impl.hpp>
#include <frantic/max3d/convert.hpp>
#include <frantic/max3d/shaders/map_query.hpp>

#include <boost/thread/tss.hpp>

using namespace frantic::magma;

namespace ember {
namespace max3d {

// MAGMA_DEFINE_MAX_TYPE( "TexmapEval", "Object", TexmapNode )
//	MAGMA_EXPOSE_PROPERTY( texmap, Texmap* )
//	MAGMA_EXPOSE_ARRAY_PROPERTY( mapChannels, int )
//	MAGMA_ENUM_PROPERTY( resultType, "Color", "Mono", "NormalPerturb" )
//	MAGMA_INPUT( "Lookup Point (OS)", frantic::graphics::vector3f(0) )
//	MAGMA_INPUT( "VertexColor", frantic::graphics::vector3f(0) )
//	MAGMA_INPUT( "TextureCoord", frantic::graphics::vector3f(0) )
//	MAGMA_INPUT( "Normal (OS)", frantic::graphics::vector3f(0) )
//	MAGMA_INPUT( "MtlID", 0 )
//	MAGMA_INPUT( "IOR", 1.f )
//	MAGMA_OUTPUT_NAMES( "Result" )
//	MAGMA_DESCRIPTION( "Evaluates a 3D texmap at a point in space" )
// MAGMA_DEFINE_TYPE_END

// namespace{
//	template < class T1, class T2, class Cmp = std::less<T1> >
//	struct cmp_first : std::binary_function< std::pair<T1,T2>, std::pair<T1,T2>, bool > {
//		Cmp m_cmp;
//
//		cmp_first()
//		{}
//
//		bool operator()( const std::pair<T1,T2>& lhs, const std::pair<T1,T2>& rhs ){
//			return m_cmp( lhs.first, rhs.first );
//		}
//
//		bool operator()( const std::pair<T1,T2>& lhs, T1 rhs ){
//			return m_cmp( lhs.first, rhs );
//		}
//
//		bool operator()( T1 lhs, const std::pair<T1,T2>& rhs ){
//			return m_cmp( lhs, rhs.first );
//		}
//	};
//
//	inline Point3 VectorTransposeTransform( const Point3& p, const Matrix3& m ){
//		return Point3( DotProd( p, m[0] ), DotProd( p, m[1] ), DotProd( p, m[2] ) );
//	}
//
//	//Shared data across threads goes here.
//	/*struct ShadeContextData{
//		Matrix3 m_toWorld, m_toObject, m_fromWorld, m_fromObject;
//	};*/
//
//	class MyShadeContext : public ShadeContext{
//	public:
//		Point3 m_pos, m_view, m_origView, m_normal, m_origNormal;
//		float m_ior;
//
//	private:
//		std::vector< std::pair<int, Point3> > m_mapChannels;
//
//		Matrix3 m_toWorld, m_toObject, m_fromWorld, m_fromObject;
//
//		RenderInstance* m_renderInstance;
//		INode* m_node; //Only used if m_renderInstance == NULL;
//
//	public:
//		MyShadeContext(){
//			/*mode = SCMODE_NORMAL; nLights = 0; shadow = TRUE;  rayLevel = 0;
//			globContext = NULL; atmosSkipLight = NULL;*/
//
//			this->doMaps = TRUE;
//			this->filterMaps = TRUE;
//			this->backFace = FALSE;
//			this->mtlNum = 0;
//			this->ambientLight.White();
//			this->xshadeID = 0;
//
//			m_ior = 1.f;
//			m_renderInstance = NULL;
//			m_node = NULL;
//		}
//
//		void set_render_global_context( RenderGlobalContext* globContext ){
//			this->globContext = globContext;
//
//			m_toWorld = globContext->camToWorld;
//			m_fromWorld = globContext->worldToCam;
//		}
//
//		/*void set_render_instance( RenderInstance* inst, INode* node ){
//			m_renderInstance = inst;
//			m_node = node;
//
//			if( inst ){
//				this->nLights = m_renderInstance->NumLights();
//
//				m_fromObject = m_renderInstance->objToCam;
//				m_toObject = m_renderInstance->camToObj;
//			}else if( node ){
//				Matrix3 nodeTM = node->GetNodeTM( globContext->time );
//
//				m_fromObject = nodeTM * m_fromWorld;
//				m_toObject = m_toWorld * Inverse( nodeTM );
//			}else{
//				m_fromObject.IdentityMatrix();
//				m_toObject.IdentityMatrix();
//			}
//		}*/
//		void set_render_instance( RenderInstance* inst, INode* node, const Matrix3& nodeTM ){
//			m_renderInstance = inst;
//			m_node = node;
//
//			if( inst ){
//				this->nLights = m_renderInstance->NumLights();
//
//				m_fromObject = m_renderInstance->objToCam;
//				m_toObject = m_renderInstance->camToObj;
//			}else{
//				m_fromObject = nodeTM * m_fromWorld;
//				m_toObject = m_toWorld * Inverse( nodeTM );
//			}
//		}
//
//		std::vector< std::pair<int,Point3> >& get_map_channels(){
//			return m_mapChannels;
//		}
//
//	#pragma warning( push )
//	#pragma warning( disable : 4100 )
//		virtual BOOL InMtlEditor(){
//			return false;
//		}
//
//		//virtual int ProjType() { return 0; }
//
//		virtual LightDesc* Light(int n){
//			return m_renderInstance ? m_renderInstance->Light(n) : NULL;
//		}
//
//		virtual TimeValue CurTime(){
//			return globContext->time;
//		}
//
//		//I used to return m_node->GetRenderID(), but that value is set by the renderer and it usually is bunk.
//The Raytrace texmap
//		//seems to this as an index into RenderGlobalContext::GetRenderInstance() which causes a crash when it
//doesn't actually
//		//correspond.
//		virtual int NodeID() { return m_renderInstance ? m_renderInstance->nodeID : -1; }
//
//		virtual INode *Node() { return m_renderInstance ? m_renderInstance->GetINode() : m_node; }
//
//		virtual Object *GetEvalObject() { return m_renderInstance ? m_renderInstance->GetEvalObject() : NULL; }
//
//		//virtual Point3 BarycentricCoords() { return Point3(0,0,0);}
//
//		virtual int FaceNumber(){ return 0; }
//
//		virtual Point3 Normal(){
//			return m_normal;
//		}
//
//		virtual void SetNormal(Point3 p){
//			m_normal = p;
//		}
//
//		virtual Point3 OrigNormal(){
//			return m_origNormal;
//		}
//
//		virtual Point3 GNormal(){
//			return m_normal;
//		}
//
//		//virtual float  Curve() { return 0.0f; }
//
//		virtual Point3 V(){
//			return m_view;
//		}
//
//		virtual void SetView(Point3 p){
//			m_view = p;
//		}
//
//		virtual Point3 OrigView() {
//			return m_origView;
//		}
//
//		virtual  Point3 ReflectVector(){
//			return m_view - 2 * DotProd( m_view, m_normal ) * m_normal;
//		}
//
//		virtual  Point3 RefractVector(float ior){
//			//Taken from Max SDK cjrender sample.
//			float VN,nur,k;
//			VN = DotProd(-m_view,m_normal);
//			if (backFace) nur = ior;
//			else nur = (ior!=0.0f) ? 1.0f/ior: 1.0f;
//			k = 1.0f-nur*nur*(1.0f-VN*VN);
//			if (k<=0.0f) {
//				// Total internal reflection:
//				return ReflectVector();
//			} else {
//				return (nur*VN-(float)sqrt(k))*m_normal + nur*m_view;
//			}
//		}
//
//		virtual void SetIOR(float ior){
//			m_ior = ior;
//		}
//
//		virtual float GetIOR(){
//			return m_ior;
//		}
//
//		virtual Point3 CamPos(){
//			return Point3(0,0,0);
//		}
//
//		virtual Point3 P(){
//			return m_pos;
//		}
//
//		virtual Point3 DP(){
//			return Point3(0,0,0);
//		}
//
//		//virtual void DP(Point3& dpdx, Point3& dpdy){}
//
//		virtual Point3 PObj(){
//			return this->PointTo( P(), REF_OBJECT );
//		}
//
//		virtual Point3 DPObj(){
//			return Point3(0,0,0);
//		}
//
//		virtual Box3 ObjectBox(){
//			return m_renderInstance ? m_renderInstance->obBox : Box3();
//		}
//
//		virtual Point3 PObjRelBox(){
//			if( m_renderInstance ){
//				Point3 pRel = (PObj() - m_renderInstance->obBox.pmin);
//				pRel.x /= (m_renderInstance->obBox.pmax.x - m_renderInstance->obBox.pmin.x);
//				pRel.x = 2.f * (pRel.x - 0.5f);
//				pRel.y /= (m_renderInstance->obBox.pmax.y - m_renderInstance->obBox.pmin.y);
//				pRel.y = 2.f * (pRel.y - 0.5f);
//				pRel.z /= (m_renderInstance->obBox.pmax.z - m_renderInstance->obBox.pmin.z);
//				pRel.z = 2.f * (pRel.z - 0.5f);
//				return pRel;
//			}else{
//				return Point3(0,0,0);
//			}
//		}
//
//		virtual Point3 DPObjRelBox(){
//			return Point3(0,0,0);
//		}
//
//		virtual void ScreenUV(Point2& uv, Point2 &duv)
//		{}
//
//		virtual IPoint2 ScreenCoord(){
//			return IPoint2(0,0);
//		}
//
//		//virtual Point2 SurfacePtScreen(){ return Point2(0.0,0.0); }
//
//		virtual Point3 UVW(int channel=0){
//			std::vector< std::pair<int,Point3> >::const_iterator it = std::lower_bound( m_mapChannels.begin(),
//m_mapChannels.end(), channel, cmp_first<int,Point3>() ); 			if( it != m_mapChannels.end() && it->first == channel )
//				return it->second;
//			return Point3(0,0,0);
//		}
//
//		virtual Point3 DUVW(int channel=0){
//			return Point3(0,0,0);
//		}
//
//		virtual void DPdUVW(Point3 dP[3],int channel=0)
//		{}
//
//		//virtual int BumpBasisVectors(Point3 dP[2], int axis, int channel=0) { return 0; }
//
//		//virtual BOOL IsSuperSampleOn(){ return FALSE; }
//		//virtual BOOL IsTextureSuperSampleOn(){ return FALSE; }
//		//virtual int GetNSuperSample(){ return 0; }
//		//virtual float GetSampleSizeScale(){ return 1.0f; }
//
//		//virtual Point3 UVWNormal(int channel=0) { return Point3(0,0,1); }
//
//		//virtual float RayDiam() { return Length(DP()); }
//
//		//virtual float RayConeAngle() { return 0.0f; }
//
//		//virtual AColor EvalEnvironMap(Texmap *map, Point3 view);
//
//		virtual void GetBGColor(Color &bgcol, Color& transp, BOOL fogBG=TRUE)
//		{}
//
//		//virtual float CamNearRange() {return 0.0f;}
//
//		//virtual float CamFarRange() {return 0.0f;}
//
//		virtual Point3 PointTo(const Point3& p, RefFrame ito){
//			switch(ito){
//			case REF_WORLD:
//				return m_toWorld.PointTransform( p );
//			case REF_OBJECT:
//				return m_toObject.PointTransform( p );
//			default:
//			case REF_CAMERA:
//				return p;
//			}
//		}
//
//		virtual Point3 PointFrom(const Point3& p, RefFrame ifrom){
//			switch(ifrom){
//			case REF_WORLD:
//				return m_fromWorld.PointTransform( p );
//			case REF_OBJECT:
//				return m_fromObject.PointTransform( p );
//			default:
//			case REF_CAMERA:
//				return p;
//			}
//		}
//
//		virtual Point3 VectorTo(const Point3& p, RefFrame ito){
//			switch(ito){
//			case REF_WORLD:
//				return m_toWorld.VectorTransform( p );
//			case REF_OBJECT:
//				return m_toObject.VectorTransform( p );
//			default:
//			case REF_CAMERA:
//				return p;
//			}
//		}
//
//		virtual Point3 VectorFrom(const Point3& p, RefFrame ifrom){
//			switch(ifrom){
//			case REF_WORLD:
//				return m_fromWorld.VectorTransform( p );
//			case REF_OBJECT:
//				return m_fromObject.VectorTransform( p );
//			default:
//			case REF_CAMERA:
//				return p;
//			}
//		}
//
//		virtual Point3 VectorToNoScale(const Point3& p, RefFrame ito){
//			switch(ito){
//			case REF_WORLD:
//				return VectorTransposeTransform( p, m_fromWorld ); //Multiply by the transpose of the
//inverse 			case REF_OBJECT: 				return VectorTransposeTransform( p, m_fromObject ); //Multiply by the transpose of the
//inverse 			default: 			case REF_CAMERA: 				return p;
//			}
//		}
//
//		virtual Point3 VectorFromNoScale(const Point3& p, RefFrame ifrom){
//			switch(ifrom){
//			case REF_WORLD:
//				return VectorTransposeTransform( p, m_toWorld ); //Multiply by the transpose of the
//inverse 			case REF_OBJECT: 				return VectorTransposeTransform( p, m_toObject ); //Multiply by the transpose of the inverse
//			default:
//			case REF_CAMERA:
//				return p;
//			}
//		}
//	#pragma warning( pop )
//	};
//
//	class texmap_expression : public subexpression{
//	public:
//		struct result_type{
//			enum enum_t{
//				color, mono, perturb, error
//			};
//
//			static enum_t from_string( const frantic::tstring& val ){
//				if( val == _T("Color") )
//					return color;
//				else if( val == _T("Mono") )
//					return mono;
//				else if( val == _T("NormalPerturb") )
//					return perturb;
//				else
//					return error;
//			}
//		};
//
//	private:
//		std::ptrdiff_t m_inputs[6];
//		std::vector< std::pair<int,std::ptrdiff_t> > m_mapChannels;
//		std::vector< int > m_inputMapping;
//
//		std::ptrdiff_t m_output;
//
//		void bind_map_channel( int n, std::ptrdiff_t relPtr ){
//			if( n < MAX_MESHMAPS && n >= -NUM_HIDDENMAPS ){
//				std::vector< std::pair<int,std::ptrdiff_t> >::iterator it = std::lower_bound(
//m_mapChannels.begin(), m_mapChannels.end(), n, cmp_first<int,std::ptrdiff_t>() );
//
//				if( it != m_mapChannels.end() && it->first == n ){
//					it->second = relPtr;
//				}else{
//					m_mapChannels.insert( it, std::make_pair(n, relPtr) );
//				}
//			}
//		}
//
//		void bind_position( std::ptrdiff_t relPtr ){
//			m_inputs[0] = relPtr;
//		}
//
//		void bind_vertexColor( std::ptrdiff_t relPtr ){
//			bind_map_channel( 0, relPtr );
//		}
//
//		void bind_textureCoord( std::ptrdiff_t relPtr ){
//			bind_map_channel( 1, relPtr );
//		}
//
//		void bind_normal( std::ptrdiff_t relPtr ){
//			m_inputs[3] = relPtr;
//		}
//
//		void bind_mtlID( std::ptrdiff_t relPtr ){
//			m_inputs[4] = relPtr;
//		}
//
//		void bind_ior( std::ptrdiff_t relPtr ){
//			m_inputs[5] = relPtr;
//		}
//
//		const vec3& get_position( char* rel ) const {
//			return *reinterpret_cast<vec3*>( rel + m_inputs[0] );
//		}
//
//		const vec3& get_normal( char* rel ) const {
//			return *reinterpret_cast<vec3*>( rel + m_inputs[3] );
//		}
//
//		int get_mtlID( char* rel ) const {
//			return *reinterpret_cast<int*>( rel + m_inputs[4] );
//		}
//
//		float get_ior( char* rel ) const {
//			return *reinterpret_cast<float*>( rel + m_inputs[5] );
//		}
//
//		void set_result( char* rel, float val ) const {
//			*reinterpret_cast<float*>( rel + m_output ) = val;
//		}
//
//		void set_result( char* rel, const vec3& val ) const {
//			*reinterpret_cast<vec3*>( rel + m_output ) = val;
//		}
//
//		result_type::enum_t resultType;
//
//		mutable boost::thread_specific_ptr<MyShadeContext> scPtr;
//
//		Texmap* texmap;
//		RenderGlobalContext* globContext;
//		RenderInstance* rendInst;
//		INode* node;
//
//		Matrix3 toCamera, toCameraInv, nodeTM;
//
//	private:
//		inline void get_map_channels( char* stack, MyShadeContext& sc ) const {
//			std::vector< std::pair<int,Point3> >& uvws = sc.get_map_channels();
//
//			if( uvws.empty() ){
//				for( std::vector< std::pair<int,std::ptrdiff_t> >::const_iterator it = m_mapChannels.begin(),
//itEnd = m_mapChannels.end(); it != itEnd; ++it ) 					uvws.push_back( std::make_pair( it->first, frantic::max3d::to_max_t(
//*reinterpret_cast<vec3*>( stack + it->second ) ) ) ); 			}else{ 				assert( uvws.size() == m_mapChannels.size() );
//
//				std::vector< std::pair<int,Point3> >::iterator uvwIt = uvws.begin();
//
//				for( std::vector< std::pair<int,ptrdiff_t> >::const_iterator it = m_mapChannels.begin(), itEnd
//= m_mapChannels.end(); it != itEnd; ++it, ++uvwIt ){ 					assert( uvwIt->first == it->first );
//
//					uvwIt->second = frantic::max3d::to_max_t( *reinterpret_cast<vec3*>( stack + it->second )
//);
//				}
//			}
//		}
//
//		/*inline void get_map_channels( char* stack, Point3 outVals[] ){
//
//		}*/
//
//		inline void apply( char* stack ) const {
//			//MyShadeContext* sc = scPtr.get();
//			//if( !sc ){
//			//	scPtr.reset( new MyShadeContext );
//			//
//			//	sc = scPtr.get();
//			//	sc->set_render_global_context( globContext );
//			//	//sc->set_render_instance( rendInst, node );
//			//	sc->set_render_instance( rendInst, node, nodeTM );
//			//}else{
//			//	sc->TossCache( texmap );
//			//}
//
//			//Using a boost::thread_specific_ptr was not working! Curses!
//			MyShadeContext sc;
//
//			sc.set_render_global_context( globContext );
//			sc.set_render_instance( rendInst, node, nodeTM );
//
//			sc.m_pos = toCamera.PointTransform( frantic::max3d::to_max_t( get_position( stack ) ) );
//			sc.m_normal = Normalize( VectorTransposeTransform( frantic::max3d::to_max_t( get_normal( stack ) ),
//toCameraInv ) ); 			sc.mtlNum = get_mtlID( stack ); 			sc.m_ior = get_ior( stack );
//
//			//TODO: Consider allowing this to be overriden.
//			sc.m_view = (globContext->projType == PROJ_PERSPECTIVE) ? Normalize( sc.m_pos ) :
//Point3(0.f,0.f,-1.f);
//
//			sc.m_origView = sc.m_view;
//			sc.m_origNormal = sc.m_normal;
//
//			get_map_channels( stack, sc );
//
//			switch( resultType ){
//			case result_type::color:
//				{
//					AColor result = texmap->EvalColor( sc );
//					set_result( stack, vec3(result.r, result.g, result.b) );
//				}
//				break;
//			case result_type::mono:
//				{
//					float result = texmap->EvalMono( sc );
//					set_result( stack, result );
//				}
//				break;
//			case result_type::perturb:
//				{
//					Point3 result = texmap->EvalNormalPerturb( sc );
//					set_result( stack, vec3(result.x, result.y, result.z) );
//				}
//				break;
//			default:
//				__assume(0); //MS specific optimization that says there is no need to check for other
//cases.
//			}
//		}
//
//	public:
//		texmap_expression()
//			: texmap(NULL), globContext(NULL), rendInst(NULL), node(NULL), nodeTM(TRUE), toCamera(TRUE),
//toCameraInv(TRUE), resultType( result_type::color )
//		{}
//
//		void set_texmap( Texmap* texmap ){
//			this->texmap = texmap;
//		}
//
//		void set_render_context( RenderGlobalContext* globContext ){
//			this->globContext = globContext;
//			if( this->node )
//				this->nodeTM = node->GetNodeTM( globContext->time );
//		}
//
//		void set_render_instance( RenderInstance* rendInst ){
//			this->rendInst = rendInst;
//		}
//
//		void set_node( INode* node ){
//			this->node = node;
//			if( this->globContext )
//				this->nodeTM = node->GetNodeTM( globContext->time );
//		}
//
//		void set_camera( const Matrix3& toCamera, const Matrix3& toCameraInv ){
//			this->toCamera = toCamera;
//			this->toCameraInv = toCameraInv;
//		}
//
//		void set_result_type( result_type::enum_t resultType ){
//			this->resultType = resultType;
//		}
//
//		static const int DISABLED_MAP_CHANNEL = INT_MIN; //use this in add_channel_input() to prevent that input
//from binding.
//
//		void add_channel_input( int mapChan ){
//			m_inputMapping.push_back( mapChan );
//		}
//
//		virtual void set_input( std::size_t inputIndex, std::ptrdiff_t relPtr ){
//			if( inputIndex == 0 )
//				bind_position( relPtr );
//			else if( inputIndex == 1 )
//				bind_vertexColor( relPtr );
//			else if( inputIndex == 2 )
//				bind_textureCoord( relPtr );
//			else if( inputIndex == 3 )
//				bind_normal( relPtr );
//			else if( inputIndex == 4 )
//				bind_mtlID( relPtr );
//			else if( inputIndex == 5 )
//				bind_ior( relPtr );
//			else{
//				bind_map_channel( m_inputMapping[inputIndex - 6], relPtr );
//			}
//		}
//
//		virtual void set_output( std::ptrdiff_t relPtr ) {
//			m_output = relPtr;
//		}
//
//		virtual frantic::magma::magma_data_type get_output_type() const {
//			return this->resultType == result_type::mono ?
//*frantic::magma::magma_singleton::get_named_data_type( _T("Float") ) :
//*frantic::magma::magma_singleton::get_named_data_type( _T("Vec3") );
//		}
//
//		virtual void apply( runtime_data& data ) const {
//			apply( data.tempData );
//		}
//	};
// }

// namespace frantic{ namespace magma{ namespace nodes{ namespace max3d{

// void TexmapNode::compile_as_extension_type( frantic::magma::magma_compiler_interface& compiler ){
//	Texmap* texmap = get_texmap();
//	if( !texmap )
//		throw magma_exception() << magma_exception::node_id( get_id() ) << magma_exception::property_name(
//_T("texmap") ) << magma_exception::error_name( _T("The texmap was undefined or deleted") );
//
//	texmap_expression::result_type::enum_t resultType = texmap_expression::result_type::from_string(
//get_resultType() ); 	if( resultType == texmap_expression::result_type::error ) 		throw magma_exception() <<
//magma_exception::node_id( get_id() ) << magma_exception::property_name( _T("resultType") ) <<
//magma_exception::error_name( _T("The result type \"") + get_resultType() + _T("\" was unknown") );
//
//	RenderGlobalContext* globContext = NULL;
//	const RenderGlobalContext* cglobContext = NULL;
//	compiler.get_context_data().get_property( _T("MaxRenderGlobalContext"), cglobContext );
//
//	globContext = const_cast<RenderGlobalContext*>(cglobContext);
//
//	//TODO: We should substitute a default context of some sort.
//	if( !globContext )
//		throw magma_exception() << magma_exception::error_name( _T("Cannot use TexmapEval in this context") );
//
//	TimeValue t =  globContext->time;
//
//	//We currently don't bother complaining about missing UVWs. Maybe we can think on this?
//	BitArray reqUVWs;
//	bool ignoreReqUVWs = false;
//
//	frantic::max3d::shaders::update_map_for_shading( texmap, t );
//	frantic::max3d::shaders::collect_map_requirements( texmap, reqUVWs );
//
//	INode* curNode = NULL;
//	compiler.get_context_data().get_property( _T("CurrentINode"), curNode );
//
//	bool inWorldSpace = true;
//	compiler.get_context_data().get_property( _T("InWorldSpace"), inWorldSpace );
//
//	RenderInstance* rendInst = NULL;
//	if( curNode != NULL ){
//		for( int i = 0, iEnd = globContext->NumRenderInstances(); i < iEnd && !rendInst; ++i ){
//			if( RenderInstance* curInst = globContext->GetRenderInstance( i ) ){
//				if( curInst->GetINode() == curNode )
//					rendInst = curInst;
//			}
//		}
//	}
//
//	Matrix3 toCamera = globContext->worldToCam;
//	Matrix3 fromCamera = globContext->camToWorld;
//
//	if( !inWorldSpace && curNode ){
//		Matrix3 nodeTM = curNode->GetNodeTM( t );
//
//		toCamera = nodeTM * toCamera;
//		fromCamera = fromCamera * Inverse( nodeTM );
//	}
//
//	if( ember_compiler* bc2 = dynamic_cast<ember_compiler*>( &compiler ) ){
//		std::unique_ptr<texmap_expression> result( new texmap_expression );
//
//		result->set_result_type( resultType );
//		result->set_texmap( texmap );
//		result->set_render_context( globContext );
//		result->set_render_instance( rendInst );
//		result->set_node( curNode );
//		result->set_camera( toCamera, fromCamera );
//
//		std::vector< frantic::magma::magma_data_type > inputTypes;
//		inputTypes.push_back( *frantic::magma::magma_singleton::get_named_data_type( _T("Vec3") ) );  //Position
//		inputTypes.push_back( *frantic::magma::magma_singleton::get_named_data_type( _T("Vec3") ) );  //Vert
//Color 		inputTypes.push_back( *frantic::magma::magma_singleton::get_named_data_type( _T("Vec3") ) );  //Texture Coord
//		inputTypes.push_back( *frantic::magma::magma_singleton::get_named_data_type( _T("Vec3") ) );  //Normal
//		inputTypes.push_back( *frantic::magma::magma_singleton::get_named_data_type( _T("Int") ) );   //MtlID
//		inputTypes.push_back( *frantic::magma::magma_singleton::get_named_data_type( _T("Float") ) ); //IOR
//
//		for( std::vector< map_channel_data >::const_iterator it = m_mapChannelConnections.begin(), itEnd =
//m_mapChannelConnections.end(); it != itEnd; ++it ){
//			//result->add_channel_input( reqUVWs[ it->channel ] ? it->channel :
//texmap_expression::DISABLED_MAP_CHANNEL ); 			result->add_channel_input( it->channel ); 			inputTypes.push_back(
//*frantic::magma::magma_singleton::get_named_data_type( _T("Vec3") ) );
//		}
//
//		bc2->compile( static_cast< std::unique_ptr<subexpression> >(result), *this, inputTypes );
//	}else{
//		frantic::magma::nodes::max3d::magma_max_node_base::compile_as_extension_type( compiler );
//	}
// }

} // namespace max3d
} // namespace ember

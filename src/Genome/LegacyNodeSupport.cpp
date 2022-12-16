// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/magma/max3d/MagmaMaxNodeExtension.hpp>
#include <frantic/magma/max3d/nodes/magma_curve_op_node.hpp>
#include <frantic/magma/max3d/nodes/magma_geometry_input_node.hpp>
#include <frantic/magma/max3d/nodes/magma_input_object_node.hpp>
#include <frantic/magma/max3d/nodes/magma_input_particles_node.hpp>
#include <frantic/magma/max3d/nodes/magma_input_value_node.hpp>
#include <frantic/magma/max3d/nodes/magma_script_op_node.hpp>
#include <frantic/magma/max3d/nodes/magma_texmap_node.hpp>
#include <frantic/magma/max3d/nodes/magma_transform_node.hpp>

// This is pretty sketchy ... I'm purposefully returning an object linked to a different ClassID

class legacy_magma_curve_op_node_desc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new frantic::magma::nodes::max3d::magma_curve_op_node::max_impl; }
    const TCHAR* ClassName() { return frantic::magma::nodes::max3d::magma_curve_op_node::max_impl::s_ClassName; }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x6fca5a33, 0x68672cee ); }
    const TCHAR* Category() { return _T(""); }

    const TCHAR* InternalName() {
        return frantic::magma::nodes::max3d::magma_curve_op_node::max_impl::s_ClassName;
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() {
        return frantic::magma::nodes::max3d::magma_curve_op_node::max_impl::s_ClassName;
    }
#endif
};

class legacy_magma_input_geometry_node_desc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new frantic::magma::nodes::max3d::magma_input_geometry_node::max_impl; }
    const TCHAR* ClassName() { return frantic::magma::nodes::max3d::magma_input_geometry_node::max_impl::s_ClassName; }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x511b111c, 0x76ac5e7a ); }
    const TCHAR* Category() { return _T(""); }

    const TCHAR* InternalName() {
        return frantic::magma::nodes::max3d::magma_input_geometry_node::max_impl::s_ClassName;
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() {
        return frantic::magma::nodes::max3d::magma_input_geometry_node::max_impl::s_ClassName;
    }
#endif
};

class legacy_magma_input_value_node_desc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new frantic::magma::nodes::max3d::magma_input_value_node::max_impl; }
    const TCHAR* ClassName() { return frantic::magma::nodes::max3d::magma_input_value_node::max_impl::s_ClassName; }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x16495b8e, 0xce470a3 ); }
    const TCHAR* Category() { return _T(""); }

    const TCHAR* InternalName() {
        return frantic::magma::nodes::max3d::magma_input_value_node::max_impl::s_ClassName;
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() {
        return frantic::magma::nodes::max3d::magma_input_value_node::max_impl::s_ClassName;
    }
#endif
};

class legacy_magma_input_particles_node_desc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new frantic::magma::nodes::max3d::magma_input_particles_node::max_impl; }
    const TCHAR* ClassName() { return frantic::magma::nodes::max3d::magma_input_particles_node::max_impl::s_ClassName; }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x57c33580, 0x3b7c28e0 ); }
    const TCHAR* Category() { return _T(""); }

    const TCHAR* InternalName() {
        return frantic::magma::nodes::max3d::magma_input_particles_node::max_impl::s_ClassName;
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() {
        return frantic::magma::nodes::max3d::magma_input_particles_node::max_impl::s_ClassName;
    }
#endif
};

class legacy_magma_input_object_node_desc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new frantic::magma::nodes::max3d::magma_input_object_node::max_impl; }
    const TCHAR* ClassName() { return frantic::magma::nodes::max3d::magma_input_object_node::max_impl::s_ClassName; }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x292513f1, 0x54223fc6 ); }
    const TCHAR* Category() { return _T(""); }

    const TCHAR* InternalName() {
        return frantic::magma::nodes::max3d::magma_input_object_node::max_impl::s_ClassName;
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() {
        return frantic::magma::nodes::max3d::magma_input_object_node::max_impl::s_ClassName;
    }
#endif
};

class legacy_magma_script_op_node_desc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new frantic::magma::nodes::max3d::magma_script_op_node::max_impl; }
    const TCHAR* ClassName() { return frantic::magma::nodes::max3d::magma_script_op_node::max_impl::s_ClassName; }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x66e47c84, 0x433d4d37 ); }
    const TCHAR* Category() { return _T(""); }

    const TCHAR* InternalName() {
        return frantic::magma::nodes::max3d::magma_script_op_node::max_impl::s_ClassName;
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() {
        return frantic::magma::nodes::max3d::magma_script_op_node::max_impl::s_ClassName;
    }
#endif
};

class legacy_magma_from_space_node_desc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new frantic::magma::nodes::max3d::magma_from_space_node::max_impl; }
    const TCHAR* ClassName() { return frantic::magma::nodes::max3d::magma_from_space_node::max_impl::s_ClassName; }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x12b154f0, 0x697902fd ); }
    const TCHAR* Category() { return _T(""); }

    const TCHAR* InternalName() {
        return frantic::magma::nodes::max3d::magma_from_space_node::max_impl::s_ClassName;
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() {
        return frantic::magma::nodes::max3d::magma_from_space_node::max_impl::s_ClassName;
    }
#endif
};

class legacy_magma_to_space_node_desc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new frantic::magma::nodes::max3d::magma_to_space_node::max_impl; }
    const TCHAR* ClassName() { return frantic::magma::nodes::max3d::magma_to_space_node::max_impl::s_ClassName; }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x6fc53fa7, 0x17ed0e87 ); }
    const TCHAR* Category() { return _T(""); }

    const TCHAR* InternalName() {
        return frantic::magma::nodes::max3d::magma_to_space_node::max_impl::s_ClassName;
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() {
        return frantic::magma::nodes::max3d::magma_to_space_node::max_impl::s_ClassName;
    }
#endif
};

class legacy_magma_texmap_node_desc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading*/ ) { return new frantic::magma::nodes::max3d::magma_texmap_node::max_impl; }
    const TCHAR* ClassName() { return frantic::magma::nodes::max3d::magma_texmap_node::max_impl::s_ClassName; }
    SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x151a7325, 0xadf25d1 ); }
    const TCHAR* Category() { return _T(""); }

    const TCHAR* InternalName() {
        return frantic::magma::nodes::max3d::magma_texmap_node::max_impl::s_ClassName;
    } // returns fixed parsable name (scripter-visible name)
    HINSTANCE HInstance() { return hInstance; }
#if MAX_VERSION_MAJOR >= 24
    const MCHAR* NonLocalizedClassName() {
        return frantic::magma::nodes::max3d::magma_texmap_node::max_impl::s_ClassName;
    }
#endif
};

namespace {
legacy_magma_curve_op_node_desc g_legacyCurveOpDesc;
legacy_magma_input_geometry_node_desc g_legacyInputGeometryDesc;
legacy_magma_input_value_node_desc g_legacyInputValueDesc;
legacy_magma_input_particles_node_desc g_legacyInputParticlesDesc;
legacy_magma_input_object_node_desc g_legacyInputObjectDesc;
legacy_magma_script_op_node_desc g_legacyScriptOpDesc;
legacy_magma_from_space_node_desc g_legacyFromSpaceOpDesc;
legacy_magma_to_space_node_desc g_legacyToSpaceOpDesc;
legacy_magma_texmap_node_desc g_legacyTexmapDesc;
} // namespace

void get_legacy_genome_class_descs( std::vector<ClassDesc*>& outDescs ) {
    outDescs.push_back( &g_legacyCurveOpDesc );
    outDescs.push_back( &g_legacyInputGeometryDesc );
    outDescs.push_back( &g_legacyInputValueDesc );
    outDescs.push_back( &g_legacyInputParticlesDesc );
    outDescs.push_back( &g_legacyInputObjectDesc );
    outDescs.push_back( &g_legacyScriptOpDesc );
    outDescs.push_back( &g_legacyFromSpaceOpDesc );
    outDescs.push_back( &g_legacyToSpaceOpDesc );
    outDescs.push_back( &g_legacyTexmapDesc );
}

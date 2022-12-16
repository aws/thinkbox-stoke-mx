// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/magma/magma_compiler_interface.hpp>
#include <frantic/magma/magma_data_type.hpp>

#include <frantic/max3d/ifnpub_wrap.hpp>

#include <map>

#define GenomeMod_CLASS_ID Class_ID( 0x57b6085d, 0x44a41a84 )
#define Genome_INTERFACE Interface_ID( 0x5c2403e, 0x47ce3e04 )

class IGenome : public FPMixinInterface {
  public:
    virtual ~IGenome() {}

    // From FPMixinInterface
    virtual FPInterfaceDesc* GetDesc() { return GetDescByID( Genome_INTERFACE ); }

    virtual FPInterfaceDesc* GetDescByID( Interface_ID id ) = 0;

    virtual bool GetLastError( TYPE_TSTR_BR_TYPE outMessage, TYPE_INT_BR_TYPE outMagmaID ) = 0;

    struct iteration_type {
        enum enum_t { vertex, face, face_vertex };
    };

    virtual Value* GetChannelNames( int iterationType, bool forOutput = false ) = 0;

    /**
     * This method allows us to evaluate a Genome modifier while extracting debugging information.
     * @param t The time to evaluate at
     * @param mc The mod context we are apply with
     * @param os Ptr to the data flowing up the pipeline
     * @param node Ptr to the node we are evaluating for
     * @param outVals For each iteration, a list of the values output from each node is stored.
     */
    virtual void EvaluateDebug( TimeValue t, ModContext& mc, ObjectState* os, INode* node,
                                std::vector<frantic::magma::debug_data>& outVals ) = 0;

    virtual void SetThreading( bool threads = true ) = 0;
    virtual bool GetThreading() const = 0;

  public:
    enum {
        kFnGetLastError,
        kFnGetChannelNames,
        kFnSetThreading,
        kFnGetThreading,
    };

    enum { kEnumIterationType };

#pragma warning( push )
#pragma warning( disable : 4238 4100 )
    BEGIN_FUNCTION_MAP
    FN_2( kFnGetLastError, TYPE_bool, GetLastError, TYPE_TSTR_BR, TYPE_INT_BR )
    FN_2( kFnGetChannelNames, TYPE_VALUE, GetChannelNames, TYPE_ENUM, TYPE_bool )
    PROP_FNS( kFnGetThreading, GetThreading, kFnSetThreading, SetThreading, TYPE_bool )
    END_FUNCTION_MAP;
#pragma warning( pop )

    friend class MagmaHolderClassDesc;

    inline static void init_fpinterface_desc( FPInterfaceDesc& desc ) {
        desc.AppendEnum( kEnumIterationType, 3, _M( "vertex" ), iteration_type::vertex, _M( "face" ),
                         iteration_type::face, _M( "faceVertex" ), iteration_type::face_vertex, PARAMTYPE_END );

        desc.AppendFunction( kFnGetLastError, _M( "GetLastError" ), 0, TYPE_bool, 0, 2, _M( "OutMessage" ), 0,
                             TYPE_TSTR_BR, f_inOut, FPP_OUT_PARAM, f_keyArgDefault, NULL, _M( "OutNodeID" ), 0,
                             TYPE_INT_BR, f_inOut, FPP_OUT_PARAM, f_keyArgDefault, NULL, PARAMTYPE_END );

        desc.AppendFunction( kFnGetChannelNames, _M( "GetChannelNames" ), 0, TYPE_VALUE, 0, 2, _M( "IterationType" ), 0,
                             TYPE_ENUM, kEnumIterationType, _M( "ForOutput" ), 0, TYPE_bool, f_keyArgDefault, false,
                             PARAMTYPE_END );

        desc.AppendProperty( kFnGetThreading, kFnSetThreading, _M( "ThreadingEnabled" ), 0, TYPE_bool );
    }
};

inline IGenome* GetGenomeInterface( ReferenceTarget* rtarg ) {
    return rtarg ? (IGenome*)rtarg->GetInterface( Genome_INTERFACE ) : NULL;
}

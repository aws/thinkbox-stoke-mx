// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
/**
 *	This is an exact copy of the MagmaMaxSingleton at the moment, but it eventually will include
 *	mesh specific nodes, while the MagmaMaxSingleton will likely include particle specific nodes.
 */

#pragma once

#include <frantic/magma/magma_singleton.hpp>

class MaxGeometryMagmaSingleton : public frantic::magma::magma_singleton {

    MaxGeometryMagmaSingleton();

    virtual ~MaxGeometryMagmaSingleton() {}

  public:
    static MaxGeometryMagmaSingleton& get_instance();

    virtual void get_predefined_particle_channels(
        std::vector<std::pair<frantic::tstring, frantic::magma::magma_data_type>>& outChannels ) const;

    virtual void get_predefined_vertex_channels(
        std::vector<std::pair<frantic::tstring, frantic::magma::magma_data_type>>& outChannels ) const;

    virtual void get_predefined_face_channels(
        std::vector<std::pair<frantic::tstring, frantic::magma::magma_data_type>>& outChannels ) const;

    virtual void get_predefined_face_vertex_channels(
        std::vector<std::pair<frantic::tstring, frantic::magma::magma_data_type>>& outChannels ) const;
};

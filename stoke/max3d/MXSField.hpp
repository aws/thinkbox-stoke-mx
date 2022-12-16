// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/max3d/fnpublish/StandaloneInterface.hpp>

#include <frantic/volumetrics/field_interface.hpp>

#include <boost/filesystem/path.hpp>

#if MAX_VERSION_MAJOR >= 25
#include <geom/box3.h>
#else
#include <box3.h>
#endif

#define MXSField_INTERFACE Interface_ID( 0x78c86fad, 0x7e12735a )

namespace ember {
namespace max3d {

class MXSField;

MXSField* CreateMXSField( INode* pNode, TimeValue t, int stackIndex );
MXSField* GetMXSFieldInterface( InterfaceServer* pServer );

class MXSField : public frantic::max3d::fnpublish::StandaloneInterface<MXSField> {
  public:
    MXSField();

    virtual ~MXSField();

    void SetField( boost::shared_ptr<frantic::volumetrics::field_interface> pField, const Box3& bounds, float spacing );

    Point3 GetBoundsMin();

    Point3 GetBoundsMax();

    float GetSpacing();

    Tab<TYPE_STRING_TYPE> GetChannels();

    // TODO: Expose more MXS useful info here.
    void Clear();

    // void Resample( const Point3& boundsMin, const Point3& boundsMax, float spacing );

    // bool Prune( float tolerance );

    // bool WriteToFile( const MCHAR* szPath );

    FPValue SampleField( const MCHAR* szChannelName, const Point3& position );

    // Non-MXS exposed functions here
    // const boost::shared_ptr<frantic::volumetrics::field_interface>& get_field() const;

    // From StandaloneInterface
    virtual ThisInterfaceDesc* GetDesc();

  private:
    // void WriteToFXD( const boost::filesystem::path& path );
    // void WriteToOpenVDB( const boost::filesystem::path& path );

  private:
    boost::shared_ptr<frantic::volumetrics::field_interface> m_pField;

    Box3 m_bounds;
    float m_spacing;

    std::vector<MSTR> m_cachedChannelList;
};

inline MXSField* GetMXSFieldInterface( InterfaceServer* pServer ) {
    return pServer ? static_cast<MXSField*>( pServer->GetInterface( MXSField_INTERFACE ) ) : NULL;
}

} // namespace max3d
} // namespace ember

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <stoke/max3d/Attributions.hpp>
#include <stoke/max3d/StokeGlobalInterface.hpp>

#include <frantic/strings/utf8.hpp>

#include <boost/filesystem/path.hpp>

#include <codecvt>
#include <fstream>
#include <memory>
#include <sstream>

namespace stoke {

std::wstring get_attributions() {
    const boost::filesystem::path homePath = frantic::strings::to_wstring( GetStokeInterface()->GetRootDirectory() );
    const boost::filesystem::path attributionPath = homePath / L"Legal/third_party_licenses.txt";
    std::wifstream wideIn( attributionPath.wstring() );
    if( wideIn.fail() ) {
        return L"Could not load attributions.";
    }
    wideIn.imbue( std::locale( std::locale::empty(), new std::codecvt_utf8<wchar_t> ) );
    std::wstringstream wideStream;
    wideStream << wideIn.rdbuf();
    return wideStream.str();
}

} // namespace stoke

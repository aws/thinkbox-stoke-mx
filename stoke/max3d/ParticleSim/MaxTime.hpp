// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stoke/time_interface.hpp>

#include <frantic/max3d/time.hpp>

#pragma warning( push, 3 )
#include <maxtypes.h>
#pragma warning( pop )

namespace stoke {
namespace max3d {

inline TimeValue to_ticks( const time_interface& theTime ) {
    return static_cast<TimeValue>( theTime.as_int( time_interface::kNative ) );
}

class MaxTime : public time_interface {
  public:
    MaxTime( TimeValue t );

    virtual double as( format requestedType ) const;

    virtual boost::int64_t as_int( format requestedType ) const;

  private:
    TimeValue m_timeValue;
};

inline MaxTime::MaxTime( TimeValue t )
    : m_timeValue( t ) {}

inline double MaxTime::as( format requestedType ) const {
    switch( requestedType ) {
    case kPicoseconds:
        return frantic::max3d::to_picoseconds<double>( m_timeValue );
    case kMilliseconds:
        return frantic::max3d::to_milliseconds<double>( m_timeValue );
    case kSeconds:
        return frantic::max3d::to_seconds<double>( m_timeValue );
    case kFrames:
        return frantic::max3d::to_frames<double>( m_timeValue );
    case kNative:
    default:
        return static_cast<double>( m_timeValue );
    }
}

inline boost::int64_t MaxTime::as_int( format requestedType ) const {
    switch( requestedType ) {
    case kPicoseconds:
        // Error in this conversion is at most 2/3 of a picosecond, which is well beyond the accuracy of a tick.
        return static_cast<boost::int64_t>(
            ( 625 * m_timeValue ) / ( TIME_TICKSPERSEC / 1600 ) ); // gcd(4800, 1000000) = 1600, 1000000 / 1600 = 625
    case kMilliseconds:
        // Error in this conversion is at most 23/24 of a millisecond, which is ~4.6 ticks.
        return static_cast<boost::int64_t>( ( 5 * m_timeValue ) /
                                            ( TIME_TICKSPERSEC / 200 ) ); // gcd(4800, 1000) = 200, 1000 / 200 = 5
    case kSeconds:
        return static_cast<boost::int64_t>( m_timeValue / TIME_TICKSPERSEC );
    case kFrames:
        return static_cast<boost::int64_t>( m_timeValue / GetTicksPerFrame() );
    case kNative:
    default:
        return static_cast<boost::int64_t>( m_timeValue );
    }
}

} // namespace max3d
} // namespace stoke

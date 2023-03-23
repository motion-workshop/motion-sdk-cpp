// Copyright Motion Workshop. All Rights Reserved.
#pragma once

#include "endian.hpp"

#include <algorithm>

#if defined(BOOST_BIG_ENDIAN)
#define MOTION_SDK_BIG_ENDIAN 1
#elif !defined(BOOST_LITTLE_ENDIAN)
#error Unsupported endian platform
#endif

namespace Motion { namespace SDK { namespace detail {

/**
  Generic byte swapping function for little-endian data to native type.
  This is certainly not the fastest way to handle byte-swapping, but
  consider it to be a fall-back implementation with minimal dependencies.

  Example usage:
  @code
  std::vector<int> buffer;

  // ... Read some binary data into the int buffer ...

  // Element-wise transformation from little-endian to native byte ordering.
  std::transform(
    buffer.begin(), buffer.end(),
    buffer.begin(), &detail::little_endian_to_native<int>);
  @endcode
*/
template <typename T>
inline T little_endian_to_native(T& value)
{
#if MOTION_SDK_BIG_ENDIAN
    T result = T();

    const char* p = reinterpret_cast<const char*>(&value);

    std::reverse_copy(p, p + sizeof(T), reinterpret_cast<char*>(&result));

    return result;
#else
    return value;
#endif // MOTION_SDK_BIG_ENDIAN
}

}}} // namespace Motion::SDK::detail

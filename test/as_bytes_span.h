#ifndef D_AS_BYTES_SPAN_H
#define D_AS_BYTES_SPAN_H

#include <cstddef>
#include <span>

namespace aria2 {

// Wraps a raw buffer the tests own into the byte span the production
// entry points take.
inline std::span<const std::byte> asBytesSpan(const unsigned char* data,
                                              size_t len)
{
  return std::as_bytes(std::span{data, len});
}

} // namespace aria2

#endif // D_AS_BYTES_SPAN_H

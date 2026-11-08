#ifndef D_MOCK_PIECE_SELECTOR_H
#define D_MOCK_PIECE_SELECTOR_H

#include "PieceSelector.h"

namespace aria2 {

class MockPieceSelector : public PieceSelector {
public:
  virtual std::optional<size_t> select(const unsigned char* bitfield,
                                       size_t nbits) const override
  {
    return std::nullopt;
  }
};

} // namespace aria2

#endif // D_MOCK_PIECE_SELECTOR_H

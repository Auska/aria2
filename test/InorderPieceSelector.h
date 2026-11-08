#ifndef D_IN_ORDER_PIECE_SELECTOR_H
#define D_IN_ORDER_PIECE_SELECTOR_H

#include "PieceSelector.h"
#include "bitfield.h"

namespace aria2 {

class InorderPieceSelector : public PieceSelector {
public:
  virtual std::optional<size_t> select(const unsigned char* bitfield,
                                       size_t nbits) const override
  {
    for (size_t i = 0; i < nbits; ++i) {
      if (bitfield::test(bitfield, nbits, i)) {
        return i;
      }
    }
    return std::nullopt;
  }
};

} // namespace aria2

#endif // D_IN_ORDER_PIECE_SELECTOR_H

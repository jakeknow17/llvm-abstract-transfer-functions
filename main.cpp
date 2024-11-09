#include <llvm/ADT/APInt.h>
#include <llvm/Support/KnownBits.h>
#include <llvm/Support/raw_ostream.h>
#include <vector>

using llvm::APInt;
using llvm::KnownBits;

std::vector<KnownBits> enumerateFromBitWidth(unsigned bitWidth) {
  // Each KnownBit can be either 0, 1, or unknown.
  // This corresponds to finding all tertiary numbers
  // with bitwidth of `bitWidth`
  uint64_t total = 1;
  for (unsigned i = 0; i < bitWidth; i++) {
    total *= 3;
  }

  std::vector<KnownBits> result;
  result.reserve(total); // We know there will be `total` numbers

  for (uint64_t i; i < total; i++) {
    KnownBits kb(bitWidth);
    uint64_t temp = i;

    for (unsigned bit = 0; bit < bitWidth; bit++) {
      uint64_t digit = temp % 3; // 0, 1, or unknown
      temp /= 3;                 // Move to next tertiary "bit"
      if (digit == 0) {          // Check cases
        kb.Zero.setBit(bit);
      } else if (digit == 1) {
        kb.One.setBit(bit);
      }
    }

    result.push_back(kb);
  }

  return result;
}

std::vector<APInt> concretization(const KnownBits &kb) {
  unsigned bw = kb.getBitWidth();

  // Find the indexes of all unknown values
  std::vector<unsigned> unknownIndexes;
  for (unsigned i = 0; i < bw; i++) {
    if (!kb.Zero[i] && !kb.One[i]) {
      unknownIndexes.push_back(i);
    }
  }

  // All possible combinations is 2^numUnknownIndexes
  uint64_t total = 1ull << unknownIndexes.size();

  std::vector<APInt> result;

  for (uint64_t i = 0; i < total; i++) {
    APInt ap = kb.One; // Start with all values known to be 1
    uint64_t temp = i;
    for (const unsigned unknownIdx : unknownIndexes) {
      if (temp & 1) {
        ap.setBit(unknownIdx);
      } else {
        ap.clearBit(unknownIdx);
      }
      temp /= 2;
    }
    result.push_back(ap);
  }

  return result;
}

KnownBits abstraction(const std::vector<APInt> &values) {
  assert(!values.empty() && "Values set should not be empty");
  unsigned bw = values.begin()->getBitWidth();

  APInt knownZero(bw, 0x0, false);
  APInt knownOne = APInt::getAllOnes(bw);

  for (const APInt &value : values) {
    assert(value.getBitWidth() == bw &&
           "All values must have the same bitwidth");
    knownZero &= ~value;
    knownOne &= value;
  }

  KnownBits kb(bw);
  kb.Zero = knownZero;
  kb.One = knownOne;

  return kb;
}

int main() { return 0; }

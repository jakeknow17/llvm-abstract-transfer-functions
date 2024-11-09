// Author: Jacob Knowlton
// Date:   Nov 2024

#include <chrono>
#include <iostream>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/KnownBits.h>
#include <string>
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

  for (uint64_t i = 0; i < total; i++) {
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

    assert(!kb.hasConflict() && "Known 0s and 1s should not conflict");

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

  APInt knownZero = APInt::getAllOnes(bw);
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

KnownBits naiveMulhs(const KnownBits &lhs, const KnownBits &rhs) {
  unsigned bw = lhs.getBitWidth();
  assert(bw == rhs.getBitWidth() && "RHS and LHS must have the same bitwidth");
  std::vector<APInt> concreteLhs = concretization(lhs);
  std::vector<APInt> concreteRhs = concretization(rhs);

  std::vector<APInt> cfResult;
  for (const APInt cLhs : concreteLhs) {
    for (const APInt cRhs : concreteRhs) {
      APInt wideLhs = cLhs.sext(2 * bw);
      APInt wideRhs = cRhs.sext(2 * bw);
      APInt prod = wideLhs * wideRhs;
      APInt highBits =
          prod.extractBits(bw, bw); // Extract bits starting at bw, length bw
      cfResult.push_back(highBits);
    }
  }
  return abstraction(cfResult);
}

void testMulhsTransferFunctions(unsigned BitWidth) {
  std::vector<KnownBits> allKnownBits = enumerateFromBitWidth(BitWidth);
  uint64_t totalKnownBits = allKnownBits.size();

  uint64_t compositeMorePrecise = 0;
  uint64_t naiveMorePrecise = 0;
  uint64_t samePrecision = 0;
  uint64_t incomparableResults = 0;

  double totalTimeComposite = 0.0;
  double totalTimeNaive = 0.0;

  // Iterate through all pairs
  for (size_t i = 0; i < totalKnownBits; i++) {
    for (size_t j = 0; j < totalKnownBits; j++) {
      const KnownBits &LHS = allKnownBits[i];
      const KnownBits &RHS = allKnownBits[j];

      // Compute composite and naive mulhs
      auto t1 = std::chrono::high_resolution_clock::now();
      KnownBits composite = KnownBits::mulhs(LHS, RHS);
      auto t2 = std::chrono::high_resolution_clock::now();
      totalTimeComposite += (t2 - t1).count();

      t1 = std::chrono::high_resolution_clock::now();
      KnownBits naive = naiveMulhs(LHS, RHS);
      t2 = std::chrono::high_resolution_clock::now();
      totalTimeNaive += (t2 - t1).count();

      // Check if results are comparable
      bool isIncomparable = false;
      for (int k = 0; k < BitWidth; k++) {
        if ((composite.Zero[k] && naive.One[k]) ||
            (composite.One[k] && naive.Zero[k])) {
          incomparableResults++;
          isIncomparable = true;
          break;
        }
      }
      if (isIncomparable)
        continue;

      // Check which transfer function is more precise
      unsigned compositePrecision =
          composite.Zero.countPopulation() + composite.One.countPopulation();
      unsigned naivePrecision =
          naive.Zero.countPopulation() + naive.One.countPopulation();

      if (compositePrecision > naivePrecision)
        compositeMorePrecise++;
      else if (naivePrecision > compositePrecision)
        naiveMorePrecise++;
      else
        samePrecision++;
    }
  }

  // Calculate average time
  double avgTimeComposite =
      totalTimeComposite / (totalKnownBits * totalKnownBits);
  double avgTimeNaive = totalTimeNaive / (totalKnownBits * totalKnownBits);

  // Report results
  std::cout << "Testing mulhs Transfer Functions for BitWidth = " << BitWidth
            << std::endl;
  std::cout << "Total abstract values: " << totalKnownBits << std::endl;
  std::cout << "Composite transfer function more precise: "
            << compositeMorePrecise << std::endl;
  std::cout << "Naive transfer function more precise: " << naiveMorePrecise
            << std::endl;
  std::cout << "Same precision for both transfer functions: " << samePrecision
            << std::endl;
  std::cout << "Incomparable results: " << incomparableResults << std::endl;
  std::cout << "Average composite time: " << avgTimeComposite << std::endl;
  std::cout << "Average naive time: " << avgTimeNaive << std::endl << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "Usage: testMulhs <bitWidth>" << std::endl;
    return 1;
  }

  // Try to parse bitwidth from cmdline
  unsigned bw = 6;
  try {
    bw = std::stoi(argv[1]);
  } catch (std::invalid_argument) {
    bw = 4;
  }

  testMulhsTransferFunctions(bw);
  return 0;
}

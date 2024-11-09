# TestMulhs Transfer Function Comparison

A C++ program to compare precision and performance of LLVM's native `mulhs` (signed high half multiply) vs. a naive, optimal transfer function using LLVM's `KnownBits` utility.

## Features

- Enumerates all `KnownBits` values for a given bit width.
- Compares precision and execution time of two `mulhs` implementations.

## Requirements

- LLVM library (for `APInt` and `KnownBits`)
- C++17 or later

## Building the Code

```bash
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH=/path/to/llvm/project -DCMAKE_BUILD_TYPE=Release ..
make
```

## Running the Code

After building, run the program with a specified bit width for the KnownBits enumeration:
```bash
./testMulhs <BITWIDTH>
```

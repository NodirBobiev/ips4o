# IPS4o Sequential Sorting Algorithm Overview

## Introduction

IPS4o (In-place Parallel Super Scalar Samplesort) is a highly optimized comparison-based sorting algorithm that combines sample sort with in-place partitioning techniques. This document focuses on the **sequential mode** of the algorithm, analyzing its structure, phases, and implementation details.

## Entry Points and API

### Main Entry Points
- **`ips4o::sort(begin, end, comp)`** - Standard interface for sequential sorting
- **`ips4o::make_sorter(comp)`** - Creates a reusable sequential sorter object
- **`SequentialSorter::operator()(begin, end)`** - Callable sorter object

### Key Files
- `include/ips4o.hpp` - Main API entry point
- `include/ips4o/ips4o.hpp` - Core implementation with both sequential and parallel interfaces
- `include/ips4o/sequential.hpp` - Sequential algorithm implementation
- `include/ips4o/partitioning.hpp` - Core partitioning logic
- `include/ips4o/sampling.hpp` - Sample selection and classifier building
- `include/ips4o/base_case.hpp` - Small array sorting (insertion sort)

## Algorithm Flow

### Phase 1: Initial Checks and Simple Cases
```
ips4o::sort() → sortSimpleCases() → {
  - Empty array check
  - Already sorted check (forward/reverse)
  - Reverse sorted optimization
}
```

**Key Functions:**
- `detail::sortSimpleCases()` - Handles trivial cases
- Early termination for sorted/reverse-sorted inputs

### Phase 2: Base Case Handling
```
if (size <= kBaseCaseMultiplier * kBaseCaseSize) {
    detail::baseCaseSort() → detail::insertionSort()
}
```

**Configuration:**
- `kBaseCaseSize = 16` (default)
- `kBaseCaseMultiplier = 16` (default)
- Threshold: 256 elements (16 × 16)

### Phase 3: Recursive Sample Sort
```
SequentialSorter::operator() → Sorter::sequential() → {
  1. Sampling Phase
  2. Classification Phase  
  3. Partitioning Phase
  4. Recursive Calls
}
```

## Detailed Algorithm Phases

### 1. Sampling Phase (`sampling.hpp`)

**Purpose:** Select representative elements to create bucket boundaries

**Process:**
1. **Sample Selection:**
   ```cpp
   num_samples = min(step * num_buckets - 1, n / 2)
   selectSample(begin, end, num_samples, random_generator)
   ```

2. **Sample Sorting:**
   ```cpp
   sequential(begin, begin + num_samples)  // Recursive call
   ```

3. **Splitter Selection:**
   - Choose every `step`-th element as splitter
   - Skip duplicates to avoid empty buckets
   - Fill to next power of two

**Key Parameters:**
- `kLogBuckets = 8` → up to 256 buckets
- `oversamplingFactor = 20%` of bucket count
- Dynamic bucket count based on input size

### 2. Classification Phase (`local_classification.hpp`)

**Purpose:** Assign each element to its target bucket

**Process:**
1. **Classifier Building:**
   ```cpp
   buildClassifier(begin, end, classifier)
   classifier.build(log_buckets)  // Build decision tree
   ```

2. **Element Classification:**
   - Use branchless decision tree
   - Unrolled loops for performance (`kUnrollClassifier = 7`)
   - Cache-efficient block-wise processing

### 3. Partitioning Phase (`partitioning.hpp`)

**Purpose:** Rearrange elements according to bucket assignments

**Process:**
1. **Block Permutation:**
   ```cpp
   permuteBlocks<use_equal_buckets, false>()
   ```

2. **Cleanup Margins:**
   ```cpp
   writeMargins<false>(my_first_bucket, my_last_bucket, overflow_bucket)
   ```

**Key Features:**
- In-place partitioning using block swaps
- Block size: 2KB (configurable)
- Overflow handling for uneven distributions

### 4. Recursive Processing

**Process:**
```cpp
for (int i = 0; i < num_buckets; i += 1 + equal_buckets) {
    const auto start = bucket_start[i];
    const auto stop = bucket_start[i + 1];
    if (stop - start > 2 * kBaseCaseSize)
        sequential(begin + start, begin + stop);  // Recurse
}
```

**Termination Conditions:**
- Bucket size ≤ `2 * kBaseCaseSize` (32 elements)
- Single level threshold reached (`kSingleLevelThreshold`)

## Key Data Structures

### Configuration (`config.hpp`)
```cpp
struct Config {
    static constexpr bool kAllowEqualBuckets = true;
    static constexpr std::ptrdiff_t kBaseCaseSize = 16;
    static constexpr std::ptrdiff_t kBaseCaseMultiplier = 16;
    static constexpr std::ptrdiff_t kBlockSize = 2048;  // bytes
    static constexpr int kLogBuckets = 8;
    static constexpr int kEqualBucketsThreshold = 5;
    static constexpr int kOversamplingFactorPercent = 20;
};
```

### Sorter State (`ips4o_fwd.hpp`)
```cpp
template <class Cfg>
class Sorter {
    LocalData local_;           // Thread-local data
    Classifier* classifier_;    // Decision tree for classification
    diff_t* bucket_start_;     // Bucket boundaries
    // ... other state
};
```

### Local Data
- **Classifier:** Branchless decision tree for element classification
- **Bucket Pointers:** Track current write positions per bucket
- **Random Generator:** For sample selection
- **Buffers:** Temporary storage for block permutation

## Performance Characteristics

### Time Complexity
- **Average Case:** O(n log n)
- **Worst Case:** O(n log n) with high probability
- **Best Case:** O(n) for already sorted inputs

### Space Complexity
- **In-place:** O(log n) auxiliary space
- **Recursion Stack:** O(log n) depth
- **Temporary Buffers:** O(block_size) for permutation

### Cache Efficiency
- **Block-wise Processing:** Improves cache locality
- **Branchless Classification:** Reduces branch mispredictions
- **In-place Partitioning:** Minimizes memory movement

## Algorithm Invariants

1. **Partitioning Invariant:** After partitioning, all elements in bucket i are ≤ all elements in bucket j for i < j
2. **In-place Invariant:** Algorithm uses only O(log n) extra space
3. **Stability:** Algorithm is not stable (relative order not preserved)
4. **Comparison-based:** Only uses element comparisons, no assumptions about data

## Optimizations

### Branchless Operations
- Decision tree classification without conditional branches
- Reduces CPU pipeline stalls

### Equal Elements Handling
- Special buckets for equal elements (`kAllowEqualBuckets`)
- Reduces recursive depth for inputs with many duplicates

### Adaptive Bucket Count
- Dynamic bucket count based on input size
- Balances partitioning overhead vs. recursion depth

### Block-wise Processing
- Process elements in cache-friendly blocks
- Improves memory access patterns

## Integration Points

### Base Case Integration
```cpp
if (n <= 2 * Cfg::kBaseCaseSize) {
    detail::baseCaseSort(begin, end, comp);  // Insertion sort
    return;
}
```

### Parallel Mode Compatibility
- Sequential algorithm used as fallback in parallel mode
- Shared data structures designed for both modes
- Thread-safe classifier building

## Summary

The IPS4o sequential algorithm is a sophisticated sample sort implementation that achieves high performance through:

1. **Efficient Sampling:** Adaptive oversampling with duplicate handling
2. **Branchless Classification:** Fast element-to-bucket assignment
3. **In-place Partitioning:** Cache-efficient block-wise rearrangement
4. **Optimized Base Cases:** Fast insertion sort for small arrays
5. **Adaptive Parameters:** Dynamic configuration based on input characteristics

The algorithm combines theoretical soundness with practical optimizations, making it one of the fastest comparison-based sorting algorithms available.

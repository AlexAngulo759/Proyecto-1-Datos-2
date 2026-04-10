#include "sorting.h"
#include "../PagedArray.h"
#include <cstring>

using namespace std;

void bucketSort(PagedArray& arr, size_t n) {
    if (n < 2) return;

    int minVal = arr[0];
    int maxVal = arr[0];
    for (size_t i = 1; i < n; ++i) {
        int v = arr[i];
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
    }

    long long rangeLL = static_cast<long long>(maxVal) - static_cast<long long>(minVal) + 1;
    if (rangeLL > 5000000LL) {
        radixSort(arr, n);
        return;
    }

    size_t bucketCount = n / 1000 + 1;
    if (bucketCount < 2) bucketCount = 2;
    if (bucketCount > 10000) bucketCount = 10000;

    int** buckets = new int* [bucketCount];
    size_t* bucketSizes = new size_t[bucketCount]();
    size_t* bucketCaps = new size_t[bucketCount]();

    for (size_t b = 0; b < bucketCount; ++b) {
        buckets[b] = nullptr;
        bucketCaps[b] = 0;
    }

    for (size_t i = 0; i < n; ++i) {
        int val = arr[i];
        size_t idx = static_cast<size_t>((static_cast<long long>(val) - minVal) * bucketCount / rangeLL);
        if (idx >= bucketCount) idx = bucketCount - 1;

        if (buckets[idx] == nullptr) {
            bucketCaps[idx] = 16;
            buckets[idx] = new int[bucketCaps[idx]];
        }
        else if (bucketSizes[idx] == bucketCaps[idx]) {
            bucketCaps[idx] *= 2;
            int* newBucket = new int[bucketCaps[idx]];
            memcpy(newBucket, buckets[idx], bucketSizes[idx] * sizeof(int));
            delete[] buckets[idx];
            buckets[idx] = newBucket;
        }
        buckets[idx][bucketSizes[idx]++] = val;
    }

    size_t outIdx = 0;
    for (size_t b = 0; b < bucketCount; ++b) {
        if (bucketSizes[b] == 0) continue;

        int* bucket = buckets[b];
        size_t size = bucketSizes[b];

        for (size_t i = 1; i < size; ++i) {
            int key = bucket[i];
            size_t j = i;
            while (j > 0 && bucket[j - 1] > key) {
                bucket[j] = bucket[j - 1];
                --j;
            }
            bucket[j] = key;
        }

        for (size_t i = 0; i < size; ++i) {
            arr[outIdx++] = bucket[i];
        }

        delete[] bucket;
    }

    delete[] buckets;
    delete[] bucketSizes;
    delete[] bucketCaps;
}
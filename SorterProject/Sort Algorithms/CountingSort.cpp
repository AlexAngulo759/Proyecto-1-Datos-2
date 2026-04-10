#include "sorting.h"
#include "../PagedArray.h"

#include <memory>
#include <stdexcept>

using namespace std;

void countingSort(PagedArray& arr, std::size_t n) {
    if (n < 2) return;

    int minVal = arr[0];
    int maxVal = arr[0];

    for (std::size_t i = 1; i < n; ++i) {
        int v = arr[i];
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
    }

    long long rangeLL = static_cast<long long>(maxVal) - static_cast<long long>(minVal) + 1LL;

    if (rangeLL <= 0 || rangeLL > 5000000LL) {
        radixSort(arr, n);
        return;
    }

    std::size_t range = static_cast<std::size_t>(rangeLL);

    std::unique_ptr<std::size_t[]> count(new (std::nothrow) std::size_t[range]());
    std::unique_ptr<int[]> output(new (std::nothrow) int[n]);

    if (!count || !output) {
        throw std::runtime_error("No se pudo reservar memoria para counting sort.");
    }

    for (std::size_t i = 0; i < n; ++i) {
        long long idxLL = static_cast<long long>(arr[i]) - static_cast<long long>(minVal);
        ++count[static_cast<std::size_t>(idxLL)];
    }

    for (std::size_t i = 1; i < range; ++i) {
        count[i] += count[i - 1];
    }

    for (std::size_t i = n; i-- > 0;) {
        int value = arr[i];
        std::size_t idx = static_cast<std::size_t>(static_cast<long long>(value) - static_cast<long long>(minVal));
        output[--count[idx]] = value;
    }

    for (std::size_t i = 0; i < n; ++i) {
        arr[i] = output[i];
    }
}
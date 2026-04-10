#include "sorting.h"
#include "../PagedArray.h"

using namespace std;

static std::size_t partition(PagedArray& arr, std::size_t low, std::size_t high) {
    int pivot = arr[high];
    std::size_t i = low;

    for (std::size_t j = low; j < high; ++j) {
        if (arr[j] <= pivot) {
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
            ++i;
        }
    }

    int temp = arr[i];
    arr[i] = arr[high];
    arr[high] = temp;
    return i;
}

void quickSort(PagedArray& arr, std::size_t low, std::size_t high) {
    while (low < high) {
        std::size_t p = partition(arr, low, high);

        if (p > low && p - low < high - p) {
            quickSort(arr, low, p - 1);
            low = p + 1;
        }
        else {
            if (p + 1 < high) {
                quickSort(arr, p + 1, high);
            }
            if (p == 0) {
                break;
            }
            high = p - 1;
        }
    }
}
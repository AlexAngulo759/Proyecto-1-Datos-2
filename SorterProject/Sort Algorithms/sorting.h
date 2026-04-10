#pragma once

#include <cstddef>
#include <string>

class PagedArray;

extern std::size_t g_pageSizeBytes;
extern std::size_t g_pageCount;
extern std::string g_radixTempPath;

void countingSort(PagedArray& arr, std::size_t n);
void radixSort(PagedArray& arr, std::size_t n);
void quickSort(PagedArray& arr, std::size_t low, std::size_t high);
void bucketSort(PagedArray& arr, std::size_t n);
void mergeSort(PagedArray& arr, std::size_t left, std::size_t right);
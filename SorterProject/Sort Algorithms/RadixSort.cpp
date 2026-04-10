#define _CRT_SECURE_NO_WARNINGS  // Por si acaso, aunque usemos fopen_s

#include "sorting.h"
#include "../PagedArray.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <stdexcept>

using namespace std;

static void createZeroFilledBinaryFile(const std::string& path, std::size_t totalInts) {
    std::remove(path.c_str());

    FILE* f = nullptr;
    errno_t err = fopen_s(&f, path.c_str(), "wb");
    if (err != 0 || !f) {
        throw std::runtime_error("No se pudo crear el archivo temporal para radix.");
    }

    const std::size_t chunkInts = 1u << 20;
    std::unique_ptr<int[]> buffer(new (std::nothrow) int[chunkInts]());
    if (!buffer) {
        std::fclose(f);
        throw std::runtime_error("No se pudo reservar memoria para inicializar el archivo temporal.");
    }

    std::size_t remaining = totalInts;
    while (remaining > 0) {
        std::size_t toWrite = std::min(remaining, chunkInts);
        std::size_t written = std::fwrite(buffer.get(), sizeof(int), toWrite, f);
        if (written != toWrite) {
            std::fclose(f);
            throw std::runtime_error("No se pudo inicializar completamente el archivo temporal.");
        }
        remaining -= toWrite;
    }

    std::fclose(f);
}

void radixSort(PagedArray& arr, std::size_t n) {
    if (n < 2) return;

    if (g_pageSizeBytes == 0 || g_pageCount == 0) {
        throw std::runtime_error("RadixSort requiere que g_pageSizeBytes y g_pageCount estén inicializados.");
    }

    const std::size_t auxPageSizeBytes = std::max(g_pageSizeBytes * 4, static_cast<std::size_t>(4096));
    const std::size_t auxPageCount = std::max(g_pageCount * 4, static_cast<std::size_t>(16));

    createZeroFilledBinaryFile(g_radixTempPath, n);

    const std::size_t base = 256;
    std::size_t count[base];

    try {
        {
            PagedArray output(g_radixTempPath, auxPageSizeBytes, auxPageCount);

            for (std::size_t shift = 0; shift < 32; shift += 8) {
                for (std::size_t i = 0; i < base; ++i) count[i] = 0;

                for (std::size_t i = 0; i < n; ++i) {
                    std::uint32_t key = static_cast<std::uint32_t>(arr[i]) ^ 0x80000000u;
                    ++count[(key >> shift) & 0xFFu];
                }

                for (std::size_t i = 1; i < base; ++i) {
                    count[i] += count[i - 1];
                }

                for (std::size_t i = n; i-- > 0;) {
                    int value = arr[i];
                    std::uint32_t key = static_cast<std::uint32_t>(value) ^ 0x80000000u;
                    std::size_t bucket = (key >> shift) & 0xFFu;
                    output[--count[bucket]] = value;
                }

                for (std::size_t i = 0; i < n; ++i) {
                    arr[i] = output[i];
                }
            }

            output.flushAll();
        }
    }
    catch (...) {
        std::remove(g_radixTempPath.c_str());
        throw;
    }

    std::remove(g_radixTempPath.c_str());
}
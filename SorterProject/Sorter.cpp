#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

using namespace std;

#include "PagedArray.h"
#include "Sort Algorithms/sorting.h"

size_t g_pageSizeBytes = 0;
size_t g_pageCount = 0;
string g_radixTempPath = "radix_aux.tmp";

enum class Algorithm {
    Merge,
    Counting,
    Radix,
    Quick,
    Bucket
};

static string toUpper(string s) {
    for (char& c : s) {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool parseAlgorithm(const string& raw, Algorithm& out) {
    string s = toUpper(raw);
    if (s == "MERGE" || s == "MERGED" || s == "MERGESORT") { out = Algorithm::Merge; return true; }
    if (s == "COUNTING" || s == "COUNTINGSORT") { out = Algorithm::Counting; return true; }
    if (s == "RADIX" || s == "RADIXSORT") { out = Algorithm::Radix; return true; }
    if (s == "QUICK" || s == "QUICKSORT") { out = Algorithm::Quick; return true; }
    if (s == "BUCKET" || s == "BUCKETSORT") { out = Algorithm::Bucket; return true; }
    return false;
}

static const char* algorithmName(Algorithm alg) {
    switch (alg) {
    case Algorithm::Merge: return "merge";
    case Algorithm::Counting: return "counting";
    case Algorithm::Radix: return "radix";
    case Algorithm::Quick: return "quick";
    case Algorithm::Bucket: return "bucket";
    }
    return "unknown";
}

static void printUsage(const char* prog) {
    cerr
        << "Uso:\n  " << prog
        << " -input <INPUT FILE PATH> -output <OUTPUT FILE PATH> -alg <ALGORITMO> -pageSize <PAGE-SIZE-BYTES> -pageCount <PAGE-COUNT>\n\n"
        << "Algoritmos soportados: merge, counting, radix, quick, bucket\n"
        << "pageSize debe ser en bytes y multiplo de 4.\n";
}

static bool copyBinaryFile(const string& inputPath, const string& outputPath) {
    if (inputPath == outputPath) {
        cerr << "Error: input y output no pueden ser la misma ruta.\n";
        return false;
    }

    ifstream in(inputPath, ios::binary);
    if (!in) {
        cerr << "Error: no se pudo abrir el archivo de entrada.\n";
        return false;
    }

    ofstream out(outputPath, ios::binary | ios::trunc);
    if (!out) {
        cerr << "Error: no se pudo crear el archivo de salida.\n";
        return false;
    }

    const size_t bufferSize = 4 * 1024 * 1024;
    unique_ptr<char[]> buffer(new (nothrow) char[bufferSize]);
    if (!buffer) {
        cerr << "Error: no se pudo reservar memoria para copiar el archivo.\n";
        return false;
    }

    while (in) {
        in.read(buffer.get(), static_cast<streamsize>(bufferSize));
        streamsize readBytes = in.gcount();
        if (readBytes > 0) {
            out.write(buffer.get(), readBytes);
            if (!out) {
                cerr << "Error: fallo al escribir el archivo copiado.\n";
                return false;
            }
        }
    }

    return true;
}

static bool writeAsciiFromBinary(const string& binaryPath, const string& asciiPath) {
    ifstream in(binaryPath, ios::binary);
    if (!in) {
        cerr << "Error: no se pudo abrir el binario para generar el archivo legible.\n";
        return false;
    }
    ofstream out(asciiPath, ios::out | ios::trunc);
    if (!out) {
        cerr << "Error: no se pudo crear el archivo legible.\n";
        return false;
    }

    const size_t bufferSize = 8192;
    unique_ptr<int[]> buffer(new (nothrow) int[bufferSize]);
    if (!buffer) {
        cerr << "Error: no se pudo reservar memoria para buffer de escritura.\n";
        return false;
    }

    bool first = true;
    while (in) {
        in.read(reinterpret_cast<char*>(buffer.get()), bufferSize * sizeof(int));
        streamsize count = in.gcount() / sizeof(int);
        for (streamsize i = 0; i < count; ++i) {
            if (!first) out << ",";
            out << buffer[i];
            first = false;
        }
    }
    return true;
}

static void sortPagedArray(PagedArray& arr, Algorithm alg) {
    const size_t n = arr.size();
    if (n < 2) return;

    switch (alg) {
    case Algorithm::Merge:
        mergeSort(arr, 0, n - 1);
        break;
    case Algorithm::Counting:
        countingSort(arr, n);
        break;
    case Algorithm::Radix:
        radixSort(arr, n);
        break;
    case Algorithm::Quick:
        quickSort(arr, 0, n - 1);
        break;
    case Algorithm::Bucket:
        bucketSort(arr, n);
        break;
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 9) {
            printUsage(argv[0]);
            return 1;
        }

        string inputPath;
        string outputPath;
        string algRaw;
        size_t pageSize = 0;
        size_t pageCount = 0;

        for (int i = 1; i < argc; ++i) {
            string arg = argv[i];

            auto takeValue = [&](string& dest) -> bool {
                if (i + 1 >= argc) return false;
                dest = argv[++i];
                return true;
                };

            if (arg == "-input" || arg == "--input") {
                if (!takeValue(inputPath)) { cerr << "Falta valor para -input.\n"; return 1; }
            }
            else if (arg.rfind("-input=", 0) == 0 || arg.rfind("--input=", 0) == 0) {
                inputPath = arg.substr(arg.find('=') + 1);
            }
            else if (arg == "-output" || arg == "--output") {
                if (!takeValue(outputPath)) { cerr << "Falta valor para -output.\n"; return 1; }
            }
            else if (arg.rfind("-output=", 0) == 0 || arg.rfind("--output=", 0) == 0) {
                outputPath = arg.substr(arg.find('=') + 1);
            }
            else if (arg == "-alg" || arg == "--alg") {
                if (!takeValue(algRaw)) { cerr << "Falta valor para -alg.\n"; return 1; }
            }
            else if (arg.rfind("-alg=", 0) == 0 || arg.rfind("--alg=", 0) == 0) {
                algRaw = arg.substr(arg.find('=') + 1);
            }
            else if (arg == "-pageSize" || arg == "--pageSize") {
                string tmp;
                if (!takeValue(tmp)) { cerr << "Falta valor para -pageSize.\n"; return 1; }
                pageSize = static_cast<size_t>(stoull(tmp));
            }
            else if (arg.rfind("-pageSize=", 0) == 0 || arg.rfind("--pageSize=", 0) == 0) {
                pageSize = static_cast<size_t>(stoull(arg.substr(arg.find('=') + 1)));
            }
            else if (arg == "-pageCount" || arg == "--pageCount") {
                string tmp;
                if (!takeValue(tmp)) { cerr << "Falta valor para -pageCount.\n"; return 1; }
                pageCount = static_cast<size_t>(stoull(tmp));
            }
            else if (arg.rfind("-pageCount=", 0) == 0 || arg.rfind("--pageCount=", 0) == 0) {
                pageCount = static_cast<size_t>(stoull(arg.substr(arg.find('=') + 1)));
            }
        }

        if (inputPath.empty() || outputPath.empty() || algRaw.empty() || pageSize == 0 || pageCount == 0) {
            printUsage(argv[0]);
            return 1;
        }

        Algorithm alg;
        if (!parseAlgorithm(algRaw, alg)) {
            cerr << "Algoritmo no reconocido: " << algRaw << "\n";
            printUsage(argv[0]);
            return 1;
        }

        g_pageSizeBytes = pageSize;
        g_pageCount = pageCount;
        g_radixTempPath = outputPath + ".radix.tmp";

        auto start = chrono::steady_clock::now();

        if (!copyBinaryFile(inputPath, outputPath)) {
            return 1;
        }

        PagedArray::resetGlobalStats();

        {
            PagedArray arr(outputPath, pageSize, pageCount);
            sortPagedArray(arr, alg);
            arr.flushAll();
        }

        string asciiPath = outputPath + ".txt";
        if (!writeAsciiFromBinary(outputPath, asciiPath)) {
            return 1;
        }

        auto end = chrono::steady_clock::now();
        chrono::duration<double> elapsed = end - start;

        cout << "Resumen final\n";
        cout << "Algoritmo: " << algorithmName(alg) << "\n";
        cout << "Tiempo durado: " << elapsed.count() << " s\n";
        cout << "Page faults: " << PagedArray::totalPageFaults << "\n";
        cout << "Page hits: " << PagedArray::totalPageHits << "\n";
        cout << "Binario ordenado: " << outputPath << "\n";
        cout << "Legible: " << asciiPath << "\n";
        return 0;
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

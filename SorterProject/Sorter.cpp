#define _CRT_SECURE_NO_WARNINGS
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

class PagedArray {
public:
    class Reference {
    public:
        Reference(PagedArray* parent, std::size_t index) : parent_(parent), index_(index) {}

        Reference& operator=(int value) {
            parent_->set(index_, value);
            return *this;
        }

        Reference& operator=(const Reference& other) {
            return (*this = static_cast<int>(other));
        }

        operator int() const {
            return parent_->get(index_);
        }

        friend void swap(Reference a, Reference b) {
            int tmp = static_cast<int>(a);
            a = static_cast<int>(b);
            b = tmp;
        }

    private:
        PagedArray* parent_;
        std::size_t index_;
    };

    PagedArray(const std::string& path, std::size_t pageSizeInts, std::size_t pageCount)
        : path_(path),
          file_(nullptr),
          pageSize_(pageSizeInts),
          pageCount_(pageCount),
          totalInts_(0),
          totalPages_(0),
          hits_(0),
          faults_(0),
          tick_(0) {

        if (pageSize_ == 0 || pageCount_ == 0) {
            throw std::runtime_error("pageSize y pageCount deben ser mayores que cero.");
        }

        file_ = std::fopen(path_.c_str(), "r+b");
        if (!file_) {
            throw std::runtime_error("No se pudo abrir el archivo binario de trabajo: " + path_);
        }

        totalInts_ = fileIntCount();
        totalPages_ = (totalInts_ + pageSize_ - 1) / pageSize_;

        frames_ = new PageFrame[pageCount_];
        for (std::size_t i = 0; i < pageCount_; ++i) {
            frames_[i].data = new int[pageSize_];
            frames_[i].loaded = false;
            frames_[i].dirty = false;
            frames_[i].pageNo = invalidPage();
            frames_[i].validCount = 0;
            frames_[i].lastUsed = 0;
        }

        if (totalInts_ > 0) {
        }
    }

    ~PagedArray() {
        try {
            flushAll();
        } catch (...) {
        }

        if (file_) {
            std::fclose(file_);
            file_ = nullptr;
        }

        if (frames_) {
            for (std::size_t i = 0; i < pageCount_; ++i) {
                delete[] frames_[i].data;
                frames_[i].data = nullptr;
            }
            delete[] frames_;
            frames_ = nullptr;
        }
    }

    Reference operator[](std::size_t index) {
        checkIndex(index);
        return Reference(this, index);
    }

    int operator[](std::size_t index) const {
        checkIndex(index);
        return const_cast<PagedArray*>(this)->get(index);
    }

    std::size_t size() const {
        return totalInts_;
    }

    std::uint64_t pageHits() const { return hits_; }
    std::uint64_t pageFaults() const { return faults_; }

    void flushAll() {
        if (!file_ || !frames_) return;
        for (std::size_t i = 0; i < pageCount_; ++i) {
            if (frames_[i].loaded && frames_[i].dirty) {
                writeFrameToDisk(frames_[i]);
                frames_[i].dirty = false;
            }
        }
        std::fflush(file_);
    }

private:
    struct PageFrame {
        int* data;
        bool loaded;
        bool dirty;
        std::size_t pageNo;
        std::size_t validCount;
        std::uint64_t lastUsed;
    };

    static constexpr std::size_t invalidPage() {
        return static_cast<std::size_t>(-1);
    }

    void checkIndex(std::size_t index) const {
        if (index >= totalInts_) {
            throw std::out_of_range("Indice fuera de rango en PagedArray.");
        }
    }

    std::size_t fileIntCount() {
        if (std::fseek(file_, 0, SEEK_END) != 0) {
            throw std::runtime_error("No se pudo posicionar al final del archivo.");
        }
        long fileSize = std::ftell(file_);
        if (fileSize < 0) {
            throw std::runtime_error("No se pudo obtener el tamaño del archivo.");
        }
        if (std::fseek(file_, 0, SEEK_SET) != 0) {
            throw std::runtime_error("No se pudo regresar al inicio del archivo.");
        }

        if (fileSize % static_cast<long>(sizeof(int)) != 0) {
            throw std::runtime_error("El archivo no tiene un tamaño múltiplo de 4 bytes.");
        }

        return static_cast<std::size_t>(fileSize / static_cast<long>(sizeof(int)));
    }

    PageFrame* findFrame(std::size_t pageNo) {
        for (std::size_t i = 0; i < pageCount_; ++i) {
            if (frames_[i].loaded && frames_[i].pageNo == pageNo) {
                return &frames_[i];
            }
        }
        return nullptr;
    }

    PageFrame* chooseVictimFrame() {
        for (std::size_t i = 0; i < pageCount_; ++i) {
            if (!frames_[i].loaded) {
                return &frames_[i];
            }
        }

        PageFrame* victim = &frames_[0];
        for (std::size_t i = 1; i < pageCount_; ++i) {
            if (frames_[i].lastUsed < victim->lastUsed) {
                victim = &frames_[i];
            }
        }
        return victim;
    }

    void loadFrameFromDisk(PageFrame& frame, std::size_t pageNo) {
        const std::size_t start = pageNo * pageSize_;
        const std::size_t remaining = totalInts_ - start;
        frame.validCount = (remaining < pageSize_) ? remaining : pageSize_;
        frame.pageNo = pageNo;

        long offsetBytes = static_cast<long>(start * sizeof(int));
        if (std::fseek(file_, offsetBytes, SEEK_SET) != 0) {
            throw std::runtime_error("No se pudo posicionar el archivo para leer una pagina.");
        }

        std::size_t readCount = std::fread(frame.data, sizeof(int), frame.validCount, file_);
        if (readCount != frame.validCount) {
            throw std::runtime_error("No se pudo leer la pagina completa desde disco.");
        }

        frame.loaded = true;
        frame.dirty = false;
        frame.lastUsed = ++tick_;
    }

    void writeFrameToDisk(PageFrame& frame) {
        if (!frame.loaded || !frame.dirty) return;

        const std::size_t start = frame.pageNo * pageSize_;
        long offsetBytes = static_cast<long>(start * sizeof(int));
        if (std::fseek(file_, offsetBytes, SEEK_SET) != 0) {
            throw std::runtime_error("No se pudo posicionar el archivo para escribir una pagina.");
        }

        std::size_t written = std::fwrite(frame.data, sizeof(int), frame.validCount, file_);
        if (written != frame.validCount) {
            throw std::runtime_error("No se pudo escribir la pagina completa en disco.");
        }

        frame.dirty = false;
    }

    int access(std::size_t index, bool write, int value) {
        checkIndex(index);

        const std::size_t pageNo = index / pageSize_;
        const std::size_t offset = index % pageSize_;

        PageFrame* frame = findFrame(pageNo);
        if (frame) {
            ++hits_;
            frame->lastUsed = ++tick_;
            if (write) {
                frame->data[offset] = value;
                frame->dirty = true;
            }
            return frame->data[offset];
        }

        ++faults_;
        PageFrame* victim = chooseVictimFrame();

        if (victim->loaded && victim->dirty) {
            writeFrameToDisk(*victim);
        }

        loadFrameFromDisk(*victim, pageNo);

        if (write) {
            victim->data[offset] = value;
            victim->dirty = true;
        }

        victim->lastUsed = ++tick_;
        return victim->data[offset];
    }

    int get(std::size_t index) {
        return access(index, false, 0);
    }

    void set(std::size_t index, int value) {
        (void)access(index, true, value);
    }

    std::string path_;
    FILE* file_;
    std::size_t pageSize_;
    std::size_t pageCount_;
    std::size_t totalInts_;
    std::size_t totalPages_;
    std::uint64_t hits_;
    std::uint64_t faults_;
    std::uint64_t tick_;
    PageFrame* frames_{nullptr};
};

enum class Algorithm {
    Bubble,
    Insertion,
    Selection,
    Quick,
    Heap
};

static std::string toUpper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool parseAlgorithm(const std::string& raw, Algorithm& out) {
    std::string s = toUpper(raw);
    if (s == "BUBBLE" || s == "BUBBLESORT") { out = Algorithm::Bubble; return true; }
    if (s == "INSERTION" || s == "INSERTIONSORT") { out = Algorithm::Insertion; return true; }
    if (s == "SELECTION" || s == "SELECTIONSORT") { out = Algorithm::Selection; return true; }
    if (s == "QUICK" || s == "QUICKSORT") { out = Algorithm::Quick; return true; }
    if (s == "HEAP" || s == "HEAPSORT") { out = Algorithm::Heap; return true; }
    return false;
}

static const char* algorithmName(Algorithm alg) {
    switch (alg) {
    case Algorithm::Bubble: return "bubble";
    case Algorithm::Insertion: return "insertion";
    case Algorithm::Selection: return "selection";
    case Algorithm::Quick: return "quick";
    case Algorithm::Heap: return "heap";
    }
    return "unknown";
}

static void printUsage(const char* prog) {
    std::cerr
        << "Uso:\n  " << prog
        << " -input <INPUT FILE PATH> -output <OUTPUT FILE PATH> -alg <ALGORITMO> -pageSize <PAGE-SIZE> -pageCount <PAGE-COUNT>\n\n"
        << "Algoritmos soportados: bubble, insertion, selection, quick, heap\n";
}

static bool copyBinaryFile(const std::string& inputPath, const std::string& outputPath) {
    if (inputPath == outputPath) {
        std::cerr << "Error: input y output no pueden ser la misma ruta.\n";
        return false;
    }

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "Error: no se pudo abrir el archivo de entrada.\n";
        return false;
    }

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "Error: no se pudo crear el archivo de salida.\n";
        return false;
    }

    const std::size_t bufferSize = 4 * 1024 * 1024;
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[bufferSize]);
    if (!buffer) {
        std::cerr << "Error: no se pudo reservar memoria para copiar el archivo.\n";
        return false;
    }

    while (in) {
        in.read(buffer.get(), static_cast<std::streamsize>(bufferSize));
        std::streamsize readBytes = in.gcount();
        if (readBytes > 0) {
            out.write(buffer.get(), readBytes);
            if (!out) {
                std::cerr << "Error: fallo al escribir el archivo copiado.\n";
                return false;
            }
        }
    }

    return true;
}

static bool writeAsciiFromBinary(const std::string& binaryPath, const std::string& asciiPath) {
    std::ifstream in(binaryPath, std::ios::binary);
    if (!in) {
        std::cerr << "Error: no se pudo abrir el binario para generar el archivo legible.\n";
        return false;
    }

    std::ofstream out(asciiPath, std::ios::out | std::ios::trunc);
    if (!out) {
        std::cerr << "Error: no se pudo crear el archivo legible.\n";
        return false;
    }

    int value = 0;
    bool first = true;
    while (in.read(reinterpret_cast<char*>(&value), sizeof(int))) {
        if (!first) {
            out << ",";
        }
        out << value;
        first = false;
    }

    return true;
}

static void bubbleSort(PagedArray& arr) {
    const std::size_t n = arr.size();
    if (n < 2) return;

    for (std::size_t end = n; end > 1; --end) {
        bool swapped = false;
        for (std::size_t i = 1; i < end; ++i) {
            if (arr[i - 1] > arr[i]) {
                using std::swap;
                swap(arr[i - 1], arr[i]);
                swapped = true;
            }
        }
        if (!swapped) break;
    }
}

static void insertionSort(PagedArray& arr) {
    const std::size_t n = arr.size();
    for (std::size_t i = 1; i < n; ++i) {
        int key = arr[i];
        std::size_t j = i;
        while (j > 0 && arr[j - 1] > key) {
            arr[j] = arr[j - 1];
            --j;
        }
        arr[j] = key;
    }
}

static void selectionSort(PagedArray& arr) {
    const std::size_t n = arr.size();
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t minIndex = i;
        for (std::size_t j = i + 1; j < n; ++j) {
            if (arr[j] < arr[minIndex]) {
                minIndex = j;
            }
        }
        if (minIndex != i) {
            using std::swap;
            swap(arr[i], arr[minIndex]);
        }
    }
}

static std::size_t partition(PagedArray& arr, std::size_t low, std::size_t high) {
    int pivot = arr[high];
    std::size_t i = low;

    for (std::size_t j = low; j < high; ++j) {
        if (arr[j] <= pivot) {
            using std::swap;
            swap(arr[i], arr[j]);
            ++i;
        }
    }

    using std::swap;
    swap(arr[i], arr[high]);
    return i;
}

static void quickSort(PagedArray& arr, std::size_t low, std::size_t high) {
    if (low >= high) return;

    std::size_t p = partition(arr, low, high);
    if (p > 0) {
        quickSort(arr, low, p - 1);
    }
    quickSort(arr, p + 1, high);
}

static void heapify(PagedArray& arr, std::size_t n, std::size_t i) {
    while (true) {
        std::size_t largest = i;
        std::size_t left = 2 * i + 1;
        std::size_t right = 2 * i + 2;

        if (left < n && arr[left] > arr[largest]) {
            largest = left;
        }
        if (right < n && arr[right] > arr[largest]) {
            largest = right;
        }

        if (largest == i) {
            break;
        }

        using std::swap;
        swap(arr[i], arr[largest]);
        i = largest;
    }
}

static void heapSort(PagedArray& arr) {
    const std::size_t n = arr.size();
    if (n < 2) return;

    for (std::size_t i = n / 2; i > 0; --i) {
        heapify(arr, n, i - 1);
    }

    for (std::size_t end = n; end > 1; --end) {
        using std::swap;
        swap(arr[0], arr[end - 1]);
        heapify(arr, end - 1, 0);
    }
}

static void sortPagedArray(PagedArray& arr, Algorithm alg) {
    const std::size_t n = arr.size();
    if (n < 2) return;

    switch (alg) {
    case Algorithm::Bubble:
        bubbleSort(arr);
        break;
    case Algorithm::Insertion:
        insertionSort(arr);
        break;
    case Algorithm::Selection:
        selectionSort(arr);
        break;
    case Algorithm::Quick:
        quickSort(arr, 0, n - 1);
        break;
    case Algorithm::Heap:
        heapSort(arr);
        break;
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 9) {
            printUsage(argv[0]);
            return 1;
        }

        std::string inputPath;
        std::string outputPath;
        std::string algRaw;
        std::size_t pageSize = 0;
        std::size_t pageCount = 0;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            auto takeValue = [&](std::string& dest) -> bool {
                if (i + 1 >= argc) return false;
                dest = argv[++i];
                return true;
            };

            if (arg == "-input" || arg == "--input") {
                if (!takeValue(inputPath)) {
                    std::cerr << "Falta valor para -input.\n";
                    return 1;
                }
            } else if (arg.rfind("-input=", 0) == 0 || arg.rfind("--input=", 0) == 0) {
                std::size_t pos = arg.find('=');
                inputPath = arg.substr(pos + 1);
            } else if (arg == "-output" || arg == "--output") {
                if (!takeValue(outputPath)) {
                    std::cerr << "Falta valor para -output.\n";
                    return 1;
                }
            } else if (arg.rfind("-output=", 0) == 0 || arg.rfind("--output=", 0) == 0) {
                std::size_t pos = arg.find('=');
                outputPath = arg.substr(pos + 1);
            } else if (arg == "-alg" || arg == "--alg") {
                if (!takeValue(algRaw)) {
                    std::cerr << "Falta valor para -alg.\n";
                    return 1;
                }
            } else if (arg.rfind("-alg=", 0) == 0 || arg.rfind("--alg=", 0) == 0) {
                std::size_t pos = arg.find('=');
                algRaw = arg.substr(pos + 1);
            } else if (arg == "-pageSize" || arg == "--pageSize") {
                std::string tmp;
                if (!takeValue(tmp)) {
                    std::cerr << "Falta valor para -pageSize.\n";
                    return 1;
                }
                pageSize = static_cast<std::size_t>(std::stoull(tmp));
            } else if (arg.rfind("-pageSize=", 0) == 0 || arg.rfind("--pageSize=", 0) == 0) {
                std::size_t pos = arg.find('=');
                pageSize = static_cast<std::size_t>(std::stoull(arg.substr(pos + 1)));
            } else if (arg == "-pageCount" || arg == "--pageCount") {
                std::string tmp;
                if (!takeValue(tmp)) {
                    std::cerr << "Falta valor para -pageCount.\n";
                    return 1;
                }
                pageCount = static_cast<std::size_t>(std::stoull(tmp));
            } else if (arg.rfind("-pageCount=", 0) == 0 || arg.rfind("--pageCount=", 0) == 0) {
                std::size_t pos = arg.find('=');
                pageCount = static_cast<std::size_t>(std::stoull(arg.substr(pos + 1)));
            }
        }

        if (inputPath.empty() || outputPath.empty() || algRaw.empty() || pageSize == 0 || pageCount == 0) {
            printUsage(argv[0]);
            return 1;
        }

        Algorithm alg;
        if (!parseAlgorithm(algRaw, alg)) {
            std::cerr << "Algoritmo no reconocido: " << algRaw << "\n";
            printUsage(argv[0]);
            return 1;
        }

        auto start = std::chrono::steady_clock::now();

        if (!copyBinaryFile(inputPath, outputPath)) {
            return 1;
        }

        std::uint64_t hits = 0;
        std::uint64_t faults = 0;

        {
            PagedArray arr(outputPath, pageSize, pageCount);
            sortPagedArray(arr, alg);
            arr.flushAll();
            hits = arr.pageHits();
            faults = arr.pageFaults();
        }

        std::string asciiPath = outputPath + ".txt";
        if (!writeAsciiFromBinary(outputPath, asciiPath)) {
            return 1;
        }

        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        std::cout << "Resumen final\n";
        std::cout << "Algoritmo: " << algorithmName(alg) << "\n";
        std::cout << "Tiempo durado: " << elapsed.count() << " s\n";
        std::cout << "Page faults: " << faults << "\n";
        std::cout << "Page hits: " << hits << "\n";
        std::cout << "Binario ordenado: " << outputPath << "\n";
        std::cout << "Legible: " << asciiPath << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

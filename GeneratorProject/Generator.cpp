#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <cstdint>
#include <memory>
#include <chrono>
#include <cstring>

enum class SizeLabel { SMALL, MEDIUM, LARGE };

static bool parse_size_label(const std::string& s, SizeLabel& out) {
    if (s == "SMALL" || s == "--size=SMALL" || s == "-size=SMALL") { out = SizeLabel::SMALL; return true; }
    if (s == "MEDIUM" || s == "--size=MEDIUM" || s == "-size=MEDIUM") { out = SizeLabel::MEDIUM; return true; }
    if (s == "LARGE" || s == "--size=LARGE" || s == "-size=LARGE") { out = SizeLabel::LARGE; return true; }
    return false;
}

static uint64_t bytes_for_label(SizeLabel label) {
    const uint64_t MB = 1024ull * 1024ull;
    switch (label) {
    case SizeLabel::SMALL:  return 4ull * MB;
    case SizeLabel::MEDIUM: return 512ull * MB;
    case SizeLabel::LARGE:  return 1ull * 1024ull * MB; 
    }
    return 0;
}

void print_usage(const char* prog) {
    std::cerr << "Usage:\n"
        << "  " << prog << " --size <SMALL|MEDIUM|LARGE> --output <OUTPUT FILE PATH>\n\n"
        << "Examples:\n"
        << "  " << prog << " --size SMALL --output /tmp/test.bin\n";
}


bool generate_random_binary(const std::string& output_path, SizeLabel label) {
    const uint64_t total_bytes = bytes_for_label(label);
    if (total_bytes == 0) return false;
    if (total_bytes % sizeof(int32_t) != 0) {
        std::cerr << "Error: tamaño total no divisible por 4 bytes.\n";
        return false;
    }
    const uint64_t total_ints = total_bytes / sizeof(int32_t);

    const uint64_t buffer_ints = 1ull << 20;
    const uint64_t ints_per_write = (buffer_ints > total_ints) ? total_ints : buffer_ints;

    std::unique_ptr<int32_t[]> buffer(new(std::nothrow) int32_t[ints_per_write]);
    if (!buffer) {
        std::cerr << "Error: no se pudo reservar memoria para el buffer.\n";
        return false;
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int32_t> dist(std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int32_t>::max());

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: no se pudo abrir el archivo de salida: " << output_path << "\n";
        return false;
    }

    auto start = std::chrono::steady_clock::now();
    uint64_t remaining = total_ints;
    uint64_t written_ints = 0;
    const uint64_t progress_interval = std::max<uint64_t>(1, total_ints / 100); // para progreso en %
    while (remaining > 0) {
        uint64_t to_fill = (remaining > ints_per_write) ? ints_per_write : remaining;
        for (uint64_t i = 0; i < to_fill; ++i) buffer[i] = dist(rng);
        out.write(reinterpret_cast<const char*>(buffer.get()), static_cast<std::streamsize>(to_fill * sizeof(int32_t)));
        if (!out) {
            std::cerr << "Error: fallo al escribir en disco.\n";
            out.close();
            return false;
        }

        remaining -= to_fill;
        written_ints += to_fill;

        if (written_ints % progress_interval == 0 || remaining == 0) {
            double perc = (100.0 * written_ints) / total_ints;
            std::cerr << "\rGenerando: " << static_cast<int>(perc) << "% (" << written_ints << " / " << total_ints << " ints) " << std::flush;
        }
    }
    out.close();
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cerr << "\rGenerado " << total_bytes << " bytes (" << total_ints << " enteros) en ";
    std::cerr << elapsed.count() << " s.                   \n";
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        print_usage(argv[0]);
        return 1;
    }

    std::string size_arg, output_arg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--size" || a == "-size") {
            if (i + 1 < argc) { size_arg = argv[++i]; }
        }
        else if (a.rfind("--size=", 0) == 0) {
            size_arg = a.substr(7);
        }
        else if (a == "--output" || a == "-output") {
            if (i + 1 < argc) { output_arg = argv[++i]; }
        }
        else if (a.rfind("--output=", 0) == 0) {
            output_arg = a.substr(9);
        }
    }

    if (size_arg.empty() || output_arg.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    SizeLabel label;
    if (!parse_size_label(size_arg, label)) {
        std::cerr << "Error: tamaño no reconocido. Use SMALL, MEDIUM o LARGE.\n";
        return 1;
    }

    bool ok = generate_random_binary(output_arg, label);
    if (!ok) {
        std::cerr << "Fallo en la generación del archivo.\n";
        return 1;
    }
    return 0;
}
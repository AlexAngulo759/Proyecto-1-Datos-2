#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <cstdint>
#include <memory>
#include <chrono>
#include <cstring>

using namespace std;

enum class SizeLabel { SMALL, MEDIUM, LARGE };

static bool parse_size_label(const string& s, SizeLabel& out) {
    if (s == "SMALL" || s == "--size=SMALL" || s == "-size=SMALL") { out = SizeLabel::SMALL; return true; }
    if (s == "MEDIUM" || s == "--size=MEDIUM" || s == "-size=MEDIUM") { out = SizeLabel::MEDIUM; return true; }
    if (s == "LARGE" || s == "--size=LARGE" || s == "-size=LARGE") { out = SizeLabel::LARGE; return true; }
    return false;
}

static uint64_t bytes_for_label(SizeLabel label) {
    const uint64_t MB = 1024ull * 1024ull;
    switch (label) {
    case SizeLabel::SMALL:  return 128ull * MB;
    case SizeLabel::MEDIUM: return 256ull * MB;
    case SizeLabel::LARGE:  return 512ull * MB;
    }
    return 0;
}

void print_usage(const char* prog) {
    cerr << "Usage:\n"
        << "  " << prog << " --size <SMALL|MEDIUM|LARGE> --output <OUTPUT FILE PATH>\n\n"
        << "Examples:\n"
        << "  " << prog << " --size SMALL --output /tmp/test.bin\n";
}


bool generate_random_binary(const string& output_path, SizeLabel label) {
    const uint64_t total_bytes = bytes_for_label(label);
    if (total_bytes == 0) return false;
    if (total_bytes % sizeof(int32_t) != 0) {
        cerr << "Error: tamaño total no divisible por 4 bytes.\n";
        return false;
    }
    const uint64_t total_ints = total_bytes / sizeof(int32_t);

    const uint64_t buffer_ints = 1ull << 20;
    const uint64_t ints_per_write = (buffer_ints > total_ints) ? total_ints : buffer_ints;

    unique_ptr<int32_t[]> buffer(new(nothrow) int32_t[ints_per_write]);
    if (!buffer) {
        cerr << "Error: no se pudo reservar memoria para el buffer.\n";
        return false;
    }

    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int32_t> dist(numeric_limits<int32_t>::min(),
        numeric_limits<int32_t>::max());

    ofstream out(output_path, ios::binary | ios::trunc);
    if (!out.is_open()) {
        cerr << "Error: no se pudo abrir el archivo de salida: " << output_path << "\n";
        return false;
    }

    auto start = chrono::steady_clock::now();
    uint64_t remaining = total_ints;
    uint64_t written_ints = 0;
    const uint64_t progress_interval = max<uint64_t>(1, total_ints / 100); // para progreso en %
    while (remaining > 0) {
        uint64_t to_fill = (remaining > ints_per_write) ? ints_per_write : remaining;
        for (uint64_t i = 0; i < to_fill; ++i) buffer[i] = dist(rng);
        out.write(reinterpret_cast<const char*>(buffer.get()), static_cast<streamsize>(to_fill * sizeof(int32_t)));
        if (!out) {
            cerr << "Error: fallo al escribir en disco.\n";
            out.close();
            return false;
        }

        remaining -= to_fill;
        written_ints += to_fill;

        if (written_ints % progress_interval == 0 || remaining == 0) {
            double perc = (100.0 * written_ints) / total_ints;
            cerr << "\rGenerando: " << static_cast<int>(perc) << "% (" << written_ints << " / " << total_ints << " ints) " << flush;
        }
    }
    out.close();
    auto end = chrono::steady_clock::now();
    chrono::duration<double> elapsed = end - start;

    cerr << "\rGenerado " << total_bytes << " bytes (" << total_ints << " enteros) en ";
    cerr << elapsed.count() << " s.                   \n";
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        print_usage(argv[0]);
        return 1;
    }

    string size_arg, output_arg;
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
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
        cerr << "Error: tamaño no reconocido. Use SMALL, MEDIUM o LARGE.\n";
        return 1;
    }

    bool ok = generate_random_binary(output_arg, label);
    if (!ok) {
        cerr << "Fallo en la generación del archivo.\n";
        return 1;
    }
    return 0;
}
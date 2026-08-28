#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <set>
#include <QCoreApplication>
#include <QImageReader>
#include <QDir>
#include <QFileInfo>

// ---------------------------------------------------------------------------
// Memory measurement helpers (Linux /proc/self)
// ---------------------------------------------------------------------------

static long readVmRSS() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmRSS:") == 0) {
            long val = 0;
            std::sscanf(line.c_str() + 6, "%ld", &val);
            return val;
        }
    }
    return -1;
}

static long readVmSize() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmSize:") == 0) {
            long val = 0;
            std::sscanf(line.c_str() + 7, "%ld", &val);
            return val;
        }
    }
    return -1;
}

static inline const char* unit(long kB) {
    if (kB >= 1048576) return "GB";
    if (kB >= 1024) return "MB";
    return "kB";
}

static inline double toUnit(long kB, const char*& u) {
    if (kB >= 1048576) { u = "GB"; return kB / 1048576.0; }
    if (kB >= 1024) { u = "MB"; return kB / 1024.0; }
    u = "kB"; return (double)kB;
}

// ---------------------------------------------------------------------------
// Image info for reporting
// ---------------------------------------------------------------------------

struct ImageInfo {
    std::string filename;
    int width = 0, height = 0;
    long rawPixelSizeKB = 0;   // w * h * bytesPerPixel / 1024
    long decodedSizeKB = 0;    // actual memory consumed by QImage (including alloc overhead)
};

// Calculate expected raw pixel size in kB for a given format
static long calcRawPixelSize(int width, int height, const QString& format) {
    int bytesPerPixel = 1; // default grayscale
    if (format == "rgb32" || format == "argb32") bytesPerPixel = 4;
    else if (format == "rgb888") bytesPerPixel = 3;
    else if (format == "rgb24") bytesPerPixel = 3;

    // Round up to KB
    long totalBytes = (long)width * height * bytesPerPixel;
    return (totalBytes + 1023) / 1024;
}

// ---------------------------------------------------------------------------
// Profiler: load a directory and measure memory at each step
// ---------------------------------------------------------------------------

struct LoadStats {
    // Before loading
    long baselineVmRSS = 0;       // kB
    long baselineVmSize = 0;      // kB

    // During loading (tracked per image)
    std::vector<ImageInfo> images;
    long peakVmRSS = 0;           // kB
    long peakVmSize = 0;          // kB
    double totalLoadTimeMs = 0.0;
    int skippedCount = 0;

    // After loading all
    long finalVmRSS = 0;          // kB
    long finalVmSize = 0;         // kB
    long totalRawPixelKB = 0;     // sum of raw pixel sizes
};

static QString getFormat(const QImage& img) {
    int fmt = img.format();
    if (fmt == static_cast<int>(QImage::Format_Mono)) return "mono";
    if (fmt == static_cast<int>(QImage::Format_RGB32)) return "rgb32";
    if (fmt == static_cast<int>(QImage::Format_ARGB32)) return "argb32";
    if (fmt == static_cast<int>(QImage::Format_ARGB32_Premultiplied)) return "argb32_pm";
    if (fmt == static_cast<int>(QImage::Format_RGB888)) return "rgb888";
    if (fmt == static_cast<int>(QImage::Format_Alpha8)) return "alpha8";
    return QString("%0").arg(fmt);
}

LoadStats profileDirectory(const QString& dirPath, int containerWidth = 0) {
    LoadStats stats;

    // Snapshot baseline
    stats.baselineVmRSS = readVmRSS();
    stats.baselineVmSize = readVmSize();

    auto start = std::chrono::steady_clock::now();

    QDir dir(dirPath);
    if (!dir.exists()) {
        std::cerr << "Error: directory does not exist: " << qPrintable(dirPath) << "\n";
        return stats;
    }

    dir.setFilter(QDir::Files | QDir::Readable);
    QStringList files = dir.entryList();
    std::sort(files.begin(), files.end());

    long maxRSS = stats.baselineVmRSS;
    long maxSize = stats.baselineVmSize;

    // Gather supported formats
    QList<QByteArray> formats = QImageReader::supportedImageFormats();
    std::set<QString> supported;
    for (const QByteArray& f : formats) {
        supported.insert(QString::fromLatin1(f).toLower());
        if (f == "jpeg") supported.insert("jpg");
    }

    // Pre-allocate images list to avoid reallocation overhead confusion
    std::vector<QImage> allImages;
    allImages.reserve(files.size() / 2);

    int imageIndex = 0;
    for (const QString& fileName : files) {
        QFileInfo fi(dir.filePath(fileName));
        QString suffix = fi.suffix().toLower();
        if (!supported.count(suffix)) {
            stats.skippedCount++;
            continue;
        }

        // Snapshot before loading this image
        long rssBefore = readVmRSS();

        QImage img;
        if (!img.load(fi.absoluteFilePath())) {
            stats.skippedCount++;
            imageIndex++;
            continue;
        }

        int w = img.width(), h = img.height();
        QString fmt = getFormat(img);
        long rawKB = calcRawPixelSize(w, h, fmt);

        // Scale if needed (store scaled dimensions)
        int finalW = w, finalH = h;
        if (containerWidth > 0 && w > containerWidth) {
            img = img.scaledToWidth(containerWidth, Qt::SmoothTransformation);
            finalW = img.width();
            finalH = img.height();
        }

        allImages.push_back(std::move(img));

        long rssAfter = readVmRSS();
        long sizeAfter = readVmSize();

        // Update peaks
        if (rssAfter > maxRSS) maxRSS = rssAfter;
        if (sizeAfter > maxSize) maxSize = sizeAfter;

        ImageInfo info;
        info.filename = fi.fileName().toStdString();
        info.width = finalW;
        info.height = finalH;
        info.rawPixelSizeKB = rawKB;
        // Approximate decoded overhead: RSS delta from this image load
        long decodedOverhead = rssAfter - rssBefore;
        info.decodedSizeKB = std::max(decodedOverhead, (long)rawKB);

        stats.images.push_back(info);
        imageIndex++;

        // Per-image progress output
        double pct = 100.0 * imageIndex / files.size();
        std::cout << "\r  [" << std::setw(3) << std::setfill(' ') << (int)pct << "%] "
                  << info.filename
                  << " (" << info.width << "x" << info.height << ", fmt=" << qPrintable(fmt)
                  << ", raw=" << info.rawPixelSizeKB << "kB)"
                  << "  RSS delta: +" << decodedOverhead << "kB";
        std::cout.flush();
    }

    // Add all images to current process memory (simulating the cache)
    auto startAlloc = std::chrono::steady_clock::now();

    long rssAfterAll = readVmRSS();
    if (rssAfterAll > maxRSS) maxRSS = rssAfterAll;
    long sizeAfterAll = readVmSize();
    if (sizeAfterAll > maxSize) maxSize = sizeAfterAll;

    // Insert all into the vector to keep them alive in memory
    for (auto& img : allImages) {
        stats.totalRawPixelKB += calcRawPixelSize(img.width(), img.height(), getFormat(img));
    }

    auto endAlloc = std::chrono::steady_clock::now();
    double allocMs = std::chrono::duration<double, std::milli>(endAlloc - startAlloc).count();

    stats.peakVmRSS = maxRSS;
    stats.peakVmSize = maxSize;
    stats.totalLoadTimeMs = (std::chrono::duration<double, std::milli>(startAlloc - start)).count();
    stats.finalVmRSS = readVmRSS();
    stats.finalVmSize = readVmSize();

    return stats;
}

// ---------------------------------------------------------------------------
// Report
// ---------------------------------------------------------------------------

static void printReport(const QString& dirPath, const LoadStats& s) {
    std::cout << "\n\n";
    std::cout << "============================================================\n";
    std::cout << "  Memory Profile: Image Directory Loader\n";
    std::cout << "============================================================\n";
    std::cout << "Directory : " << qPrintable(dirPath) << "\n";

    const char* u;
    double val = toUnit(s.baselineVmRSS, u);
    std::cout << "Baseline  : RSS=" << std::fixed << std::setprecision(2) << val << " " << u
              << ", Size=" << toUnit(s.baselineVmSize, u) << " " << u << "\n";

    // Image summary table
    if (!s.images.empty()) {
        std::cout << "\nImages (" << s.images.size() << "):\n";
        std::cout << std::left;
        std::cout << std::setw(40) << "Filename"
                  << std::setw(12) << "Dimensions"
                  << std::setw(8) << "Fmt"
                  << std::setw(10) << "Raw(kB)"
                  << std::setw(12) << "Decoded(kB)"
                  << "\n";
        std::cout << std::string(94, '-') << "\n";

        for (const auto& img : s.images) {
            std::cout << std::setw(40) << img.filename.substr(0, 38)
                      << std::setw(12) << "(" << img.width << "x" << img.height << ")"
                      << std::setw(8) << "?"
                      << std::setw(10) << img.rawPixelSizeKB
                      << std::setw(12) << img.decodedSizeKB;
            std::cout << "\n";
        }

        long totalRaw = s.totalRawPixelKB;
        val = toUnit(totalRaw, u);
        std::cout << std::string(94, '-') << "\n";
        std::cout << "Total     : raw pixel data=" << std::fixed << std::setprecision(2) << val << " " << u
                  << " (" << totalRaw << " kB)\n";

        // Peak memory
        long rssDelta = s.peakVmRSS - s.baselineVmRSS;
        long finalRSS = s.finalVmRSS - s.baselineVmRSS;
        std::cout << "\nPeak Memory:\n";
        val = toUnit(s.peakVmRSS, u);
        std::cout << "  Peak RSS   : " << std::fixed << std::setprecision(2) << val << " " << u
                  << " (+" << rssDelta << " kB from baseline)\n";

        val = toUnit(finalRSS, u);
        std::cout << "  Final RSS  : " << std::fixed << std::setprecision(2) << val << " " << u
                  << " (+" << finalRSS << " kB from baseline)\n";

        // Efficiency: how much of the RSS is actual pixel data vs overhead
        double overheadRatio = 1.0;
        if (finalRSS > 0 && totalRaw > 0) {
            overheadRatio = (double)(finalRSS - totalRaw) / finalRSS;
        }
        std::cout << "\nEfficiency:\n";
        std::cout << "  Pixel data   : " << toUnit(totalRaw, u) << " " << u
                  << " (" << std::fixed << std::setprecision(1) 
                  << (totalRaw > finalRSS ? 100.0 : 100.0 * totalRaw / finalRSS) << "% of RSS)\n";
        std::cout << "  Overhead     : " << toUnit(finalRSS - totalRaw, u) << " " << u
                  << " (" << std::fixed << std::setprecision(1) << (overheadRatio * 100.0) << "% of RSS)\n";

        // Speed
        double mbps = (totalRaw > 0) ? (totalRaw / s.totalLoadTimeMs) : 0;
        val = toUnit(totalRaw, u);
        std::cout << "\nSpeed:\n";
        std::cout << "  Total time   : " << std::fixed << std::setprecision(2) << s.totalLoadTimeMs << " ms\n";
        if (mbps > 0) {
            val = toUnit((long)(mbps * 1.0), u); // approximate MB/s
            std::cout << "  Throughput   : ~" << std::fixed << std::setprecision(2) << mbps << " kB/s\n";
        }

        if (s.skippedCount > 0) {
            std::cout << "\nSkipped files: " << s.skippedCount << "\n";
        }
    } else {
        std::cout << "No images found or supported.\n";
    }

    // Per-image memory breakdown (top consumers)
    if (!s.images.empty()) {
        auto sorted = s.images;
        std::sort(sorted.begin(), sorted.end(),
                  [](const ImageInfo& a, const ImageInfo& b) { return a.decodedSizeKB > b.decodedSizeKB; });

        std::cout << "\nTop 5 memory consumers:\n";
        int count = 0;
        for (const auto& img : sorted) {
            if (++count > 5) break;
            std::cout << "  #" << count << " " << img.filename
                      << ": decoded=" << img.decodedSizeKB << " kB, raw=" << img.rawPixelSizeKB << " kB\n";
        }
    }

    std::cout << "\n============================================================\n\n";
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);  // No GUI needed for profiler

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <directory> [containerWidth]\n", argv[0]);
        return 1;
    }

    QString dirPath = argv[1];
    int containerWidth = (argc > 2) ? std::atoi(argv[2]) : 0;

    LoadStats stats = profileDirectory(dirPath, containerWidth);
    printReport(dirPath, stats);

    return 0;
}

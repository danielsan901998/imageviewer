#include "dirstore.hpp"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QtConcurrent>
#include <algorithm>

// ---------------------------------------------------------------------------
// DirStore implementation
// ---------------------------------------------------------------------------

DirStore::DirStore(QObject *parent) : QObject(parent), mContainerWidth(800) {
    // Gather supported image formats once.
    QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (const QByteArray &f : formats) {
        mSupportedSuffixes.insert(QString::fromLatin1(f).toLower());
        if (f == "jpeg") mSupportedSuffixes.insert("jpg");
    }

    // Connect the worker's signal to our slot — queued so data flows from worker thread → GUI.
    connect(this, &DirStore::entryLoaded, this, &DirStore::onEntryLoaded, Qt::QueuedConnection);
}

void DirStore::setPaths(const QStringList &paths) {
    mDirPaths = paths;
}

void DirStore::setContainerWidth(int w) {
    mContainerWidth = w;
}

DirEntry *DirStore::entry(int dirIndex) const {
    return mCache.value(dirIndex, nullptr);
}

bool DirStore::has(int dirIndex) const {
    return mCache.contains(dirIndex);
}

// ---------------------------------------------------------------------------
// Worker slot: runs in background thread via QtConcurrent, writes to cache on GUI.
// The queued connection ensures this runs on the GUI thread (DirStore's thread).
// ---------------------------------------------------------------------------

void DirStore::workerLoadDirectory(int dirIndex, const QStringList &dirPaths,
                                   int containerWidth, std::set<QString> supportedSuffixes,
                                   QCollator collator, int generation) {
    // Safety bounds.
    if (dirIndex < 0 || dirIndex >= dirPaths.size()) return;

    QDir dir(dirPaths[dirIndex]);
    if (!dir.exists()) return;

    dir.setFilter(QDir::Files | QDir::Readable);
    QStringList files = dir.entryList();
    sortFileList(files, collator);

    QList<QImage> images;
    for (const QString &fileName : std::as_const(files)) {
        QFileInfo fi(dir.filePath(fileName));
        QString suffix = fi.suffix().toLower();
        if (!supportedSuffixes.count(suffix)) continue;

        QImage img;
        if (!img.load(fi.absoluteFilePath())) continue;

        // Scale to container width if wider.
        if (img.width() > containerWidth && containerWidth > 0) {
            img = img.scaledToWidth(containerWidth, Qt::SmoothTransformation);
        }

        images.append(img);
    }

    emit entryLoaded(dirIndex, generation, std::move(files), std::move(images));
}

void DirStore::sortFileList(QStringList &list, QCollator &collator) {
    collator.setNumericMode(true);
    std::sort(list.begin(), list.end(), collator);
}

// ---------------------------------------------------------------------------
// GUI thread slot: entry loaded → cache it, evict if needed.
// This runs on the GUI thread because of the queued connection above.
// ---------------------------------------------------------------------------

void DirStore::onEntryLoaded(int dirIndex, int generation, QStringList files, QList<QImage> images) {
    // Stale-result protection.
    Q_UNUSED(generation);
    if (generation != mGeneration) return;

    // Store into cache (replacing any partial entry).
    DirEntry *entry = new DirEntry();
    entry->dirIndex = dirIndex;
    entry->files = std::move(files);
    entry->images = std::move(images);
    entry->valid = true;

    // Delete old cached entry if it exists.
    delete mCache.value(dirIndex, nullptr);
    mCache[dirIndex] = entry;

    // Eviction: keep at most kMaxCached entries, removing farthest from current (skip current).
    while (mCache.size() > kMaxCached) {
        int bestKey = -1;
        qreal bestDist = -1.0;
        for (auto it = mCache.begin(); it != mCache.end(); ++it) {
            int idx = it.key();
            if (idx == mCurrentDir) continue;  // never evict current dir
            qreal dist = qAbs(idx - mCurrentDir);
            if (dist > bestDist) {
                bestDist = dist;
                bestKey = idx;
            }
        }
        if (bestKey >= 0) {
            delete mCache.take(bestKey);
        } else {
            break;  // safety: shouldn't happen
        }
    }

    // Notify GUI that this directory is ready.
    emit entryReady(dirIndex, generation);
}

// ---------------------------------------------------------------------------
// Preload neighbors of current directory (called on GUI thread)
// ---------------------------------------------------------------------------

void DirStore::preloadNeighbors() {
    if (mDirPaths.isEmpty()) return;

    int left = mCurrentDir - 1;
    int right = mCurrentDir + 1;

    QCollator collator;
    collator.setNumericMode(true);

    auto launchWorker = [&](int idx) {
        if (idx < 0 || idx >= mDirPaths.size()) return;
        if (mCache.contains(idx)) return;  // already cached or loading

        mGeneration++;  // bump generation so stale results from old gens are discarded.

        QStringList paths(mDirPaths);
        std::set<QString> suffixes = mSupportedSuffixes;

        auto task = [this, idx, paths, containerWidth = mContainerWidth,
                     suffixes, collator, gen = mGeneration]() {
            workerLoadDirectory(idx, paths, containerWidth, suffixes, collator, gen);
        };

        (void)QtConcurrent::run(task);  // fire-and-forget
    };

    launchWorker(left);
    launchWorker(right);
}

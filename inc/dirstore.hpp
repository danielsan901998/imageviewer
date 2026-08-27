#ifndef DIRSTORE_HPP
#define DIRSTORE_HPP

#include <QObject>
#include <QImage>
#include <QStringList>
#include <QHash>
#include <set>
#include <QCollator>

/// A single directory's worth of decoded images. Owned by DirStore on the GUI thread.
struct DirEntry {
    int dirIndex = -1;                // index into dirPaths
    QStringList files;                // sorted file names (numeric collator)
    QList<QImage> images;             // decoded + scaled to containerWidth
    bool valid = false;               // true once fully loaded
};

/// Thread-safe directory cache with sliding-window eviction and background preloading.
/// Lives on the GUI thread but spawns workers for I/O-heavy tasks (listing, decoding).
class DirStore : public QObject {
    Q_OBJECT

public:
    explicit DirStore(QObject *parent = nullptr);

    /// Called once at startup from the GUI thread to set all directory paths.
    void setPaths(const QStringList &dirPaths);

    /// Container width used for image scaling (set after window is sized).
    void setContainerWidth(int w);

    /// Return a pointer to the entry, or nullptr if not cached / evicted.
    DirEntry *entry(int dirIndex) const;

    /// Check if an entry exists in the cache.
    bool has(int dirIndex) const;

    /// The currently active directory index (set by caller after navigation).
    int currentDir() const { return mCurrentDir; }
    void setCurrentDir(int i) { mCurrentDir = i; }

    /// Return the list of all known directory paths.
    QStringList dirPaths() const { return mDirPaths; }

    /// Maximum number of cached entries (buffer window size).
    static constexpr int kMaxCached = 4;

signals:
    /// Internal: carries loaded data from worker to onEntryLoaded slot (queued).
    void entryLoaded(int dirIndex, int generation,
                     QStringList files, QList<QImage> images);

    /// External notification for main.cpp that a directory is ready.
    void entryReady(int dirIndex, int generation);

public slots:
    /// Handle a loaded entry on the GUI thread – cache it, evict if needed.
    void onEntryLoaded(int dirIndex, int generation, QStringList files, QList<QImage> images);

    /// Start preloading neighbors of currentDir (called after every navigation).
    void preloadNeighbors();

public:
    // Public accessor for main.cpp integration (cache is owned by DirStore on GUI thread).
    QHash<int, DirEntry *> &cache() { return mCache; }

private slots:
    /// Worker task run in a background thread via QtConcurrent: list + sort + decode.
    void workerLoadDirectory(int dirIndex, const QStringList &dirPaths,
                             int containerWidth, std::set<QString> supportedSuffixes,
                             QCollator collator, int generation);

private:
    /// Sort files using numeric collator (same logic as in main.cpp).
    static void sortFileList(QStringList &list, QCollator &collator);

    // --- State ---
    QStringList mDirPaths;
    int mCurrentDir = 0;
    int mGeneration = 0;                     // monotonically increasing
    int mContainerWidth = 800;               // scaled width for images
    std::set<QString> mSupportedSuffixes;

    /// Cache: dirIndex -> DirEntry*. Evicted entries are deleted.
    QHash<int, DirEntry *> mCache;

    /// Pixel budget cap (approx. 2–3× screen area). Used by eviction logic.
    static constexpr int kPixelBudget = 1920 * 1080 * 4; // ~6 MB raw pixels
};

#endif // DIRSTORE_HPP

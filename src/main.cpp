#include <QApplication>
#include <QMainWindow>
#include <QScrollArea>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QDebug>
#include <QCommandLineParser>
#include <QImageReader>
#include <QMessageBox>
#include <QCollator>
#include <qnamespace.h>
#include <set>

#include "autoscrollwidget.hpp"
#include "dirstore.hpp"

// ---------------------------------------------------------------------------
// Helpers (extracted from original inline lambdas)
// ---------------------------------------------------------------------------

/// Check if a suffix is supported by QImageReader.
static bool imageReaderSupported(const QString &suffix) {
    static std::set<QString> supported;
    static bool initialized = false;
    if (!initialized) {
        QList<QByteArray> formats = QImageReader::supportedImageFormats();
        for (const QByteArray &f : formats) {
            supported.insert(QString::fromLatin1(f).toLower());
            if (f == "jpeg") supported.insert("jpg");
        }
        initialized = true;
    }
    return supported.count(suffix.toLower()) > 0;
}

/// Sort files using numeric collator.
static void sortFileList(QStringList &list, QCollator &collator) {
    collator.setNumericMode(true);
    std::sort(list.begin(), list.end(), collator);
}

/// Build labels and pixmaps from a DirEntry's cached images.
/// Returns the number of images loaded (0 if none).
static int materializeFromCache(DirStore *store, int dirIndex,
                                QVBoxLayout *layout, QList<QLabel *> &labels,
                                QList<QPixmap> &pixmaps, int containerWidth) {
    DirEntry *entry = store->entry(dirIndex);
    if (!entry || !entry->valid || entry->images.isEmpty()) return 0;

    int loaded = 0;
    for (const QImage &img : std::as_const(entry->images)) {
        QPixmap pm = QPixmap::fromImage(img);
        QLabel *label = new QLabel();
        label->setPixmap(pm);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
        labels.append(label);
        pixmaps.append(pm);
        loaded++;
    }
    return loaded;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("ImageScroller");
    QApplication::setApplicationVersion("1.0");

    // --- Command Line Argument Parsing ---
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Displays all images in a folder vertically stacked in a scrollable window.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("folder",
                                 QApplication::translate("main",
                                         "The folder containing images to display."));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        fprintf(stderr, "%s\n\n", qPrintable(
            QApplication::translate("main", "Error: No folder specified.")));
        parser.showHelp(1);
    }

    QList<QString> dirPaths;
    for (const QString &arg : args) {
        dirPaths.append(arg);
    }

    // Validate first directory exists.
    int currentDirIndex = 0;
    QDir firstDir(dirPaths[currentDirIndex]);
    if (!firstDir.exists()) {
        fprintf(stderr, "%s\n", qPrintable(QApplication::translate("main",
            "The specified folder does not exist:\n%1").arg(dirPaths[currentDirIndex])));
        return 1;
    }

    // --- Create GUI ---
    QMainWindow mainWindow;
    QString dirName = QFileInfo(dirPaths[currentDirIndex]).fileName();
    if (dirPaths.size() > 1) {
        mainWindow.setWindowTitle(QApplication::translate("main",
            "Image Scroller - %1 (%2/%3)").arg(
                dirName, QString::number(currentDirIndex + 1),
                QString::number(dirPaths.size())));
    } else {
        mainWindow.setWindowTitle(QApplication::translate("main",
            "Image Scroller - %1").arg(dirName));
    }

    AutoScrollWidget *scrollArea = new AutoScrollWidget(&mainWindow);
    scrollArea->setWidgetResizable(true);

    QList<QLabel *> imageLabels;
    QList<QPixmap> originalPixmaps;

    QWidget *scrollContentWidget = new QWidget();
    QVBoxLayout *verticalLayout = new QVBoxLayout(scrollContentWidget);
    verticalLayout->setSpacing(0);
    scrollContentWidget->setLayout(verticalLayout);
    scrollArea->setWidget(scrollContentWidget);
    mainWindow.setCentralWidget(scrollArea);

    // --- Get screen width for scaling ---
    int containerWidth = QGuiApplication::primaryScreen()->size().width() - 40;
    qDebug() << "width:" << containerWidth;

    // --- Create DirStore and configure it ---
    DirStore *dirStore = new DirStore(&mainWindow);   // parent → lives on GUI thread
    dirStore->setPaths(dirPaths);
    dirStore->setContainerWidth(containerWidth);
    dirStore->setCurrentDir(currentDirIndex);

    // Connect the materialization signal (emitted after cache update on GUI thread).
    QObject::connect(dirStore, &DirStore::entryReady,
                     [&](int dirIndex, int generation) {
        Q_UNUSED(generation);
        if (dirIndex != currentDirIndex) return;  // only materialize if still current

        DirEntry *entry = dirStore->entry(currentDirIndex);
        if (!entry || entry->images.isEmpty()) return;

        // Clear old widgets and rebuild from cache.
        for (QLabel *label : imageLabels) {
            verticalLayout->removeWidget(label);
            delete label;
        }
        imageLabels.clear();
        originalPixmaps.clear();

        int loaded = materializeFromCache(dirStore, currentDirIndex,
                                          verticalLayout, imageLabels,
                                          originalPixmaps, containerWidth);
        if (loaded == 0) return; // nothing to show yet.

        scrollArea->setCurrentScale(1.0);
        scrollArea->verticalScrollBar()->setValue(0);
    });

    // --- Initial synchronous load of the first directory into cache ---
    {
        QDir imageDir(dirPaths[currentDirIndex]);
        if (!imageDir.exists()) return 1;
        imageDir.setFilter(QDir::Files | QDir::Readable);
        QStringList fileList = imageDir.entryList();

        QCollator collator;
        sortFileList(fileList, collator);

        // Build DirEntry manually for the initial directory.
        DirEntry *entry = new DirEntry();
        entry->dirIndex = currentDirIndex;
        entry->files = fileList;
        int loadedCount = 0;
        for (const QString &fileName : std::as_const(fileList)) {
            QFileInfo fi(imageDir.filePath(fileName));
            QString suffix = fi.suffix().toLower();
            if (!imageReaderSupported(suffix)) continue;

            QImage img;
            if (!img.load(fi.absoluteFilePath())) continue;
            if (img.width() > containerWidth && containerWidth > 0) {
                img = img.scaledToWidth(containerWidth, Qt::SmoothTransformation);
            }
            entry->images.append(img);
            loadedCount++;
        }
        entry->valid = true;

        // Insert into cache via accessor.
        dirStore->cache()[currentDirIndex] = entry;

        if (loadedCount == 0) {
            qWarning() << "No supported image files found in the specified folder:" << dirPaths[0];
            return 0;
        }

        // Materialize initial view.
        materializeFromCache(dirStore, currentDirIndex, verticalLayout,
                             imageLabels, originalPixmaps, containerWidth);
    }

    // Add stretch at end.
    verticalLayout->addStretch(1);

    // --- Show Window ---
    mainWindow.resize(800, 600);
    mainWindow.showFullScreen();

    // --- Directory Navigation Lambda (uses buffer) ---
    auto navigateToDirectory = [&](Direction direction) {
        int offset = (direction == Direction::Left) ? -1 : 1;
        int newDirIndex = currentDirIndex + offset;
        if (newDirIndex < 0 || newDirIndex >= dirPaths.size()) return;

        QString oldFolderPath = dirPaths[currentDirIndex];
        QString newFolderPath = dirPaths[newDirIndex];

        // Check directory exists.
        QDir checkDir(newFolderPath);
        if (!checkDir.exists()) {
            fprintf(stderr, "%s\n", qPrintable(QApplication::translate("main",
                "Directory does not exist:\n%1").arg(newFolderPath)));
            return;
        }

        currentDirIndex = newDirIndex;
        dirStore->setCurrentDir(currentDirIndex);

        // Update title immediately (instant feedback).
        QString newName = QFileInfo(newFolderPath).fileName();
        if (dirPaths.size() > 1) {
            mainWindow.setWindowTitle(QApplication::translate("main",
                "Image Scroller - %1 (%2/%3)").arg(
                    newName, QString::number(currentDirIndex + 1),
                    QString::number(dirPaths.size())));
        } else {
            mainWindow.setWindowTitle(QApplication::translate("main",
                "Image Scroller - %1").arg(newName));
        }

        // Check buffer hit.
        if (dirStore->has(currentDirIndex)) {
            DirEntry *entry = dirStore->entry(currentDirIndex);
            if (entry && entry->valid && !entry->images.isEmpty()) {
                // Fast path: swap from cache instantly.
                for (QLabel *label : imageLabels) {
                    verticalLayout->removeWidget(label);
                    delete label;
                }
                imageLabels.clear();
                originalPixmaps.clear();

                int loaded = materializeFromCache(dirStore, currentDirIndex,
                                                  verticalLayout, imageLabels,
                                                  originalPixmaps, containerWidth);
                if (loaded == 0) return;

                scrollArea->setCurrentScale(1.0);
                scrollArea->verticalScrollBar()->setValue(0);

                // Preload neighbors after settling.
                dirStore->preloadNeighbors();
                return;
            }
        }

        // Miss path: show placeholder, load async.
        for (QLabel *label : imageLabels) {
            verticalLayout->removeWidget(label);
            delete label;
        }
        imageLabels.clear();
        originalPixmaps.clear();

        QLabel *placeholder = new QLabel(QString("Loading %1...").arg(newName));
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setFont(QFont("", 18, QFont::DemiBold));
        verticalLayout->addWidget(placeholder);
        imageLabels.append(placeholder);

        // Trigger async load for neighbors (current will be loaded if not cached).
        dirStore->preloadNeighbors();
    };

    QObject::connect(scrollArea, &AutoScrollWidget::zoomRequested, [&](double scaleFactor) {
        for (int i = 0; i < imageLabels.size(); ++i) {
            QLabel *label = imageLabels.at(i);
            const QPixmap &originalPixmap = originalPixmaps.at(i);
            QSize newSize = originalPixmap.size() * scaleFactor;
            label->setPixmap(originalPixmap.scaled(newSize, Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
        }
    });

    QObject::connect(scrollArea, &AutoScrollWidget::navigateRequested, navigateToDirectory);

    // --- Seed initial neighbor preloads ---
    dirStore->preloadNeighbors();

    int result = app.exec();

    // Stop background workers before DirStore is destroyed.
    dirStore->stop();

    return result;
}

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
#include <autoscrollwidget.hpp>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QApplication::setApplicationName("ImageScroller");
	QApplication::setApplicationVersion("1.0");

	// --- Command Line Argument Parsing ---
	QCommandLineParser parser;
	parser.setApplicationDescription("Displays all images in a folder vertically stacked in a scrollable window.");
	parser.addHelpOption();
	parser.addVersionOption();

	// Define the positional argument for the folder path
	parser.addPositionalArgument("folder", QApplication::translate("main", "The folder containing images to display."));

	// Process the actual command line arguments given by the user
	parser.process(app);

	const QStringList args = parser.positionalArguments();
	if (args.isEmpty()) {
		// Show help if no folder is provided
		// Note: Using fprintf because QMessageBox might not work before app.exec() is reliably running everywhere
		fprintf(stderr, "%s\n\n", qPrintable(QApplication::translate("main", "Error: No folder specified.")));
		parser.showHelp(1); // Exits after showing help
	}

	// Collect all directory paths from arguments
	QList<QString> dirPaths;
	for (const QString &arg : args) {
		dirPaths.append(arg);
	}

	int currentDirIndex = 0;
	QString folderPath = dirPaths[currentDirIndex];
	QDir imageDir(folderPath);

	// Check if at least the first directory exists
	if (!imageDir.exists()) {
		fprintf(stderr, "%s\n", qPrintable(QApplication::translate("main", "The specified folder does not exist:\n%1").arg(folderPath)));
		return 1;
	}

	// --- Prepare Main Window and Scroll Area ---
	QMainWindow mainWindow;
	QString dirName = QFileInfo(folderPath).fileName();
	if (dirPaths.size() > 1) {
		mainWindow.setWindowTitle(QApplication::translate("main", "Image Scroller - %1 (%2/%3)").arg(dirName, QString::number(currentDirIndex + 1), QString::number(dirPaths.size())));
	} else {
		mainWindow.setWindowTitle(QApplication::translate("main", "Image Scroller - %1").arg(dirName));
	}

	AutoScrollWidget *scrollArea = new AutoScrollWidget(&mainWindow);
	scrollArea->setWidgetResizable(true); // Crucial: Allows the inner widget to resize horizontally

	QList<QLabel*> imageLabels;
	QList<QPixmap> originalPixmaps;

	QWidget *scrollContentWidget = new QWidget(); // This widget will contain the layout and labels
	QVBoxLayout *verticalLayout = new QVBoxLayout(scrollContentWidget);
	verticalLayout->setSpacing(0);                 // Remove spacing between images

	scrollContentWidget->setLayout(verticalLayout);
	scrollArea->setWidget(scrollContentWidget); // Put the content widget inside the scroll area
	mainWindow.setCentralWidget(scrollArea);    // Make the scroll area the main content of the window

	// --- Image Loading Helpers ---

	// Get supported image formats dynamically
	std::set<QString> supportedSuffixes;
	QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
	for (const QByteArray &format : supportedFormats) {
		supportedSuffixes.insert(QString::fromLatin1(format).toLower());
		if (format == "jpeg") supportedSuffixes.insert("jpg");
	}

	int containerWidth = QGuiApplication::primaryScreen()->size().width()-40;
	qDebug() << "width:" << containerWidth;

	// Helper to sort files using numeric collator
	auto sortFileList = [&](QStringList &list) {
		QCollator collator;
		collator.setNumericMode(true);
		std::sort(list.begin(), list.end(), collator);
	};

	// Helper to load images from a sorted file list into the scroll area
	auto loadImages = [&](const QDir &dir, const QStringList &sortedFiles) -> int {
		int loaded = 0;
		for (const QString &fileName : sortedFiles) {
			QFileInfo fileInfo(dir.filePath(fileName));
			QString suffix = fileInfo.suffix().toLower();

			if (!supportedSuffixes.count(suffix)) {
				qDebug() << "Skipping non-supported file:" << fileName << "(suffix:" << suffix << ")";
				continue;
			}

			QString imagePath = fileInfo.absoluteFilePath();
			QPixmap pixmap;
			if (pixmap.load(imagePath)) {
				QLabel *imageLabel = new QLabel();
				if (!pixmap.isNull() && pixmap.width() > containerWidth && containerWidth > 0) {
					pixmap = pixmap.scaledToWidth(containerWidth);
				}
				imageLabel->setPixmap(pixmap);
				imageLabel->setAlignment(Qt::AlignCenter);
				verticalLayout->addWidget(imageLabel);
				imageLabels.append(imageLabel);
				originalPixmaps.append(pixmap);
				loaded++;
				qDebug() << "Loaded:" << imagePath;
			} else {
				qWarning() << "Failed to load image:" << imagePath;
			}
		}
		return loaded;
	};



	// --- Initial Image Loading ---
	imageDir.setFilter(QDir::Files | QDir::Readable);
	QStringList fileList = imageDir.entryList();
	sortFileList(fileList);
	int imagesLoaded = loadImages(imageDir, fileList);

	if (imagesLoaded == 0) {
		qWarning() << "No supported image files found in the specified folder: " << folderPath;
		return 0;
	}

	// Add a stretch at the end to push images to the top if the total height is less than the window height
	verticalLayout->addStretch(1);

	// --- Show Window ---
	mainWindow.resize(800, 600); // Set a reasonable default size
	mainWindow.showFullScreen();

	// --- Directory Navigation Lambda ---
	auto navigateToDirectory = [&](int direction) {
		int newDirIndex = currentDirIndex + direction;
		if (newDirIndex < 0 || newDirIndex >= dirPaths.size()) return;
		currentDirIndex = newDirIndex;
		QString newFolderPath = dirPaths[currentDirIndex];

		// Check directory exists
		QDir newImageDir(newFolderPath);
		if (!newImageDir.exists()) {
			fprintf(stderr, "%s\n", qPrintable(QApplication::translate("main", "Directory does not exist:\n%1").arg(newFolderPath)));
			return;
		}

		// Clear existing widgets and images
		for (QLabel *label : imageLabels) {
			verticalLayout->removeWidget(label);
			delete label;
		}
		imageLabels.clear();
		originalPixmaps.clear();

		folderPath = newFolderPath;
		imageDir.setPath(newFolderPath);
		fileList = imageDir.entryList();

		// Update title
		if (dirPaths.size() > 1) {
			mainWindow.setWindowTitle(QApplication::translate("main", "Image Scroller - %1 (%2/%3)").arg(
				QFileInfo(newFolderPath).fileName(), QString::number(currentDirIndex + 1), QString::number(dirPaths.size())));
		}

		// Sort and reload images for new directory
		sortFileList(fileList);
		int loaded = loadImages(imageDir, fileList);
		if (loaded == 0) {
			qWarning() << "No supported image files found in:" << newFolderPath;
		}

		// Reset zoom to 1.0 on directory change
		scrollArea->setCurrentScale(1.0);

		// Scroll back to top
		scrollArea->verticalScrollBar()->setValue(0);
	};

	QObject::connect(scrollArea, &AutoScrollWidget::zoomRequested, [&](double scaleFactor) {
		for (int i = 0; i < imageLabels.size(); ++i) {
			QLabel* label = imageLabels.at(i);
			const QPixmap& originalPixmap = originalPixmaps.at(i);
			// Calculate new size based on original pixmap and scale factor
			QSize newSize = originalPixmap.size() * scaleFactor;
			label->setPixmap(originalPixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
	});

	QObject::connect(scrollArea, &AutoScrollWidget::navigateRequested, navigateToDirectory);

	return app.exec();
}

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
#include <set>

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
    if (args.size() > 1) {
        fprintf(stderr, "%s\n\n", qPrintable(QApplication::translate("main", "Error: Too many arguments. Only one folder path is expected.")));
        parser.showHelp(1);
    }

    QString folderPath = args.at(0);
    QDir imageDir(folderPath);

    if (!imageDir.exists()) {
        QMessageBox::critical(nullptr, QApplication::translate("main", "Error"),
                              QApplication::translate("main", "The specified folder does not exist:\n%1").arg(folderPath));
        return 1;
    }

    // --- Prepare Main Window and Scroll Area ---
    QMainWindow mainWindow;
    mainWindow.setWindowTitle(QApplication::translate("main", "Image Scroller - %1").arg(QFileInfo(folderPath).fileName()));

    QScrollArea *scrollArea = new QScrollArea(&mainWindow);
    scrollArea->setWidgetResizable(true); // Crucial: Allows the inner widget to resize horizontally

    QWidget *scrollContentWidget = new QWidget(); // This widget will contain the layout and labels
    QVBoxLayout *verticalLayout = new QVBoxLayout(scrollContentWidget);
    verticalLayout->setContentsMargins(5, 5, 5, 5); // Add some padding
    verticalLayout->setSpacing(0);                 // Remove spacing between images

    scrollContentWidget->setLayout(verticalLayout);
    scrollArea->setWidget(scrollContentWidget); // Put the content widget inside the scroll area
    mainWindow.setCentralWidget(scrollArea);    // Make the scroll area the main content of the window

    // --- Load Images ---

    // Get supported image formats dynamically
    std::set<QString> supportedSuffixes;
    QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    for (const QByteArray &format : supportedFormats) {
        supportedSuffixes.insert(QString::fromLatin1(format).toLower());
         // Also add common variations if needed (though QImageReader often handles this)
        if (format == "jpeg") supportedSuffixes.insert("jpg");
    }


    imageDir.setFilter(QDir::Files | QDir::Readable); // Filter for readable files
    imageDir.setSorting(QDir::NoSort); // Sort files by name (optional)

    QStringList fileList = imageDir.entryList();

		QCollator collator;
		collator.setNumericMode(true);
		std::sort(fileList.begin(), fileList.end(), collator);

    int imagesLoaded = 0;

    for (const QString &fileName : fileList) {
        QFileInfo fileInfo(imageDir.filePath(fileName));
        QString suffix = fileInfo.suffix().toLower();

        // Check if the file suffix is in our set of supported formats
        if (supportedSuffixes.count(suffix)) {
            QString imagePath = fileInfo.absoluteFilePath();
            QPixmap pixmap;
            if (pixmap.load(imagePath)) {
                QLabel *imageLabel = new QLabel();
                imageLabel->setPixmap(pixmap);
                imageLabel->setAlignment(Qt::AlignCenter); // Center the image in the label
                verticalLayout->addWidget(imageLabel);
                imagesLoaded++;
                qDebug() << "Loaded:" << imagePath;
            } else {
                qWarning() << "Failed to load image:" << imagePath;
            }
        } else {
             qDebug() << "Skipping non-supported file:" << fileName << " (suffix:" << suffix << ")";
        }
    }

    if (imagesLoaded == 0) {
			qWarning() << "No supported image files found in the specified folder: " << folderPath;
         // Optionally exit or just show an empty window
         // return 0; // Exit if you don't want to show an empty window
    }

    // Add a stretch at the end to push images to the top if the total height is less than the window height
    verticalLayout->addStretch(1);

    // --- Show Window ---
    mainWindow.resize(800, 600); // Set a reasonable default size
    //mainWindow.show();
    mainWindow.showFullScreen();

    return app.exec();
}

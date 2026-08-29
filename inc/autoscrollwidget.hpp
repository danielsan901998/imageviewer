#ifndef AUTOSCROLLWIDGET_HPP
#define AUTOSCROLLWIDGET_HPP 
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QScrollArea>

enum class Direction { Left, Right };

class AutoScrollWidget : public QScrollArea {
	Q_OBJECT

	public:
		AutoScrollWidget(QWidget *parent = nullptr);

	protected:
		void mousePressEvent(QMouseEvent *event) override;
		void mouseReleaseEvent(QMouseEvent *event) override;
		void mouseMoveEvent(QMouseEvent *event) override;
		void keyPressEvent(QKeyEvent *event) override;

	signals:
		void zoomRequested(double scaleFactor);
		void navigateRequested(Direction direction);

	public slots:
		void scrollStep();
		void setCurrentScale(double scale);

	private:
		QPoint startPos;
		QPoint currentPos;
		bool isAutoScrolling = false;
		bool downpress = false;
		QTimer *timer;
		double currentScale = 1.0;
		double scrollSpeed = 3.0;
		int keyboardScrollDir = 0; // +1 down, -1 up
};
#endif /* AUTOSCROLLWIDGET_HPP */

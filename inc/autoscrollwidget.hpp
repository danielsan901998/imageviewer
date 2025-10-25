#ifndef AUTOSCROLLWIDGET_HPP
#define AUTOSCROLLWIDGET_HPP 
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QScrollArea>

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

		public slots:
			void scrollStep();

	private:
		QPoint startPos;
		QPoint currentPos;
		bool isAutoScrolling = false;
		bool downpress = false;
		QTimer *timer;
		double currentScale = 1.0;
};
#endif /* AUTOSCROLLWIDGET_HPP */

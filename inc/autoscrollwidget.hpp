#ifndef AUTOSCROLLWIDGET_HPP
#define AUTOSCROLLWIDGET_HPP 
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QScrollArea>

class AutoScrollWidget : public QScrollArea {

	public:
		AutoScrollWidget(QWidget *parent = nullptr);

	protected:
		void mousePressEvent(QMouseEvent *event) override;
		void mouseReleaseEvent(QMouseEvent *event) override;
		void mouseMoveEvent(QMouseEvent *event) override;

		public slots:
			void scrollStep();

	private:
		QPoint startPos;
		QPoint currentPos;
		bool isAutoScrolling = false;
		bool downpress = false;
		QTimer *timer;
};
#endif /* AUTOSCROLLWIDGET_HPP */

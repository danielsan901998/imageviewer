#include <autoscrollwidget.hpp>

AutoScrollWidget::AutoScrollWidget(QWidget *parent) : QScrollArea(parent) {
	timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &AutoScrollWidget::scrollStep);
}

void AutoScrollWidget::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::RightButton) {
		if (!isAutoScrolling) {
			startPos = event->pos();
			currentPos = event->pos();
			isAutoScrolling = true;
			timer->start(16); // 60 FPS
		} else {
			isAutoScrolling = false;
			timer->stop();
		}
	} else {
		QWidget::mousePressEvent(event);
	}
}

void AutoScrollWidget::mouseMoveEvent(QMouseEvent *event) {
	if (isAutoScrolling) {
		currentPos = event->pos();
	} else {
		QWidget::mouseMoveEvent(event);
	}
}

void AutoScrollWidget::scrollStep() {
	if (!isAutoScrolling) return;

	int dy = currentPos.y() - startPos.y();
	if (dy == 0) return;

	QScrollBar *vScrollBar = verticalScrollBar();
	if (!vScrollBar) return;

	vScrollBar->setValue(vScrollBar->value() + dy/5);
}

#include <autoscrollwidget.hpp>

AutoScrollWidget::AutoScrollWidget(QWidget *parent) : QScrollArea(parent) {
	timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &AutoScrollWidget::scrollStep);
}
void AutoScrollWidget::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::RightButton) {
		downpress = true;
		startPos = event->pos();
		currentPos = event->pos();
		isAutoScrolling = true;
		timer->start(16); // 60 FPS
	} else {
		QScrollArea::mousePressEvent(event);
	}
}

void AutoScrollWidget::mouseReleaseEvent(QMouseEvent *event) {
	if (event->button() == Qt::RightButton) {
		downpress = false;
	} else {
		QScrollArea::mousePressEvent(event);
	}
}

void AutoScrollWidget::mouseMoveEvent(QMouseEvent *event) {
	if (downpress) {
		currentPos = event->pos();
	} else {
		QScrollArea::mouseMoveEvent(event);
	}
}

void AutoScrollWidget::scrollStep() {
	if (!isAutoScrolling) return;

	int dy = currentPos.y() - startPos.y();
	if (dy == 0){
		if(isAutoScrolling && !downpress){
			isAutoScrolling = false;
			timer->stop();
		}
		return;
	}

	QScrollBar *vScrollBar = verticalScrollBar();
	if (!vScrollBar) return;

	vScrollBar->setValue(vScrollBar->value() + dy/5);
}

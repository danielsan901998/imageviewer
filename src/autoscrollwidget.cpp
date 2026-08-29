#include <autoscrollwidget.hpp>
#include <QApplication>

AutoScrollWidget::AutoScrollWidget(QWidget *parent) : QScrollArea(parent), currentScale(1.0) {
	setFocusPolicy(Qt::StrongFocus);
	timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &AutoScrollWidget::scrollStep);
}

void AutoScrollWidget::setCurrentScale(double scale) {
	currentScale = scale;
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
		keyboardScrollDir = 0; // mouse released, stop keyboard-driven scroll direction
		isAutoScrolling = false;
		timer->stop();
		scrollSpeed = 3.0;     // reset speed
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

	QScrollBar *vScrollBar = verticalScrollBar();
	if (!vScrollBar) return;

	int scrollAmount = 0;

	if (downpress && !keyboardScrollDir) {
		// Mouse drag mode: direction from mouse movement delta
		int dy = currentPos.y() - startPos.y();
		if (dy == 0) return;
		scrollAmount = dy / 5;
	} else if (!downpress && keyboardScrollDir != 0) {
		// Keyboard-driven scroll mode: direction from arrow keys
		// +1 means down, -1 means up
		int baseSpeed = (int)(scrollSpeed * 2); // pixels per tick
		scrollAmount = keyboardScrollDir * baseSpeed;
	}

	if (scrollAmount != 0) {
		vScrollBar->setValue(vScrollBar->value() + scrollAmount);
	}
}

void AutoScrollWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) { // Qt::Key_Equal is often used for '+' without shift
        currentScale *= 1.1; // Zoom in by 10%
        emit zoomRequested(currentScale);
    } else if (event->key() == Qt::Key_Minus) {
        currentScale /= 1.1; // Zoom out by 10%
        emit zoomRequested(currentScale);
    } else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        int newDir = (event->key() == Qt::Key_Up) ? -1 : 1; // -1 up, +1 down
        bool sameDirection = (keyboardScrollDir != 0 && keyboardScrollDir == newDir);
        bool wasIdle = (keyboardScrollDir == 0);

        if (!isAutoScrolling || !downpress) {
            isAutoScrolling = true;
            timer->start(16);
        }

        // Only change speed/direction on the first key press, not auto-repeat.
        if (!event->isAutoRepeat()) {
            // Opposite direction: slow down without switching scroll direction.
            // Same or idle: switch to this direction and increase speed.
            if (sameDirection || wasIdle) {
                keyboardScrollDir = newDir;
                scrollSpeed *= 1.25;   // Increase by 25%
            } else {
                scrollSpeed /= 1.25;   // Decrease by 25%
                if (scrollSpeed < 0.5) {
                    keyboardScrollDir = 0;
                    isAutoScrolling = false;
                    timer->stop();
                    scrollSpeed = 3.0;   // reset speed
                    return;
                }
            }
        }
    } else if (event->key() == Qt::Key_Space) {
        keyboardScrollDir = 0;
        isAutoScrolling = false;
        timer->stop();
        scrollSpeed = 3.0;     // reset speed
    } else if (event->key() == Qt::Key_Escape) {
        QApplication::quit();
    } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        Direction direction = (event->key() == Qt::Key_Left) ? Direction::Left : Direction::Right;
        emit navigateRequested(direction);
    } else {
        QScrollArea::keyPressEvent(event);
    }
}

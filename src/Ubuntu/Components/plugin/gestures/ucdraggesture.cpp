/*
 * Copyright (C) 2015 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define ACTIVETOUCHESINFO_DEBUG 0
#define DIRECTIONALDRAGAREA_DEBUG 0

#include "ucdraggesture_p.h"

#include <QQuickWindow>
#include <QtCore/qmath.h>
#include <QScreen>
#include <QDebug>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-pedantic"
#include <private/qquickwindow_p.h>
#pragma GCC diagnostic pop

// local
#include "TouchOwnershipEvent.h"
#include "TouchRegistry.h"
#include "UnownedTouchEvent.h"

using namespace UbuntuGestures;

#if DIRECTIONALDRAGAREA_DEBUG
#define ddaDebug(params) qDebug().nospace() << "[DDA(" << qPrintable(objectName()) << ")] " << params
//#include "DebugHelpers.h"

namespace {
const char *statusToString(UCDragGesturePrivate::Status status)
{
    if (status == UCDragGesturePrivate::WaitingForTouch) {
        return "WaitingForTouch";
    } else if (status == UCDragGesturePrivate::Undecided) {
        return "Undecided";
    } else {
        return "Recognized";
    }
}

QString touchPointStateToString(Qt::TouchPointState state)
{
    switch (state) {
    case Qt::TouchPointPressed:
        return QStringLiteral("pressed");
    case Qt::TouchPointMoved:
        return QStringLiteral("moved");
    case Qt::TouchPointStationary:
        return QStringLiteral("stationary");
    case Qt::TouchPointReleased:
        return QStringLiteral("released");
    default:
        return QStringLiteral("INVALID_STATE");
    }
}

QString touchEventToString(const QTouchEvent *ev)
{
    QString message;

    switch (ev->type()) {
    case QEvent::TouchBegin:
        message.append("TouchBegin ");
        break;
    case QEvent::TouchUpdate:
        message.append("TouchUpdate ");
        break;
    case QEvent::TouchEnd:
        message.append("TouchEnd ");
        break;
    case QEvent::TouchCancel:
        message.append("TouchCancel ");
        break;
    default:
        message.append("INVALID_TOUCH_EVENT_TYPE ");
    }

    Q_FOREACH(const QTouchEvent::TouchPoint& touchPoint, ev->touchPoints()) {
        message.append(
            QStringLiteral("(id:%1, state:%2, scenePos:(%3,%4)) ")
                .arg(touchPoint.id())
                .arg(touchPointStateToString(touchPoint.state()))
                .arg(touchPoint.scenePos().x())
                .arg(touchPoint.scenePos().y())
            );
    }

    return message;
}

} // namespace {
#else // DIRECTIONALDRAGAREA_DEBUG
#define ddaDebug(params) ((void)0)
#endif // DIRECTIONALDRAGAREA_DEBUG

class Direction
{
public:
    static bool isHorizontal(UCDragGesture::Direction type)
    {
        return type == UCDragGesture::Leftwards
            || type == UCDragGesture::Rightwards
            || type == UCDragGesture::Horizontal;
    }

    static bool isVertical(UCDragGesture::Direction type)
    {
        return type == UCDragGesture::Upwards
            || type == UCDragGesture::Downwards
            || type == UCDragGesture::Vertical;
    }

    static bool isPositive(UCDragGesture::Direction type)
    {
        return type == UCDragGesture::Rightwards
            || type == UCDragGesture::Downwards
            || type == UCDragGesture::Horizontal
            || type == UCDragGesture::Vertical;
    }
};
/*!
 * \qmltype DragGesture
 * \instantiates UCDragGesture
 * \inherits Item
 * \inqmlmodule Ubuntu.Components 1.3
 * \since Ubuntu.Components 1.3
 * \ingroup ubuntu-gestures
 * \brief An area that detects axis-aligned single-finger drag gestures.
 *
 * The component can be used to detect gestures of a certain direction, and can
 * grab gestures started on a component placed behind of the DragGesture area.
 * The gesture is detected on the DragGesture area, therefore the size must be
 * chosen carefully so it can properly detect the gesture.
 *
 * The gesture direction is specified by the \l direction property. The recognized
 * and captured gesture is reported through the \l dragging property, which becomes
 * \c true when the gesture is detected. If there was a component under the DragGesture,
 * the gesture will be cancelled on that component.
 *
 * The following image depicts the way the gesture is recognized.
 * \image DragGesture.svg
 * The drag recognision is performed in a distanceThreshold, which is the size
 * of the DragGesture component (either width or height, depending on the direction
 * chosen). If the drag deviates too much from the component's direction, recognition
 * will fail, as well as if the drag or the flick is too short.
 *
 */
UCDragGesture::UCDragGesture(QQuickItem *parent)
    : QQuickItem(parent)
    , d(new UCDragGesturePrivate(this))
{
    d->setRecognitionTimer(new Timer(this));
    d->recognitionTimer->setInterval(d->maxTime);
    d->recognitionTimer->setSingleShot(true);

    connect(this, &QQuickItem::enabledChanged, d, &UCDragGesturePrivate::giveUpIfDisabledOrInvisible);
    connect(this, &QQuickItem::visibleChanged, d, &UCDragGesturePrivate::giveUpIfDisabledOrInvisible);
}

/*!
 * \qmlproperty enum DragGesture::direction
 * The direction in which the gesture should move in order to be recognized.
 * \table
 * \header
 *  \li Direction
 *  \li Description
 * \row
 *  \li Rightwards
 *  \li Along the positive direction of the X axis
 * \row
 *  \li Leftwards
 *  \li Along the negative direction of the X axis
 * \row
 *  \li Downwards
 *  \li Along the positive direction of the Y axis
 * \row
 *  \li Upwards
 *  \li Along the negative direction of the Y axis
 * \row
 *  \li Horizontal
 *  \li Along the X axis, in any direction
 * \row
 *  \li Vertical
 *  \li Along the Y axis, in any direction
 * \endtable
 */
UCDragGesture::Direction UCDragGesture::direction() const
{
    return d->direction;
}

void UCDragGesture::setDirection(Direction direction)
{
    if (direction != d->direction) {
        d->direction = direction;
        Q_EMIT directionChanged(d->direction);
    }
}

void UCDragGesturePrivate::setDistanceThreshold(qreal value)
{
    if (distanceThreshold != value) {
        distanceThreshold = value;
        distanceThresholdSquared = distanceThreshold * distanceThreshold;
    }
}

void UCDragGesturePrivate::setMaxTime(int value)
{
    if (maxTime != value) {
        maxTime = value;
        recognitionTimer->setInterval(maxTime);
    }
}

void UCDragGesturePrivate::setRecognitionTimer(UbuntuGestures::AbstractTimer *timer)
{
    int interval = 0;
    bool timerWasRunning = false;
    bool wasSingleShot = false;

    // can be null when called from the constructor
    if (recognitionTimer) {
        interval = recognitionTimer->interval();
        timerWasRunning = recognitionTimer->isRunning();
        if (recognitionTimer->parent() == this) {
            delete recognitionTimer;
        }
    }

    recognitionTimer = timer;
    timer->setInterval(interval);
    timer->setSingleShot(wasSingleShot);
    connect(timer, &UbuntuGestures::AbstractTimer::timeout,
            this, &UCDragGesturePrivate::rejectGesture);
    if (timerWasRunning) {
        recognitionTimer->start();
    }
}

void UCDragGesturePrivate::setTimeSource(const SharedTimeSource &timeSource)
{
    this->timeSource = timeSource;
    activeTouches.m_timeSource = timeSource;
}

/*!
 * \qmlproperty real DragGesture::distance
 * \readonly
 * The distance travelled by the finger along the axis specified by \l direction.
 */
qreal UCDragGesture::distance() const
{
    if (::Direction::isHorizontal(d->direction)) {
        return d->publicPos.x() - d->startPos.x();
    } else {
        return d->publicPos.y() - d->startPos.y();
    }
}

void UCDragGesturePrivate::updateSceneDistance()
{
    QPointF totalMovement = publicScenePos - startScenePos;
    sceneDistance = projectOntoDirectionVector(totalMovement);
}

/*!
 * \qmlproperty real DragGesture::sceneDistance
 * \readonly
 * The distance travelled by the finger along the axis specified by \l direction
 * in scene coordinates
 */
qreal UCDragGesture::sceneDistance() const
{
    return d->sceneDistance;
}

/*!
 * \qmlproperty point DragGesture::touchPos
 * \readonly
 * Position of the touch point performing the drag relative to this item.
 */
QPointF UCDragGesture::touchPos() const
{
    return d->publicPos;
}

/*!
 * \qmlproperty point DragGesture::touchScenePos
 * \readonly
 * Position of the touch point performing the drag, in scene's coordinates.
 */
QPointF UCDragGesture::touchScenePos() const
{
    return d->publicScenePos;
}

/*!
 * \qmlproperty bool DragGesture::dragging
 * \readonly
 * Reports whether a drag gesture is taking place.
 */
bool UCDragGesture::dragging() const
{
    return d->status == UCDragGesturePrivate::Recognized;
}

/*!
 * \qmlproperty bool DragGesture::pressed
 * \readonly
 * Reports whether the drag area is pressed.
 */
bool UCDragGesture::pressed() const
{
    return d->status != UCDragGesturePrivate::WaitingForTouch;
}

/*!
 * \qmlproperty bool DragGesture::immediateRecognition
 * \readonly
 * Drives whether the gesture should be recognized as soon as the touch lands on
 * the area. With this property set it will work the same way as a MultiPointTouchArea,
 *
 * Defaults to false. In most cases this should not be set.
 */
bool UCDragGesture::immediateRecognition() const
{
    return d->immediateRecognition;
}

void UCDragGesture::setImmediateRecognition(bool enabled)
{
    if (d->immediateRecognition != enabled) {
        d->immediateRecognition = enabled;
        Q_EMIT immediateRecognitionChanged(enabled);
    }
}

void UCDragGesture::removeTimeConstraints()
{
    d->setMaxTime(60 * 60 * 1000);
    d->compositionTime = 0;
    ddaDebug("removed time constraints");
}

bool UCDragGesture::event(QEvent *event)
{
    if (event->type() == TouchOwnershipEvent::touchOwnershipEventType()) {
        d->touchOwnershipEvent(static_cast<TouchOwnershipEvent *>(event));
        return true;
    } else if (event->type() == UnownedTouchEvent::unownedTouchEventType()) {
        d->unownedTouchEvent(static_cast<UnownedTouchEvent *>(event));
        return true;
    } else {
        return QQuickItem::event(event);
    }
}

void UCDragGesturePrivate::touchOwnershipEvent(TouchOwnershipEvent *event)
{
    if (event->gained()) {
        QVector<int> ids;
        ids.append(event->touchId());
        ddaDebug("grabbing touch");
        q->grabTouchPoints(ids);

        // Work around for Qt bug. If we grab a touch that is being used for mouse pointer
        // emulation it will cause the emulation logic to go nuts.
        // Thus we have to also grab the mouse in this case.
        //
        // The fix for this bug has landed in Qt 5.4 (https://codereview.qt-project.org/96887)
        // TODO: Remove this workaround once we start using Qt 5.4
        if (q->window()) {
            QQuickWindowPrivate *windowPrivate = QQuickWindowPrivate::get(q->window());
            if (windowPrivate->touchMouseId == event->touchId() && q->window()->mouseGrabberItem()) {
                ddaDebug("removing mouse grabber");
                q->window()->mouseGrabberItem()->ungrabMouse();
            }
        }
    } else {
        // We still wanna know when it ends for keeping the composition time window up-to-date
        TouchRegistry::instance()->addTouchWatcher(touchId, q);

        setStatus(WaitingForTouch);
    }
}

void UCDragGesturePrivate::unownedTouchEvent(UnownedTouchEvent *unownedTouchEvent)
{
    QTouchEvent *event = unownedTouchEvent->touchEvent();

    Q_ASSERT(!event->touchPointStates().testFlag(Qt::TouchPointPressed));

    ddaDebug("Unowned " << timeSource->msecsSinceReference() << " " << qPrintable(touchEventToString(event)));

    switch (status) {
        case WaitingForTouch:
            // do nothing
            break;
        case Undecided:
            Q_ASSERT(q->isEnabled() && q->isVisible());
            unownedTouchEvent_undecided(unownedTouchEvent);
            break;
        default: // Recognized:
            // do nothing
            break;
    }

    activeTouches.update(event);
}

void UCDragGesturePrivate::unownedTouchEvent_undecided(UnownedTouchEvent *unownedTouchEvent)
{
    const QTouchEvent::TouchPoint *touchPoint = fetchTargetTouchPoint(unownedTouchEvent->touchEvent());
    if (!touchPoint) {
        qCritical() << "UCDragGesture[status=Undecided]: touch " << touchId
            << "missing from UnownedTouchEvent without first reaching state Qt::TouchPointReleased. "
               "Considering it as released.";

        TouchRegistry::instance()->removeCandidateOwnerForTouch(touchId, q);
        setStatus(WaitingForTouch);
        return;
    }

    const QPointF &touchScenePos = touchPoint->scenePos();

    if (touchPoint->state() == Qt::TouchPointReleased) {
        // touch has ended before recognition concluded
        ddaDebug("Touch has ended before recognition concluded");
        TouchRegistry::instance()->removeCandidateOwnerForTouch(touchId, q);
        setStatus(WaitingForTouch);
        return;
    }

    previousDampedScenePos.setX(dampedScenePos.x());
    previousDampedScenePos.setY(dampedScenePos.y());
    dampedScenePos.update(touchScenePos);

    if (!movingInRightDirection()) {
        ddaDebug("Rejecting gesture because touch point is moving in the wrong direction.");
        TouchRegistry::instance()->removeCandidateOwnerForTouch(touchId, q);
        // We still wanna know when it ends for keeping the composition time window up-to-date
        TouchRegistry::instance()->addTouchWatcher(touchId, q);
        setStatus(WaitingForTouch);
        return;
    }

    if (isWithinTouchCompositionWindow()) {
        // There's still time for some new touch to appear and ruin our party as it would be combined
        // with our touchId one and therefore deny the possibility of a single-finger gesture.
        ddaDebug("Sill within composition window. Let's wait more.");
        return;
    }

    if (movedFarEnoughAlongGestureAxis()) {
        TouchRegistry::instance()->requestTouchOwnership(touchId, q);
        setStatus(Recognized);
        setPublicPos(touchPoint->pos());
        setPublicScenePos(touchScenePos);
    } else if (isPastMaxDistance()) {
        ddaDebug("Rejecting gesture because it went farther than maxDistance without getting recognized.");
        TouchRegistry::instance()->removeCandidateOwnerForTouch(touchId, q);
        // We still wanna know when it ends for keeping the composition time window up-to-date
        TouchRegistry::instance()->addTouchWatcher(touchId, q);
        setStatus(WaitingForTouch);
    } else {
        ddaDebug("Didn't move far enough yet. Let's wait more.");
    }
}

void UCDragGesture::touchEvent(QTouchEvent *event)
{
    // TODO: Consider when more than one touch starts in the same event (although it's not possible
    //       with Mir's android-input). Have to track them all. Consider it a plus/bonus.

    ddaDebug(d->timeSource->msecsSinceReference() << " " << qPrintable(touchEventToString(event)));

    if (!isEnabled() || !isVisible()) {
        QQuickItem::touchEvent(event);
        return;
    }

    switch (d->status) {
        case UCDragGesturePrivate::WaitingForTouch:
            d->touchEvent_absent(event);
            break;
        case UCDragGesturePrivate::Undecided:
            d->touchEvent_undecided(event);
            break;
        default: // Recognized:
            d->touchEvent_recognized(event);
            break;
    }

    d->activeTouches.update(event);
}

void UCDragGesturePrivate::touchEvent_absent(QTouchEvent *event)
{
    // TODO: accept/reject is for the whole event, not per touch id. See how that affects us.

    if (!event->touchPointStates().testFlag(Qt::TouchPointPressed)) {
        // Nothing to see here. No touch starting in this event.
        return;
    }

    // to be proven wrong, if that's the case
    bool allGood = true;

    if (isWithinTouchCompositionWindow()) {
        // too close to the last touch start. So we consider them as starting roughly at the same time.
        // Can't be a single-touch gesture.
        ddaDebug("A new touch point came in but we're still within time composition window. Ignoring it.");
        allGood = false;
    }

    const QList<QTouchEvent::TouchPoint> &touchPoints = event->touchPoints();

    const QTouchEvent::TouchPoint *newTouchPoint = nullptr;
    for (int i = 0; i < touchPoints.count() && allGood; ++i) {
        const QTouchEvent::TouchPoint &touchPoint = touchPoints.at(i);
        if (touchPoint.state() == Qt::TouchPointPressed) {
            if (newTouchPoint) {
                // more than one touch starting in this QTouchEvent. Can't be a single-touch gesture
                allGood = false;
            } else {
                // that's our candidate
                newTouchPoint = &touchPoint;
            }
        }
    }

    if (allGood) {
        allGood = sanityCheckRecognitionProperties();
        if (!allGood) {
            qWarning("UCDragGesture: recognition properties are wrongly set. Gesture recognition"
                " is impossible");
        }
    }

    if (allGood) {
        Q_ASSERT(newTouchPoint);

        startPos = newTouchPoint->pos();
        startScenePos = newTouchPoint->scenePos();
        touchId = newTouchPoint->id();
        dampedScenePos.reset(startScenePos);
        setPublicPos(startPos);

        setPublicScenePos(startScenePos);
        updateSceneDirectionVector();

        if (recognitionIsDisabled()) {
            // Behave like a dumb TouchArea
            ddaDebug("Gesture recognition is disabled. Requesting touch ownership immediately.");
            TouchRegistry::instance()->requestTouchOwnership(touchId, q);
            setStatus(Recognized);
            event->accept();
        } else {
            // just monitor the touch points for now.
            TouchRegistry::instance()->addCandidateOwnerForTouch(touchId, q);

            setStatus(Undecided);
            // Let the item below have it. We will monitor it and grab it later if a gesture
            // gets recognized.
            event->ignore();
        }
    } else {
        watchPressedTouchPoints(touchPoints);
        event->ignore();
    }
}

void UCDragGesturePrivate::touchEvent_undecided(QTouchEvent *event)
{
    Q_ASSERT(fetchTargetTouchPoint(event) == nullptr);

    // We're not interested in new touch points. We already have our candidate (touchId).
    // But we do want to know when those new touches end for keeping the composition time
    // window up-to-date
    event->ignore();
    watchPressedTouchPoints(event->touchPoints());

    if (event->touchPointStates().testFlag(Qt::TouchPointPressed) && isWithinTouchCompositionWindow()) {
        // multi-finger drags are not accepted
        ddaDebug("Multi-finger drags are not accepted");

        TouchRegistry::instance()->removeCandidateOwnerForTouch(touchId, q);
        // We still wanna know when it ends for keeping the composition time window up-to-date
        TouchRegistry::instance()->addTouchWatcher(touchId, q);

        setStatus(WaitingForTouch);
    }
}

void UCDragGesturePrivate::touchEvent_recognized(QTouchEvent *event)
{
    const QTouchEvent::TouchPoint *touchPoint = fetchTargetTouchPoint(event);

    if (!touchPoint) {
        qCritical() << "UCDragGesture[status=Recognized]: touch " << touchId
            << "missing from QTouchEvent without first reaching state Qt::TouchPointReleased. "
               "Considering it as released.";
        setStatus(WaitingForTouch);
    } else {
        setPublicPos(touchPoint->pos());
        setPublicScenePos(touchPoint->scenePos());

        if (touchPoint->state() == Qt::TouchPointReleased) {
            setStatus(WaitingForTouch);
        }
    }
}

void UCDragGesturePrivate::watchPressedTouchPoints(const QList<QTouchEvent::TouchPoint> &touchPoints)
{
    for (int i = 0; i < touchPoints.count(); ++i) {
        const QTouchEvent::TouchPoint &touchPoint = touchPoints.at(i);
        if (touchPoint.state() == Qt::TouchPointPressed) {
            TouchRegistry::instance()->addTouchWatcher(touchPoint.id(), q);
        }
    }
}

bool UCDragGesturePrivate::recognitionIsDisabled() const
{
    return immediateRecognition || (distanceThreshold <= 0 && compositionTime <= 0);
}

bool UCDragGesturePrivate::sanityCheckRecognitionProperties()
{
    return recognitionIsDisabled()
        || (distanceThreshold < maxDistance && compositionTime < maxTime);
}

const QTouchEvent::TouchPoint *UCDragGesturePrivate::fetchTargetTouchPoint(QTouchEvent *event)
{
    const QList<QTouchEvent::TouchPoint> &touchPoints = event->touchPoints();
    const QTouchEvent::TouchPoint *touchPoint = 0;
    for (int i = 0; i < touchPoints.size(); ++i) {
        if (touchPoints.at(i).id() == touchId) {
            touchPoint = &touchPoints.at(i);
            break;
        }
    }
    return touchPoint;
}

bool UCDragGesturePrivate::movingInRightDirection() const
{
    if (direction == UCDragGesture::Horizontal || direction == UCDragGesture::Vertical) {
        return true;
    } else {
        QPointF movementVector(dampedScenePos.x() - previousDampedScenePos.x(),
                               dampedScenePos.y() - previousDampedScenePos.y());

        qreal scalarProjection = projectOntoDirectionVector(movementVector);

        return scalarProjection >= 0.;
    }
}

bool UCDragGesturePrivate::movedFarEnoughAlongGestureAxis() const
{
    if (distanceThreshold <= 0.) {
        // distance threshold check is disabled
        return true;
    } else {
        QPointF totalMovement(dampedScenePos.x() - startScenePos.x(),
                              dampedScenePos.y() - startScenePos.y());

        qreal scalarProjection = projectOntoDirectionVector(totalMovement);

        ddaDebug(" movedFarEnoughAlongGestureAxis: scalarProjection=" << scalarProjection
            << ", distanceThreshold=" << distanceThreshold);

        if (direction == UCDragGesture::Horizontal || direction == UCDragGesture::Vertical) {
            return qAbs(scalarProjection) > distanceThreshold;
        } else {
            return scalarProjection > distanceThreshold;
        }
    }
}

bool UCDragGesturePrivate::isPastMaxDistance() const
{
    QPointF totalMovement(dampedScenePos.x() - startScenePos.x(),
                          dampedScenePos.y() - startScenePos.y());

    qreal squaredDistance = totalMovement.x()*totalMovement.x() + totalMovement.y()*totalMovement.y();
    return squaredDistance > maxDistance*maxDistance;
}

void UCDragGesturePrivate::giveUpIfDisabledOrInvisible()
{
    if (!q->isEnabled() || !q->isVisible()) {
        if (status == Undecided) {
            TouchRegistry::instance()->removeCandidateOwnerForTouch(touchId, q);
            // We still wanna know when it ends for keeping the composition time window up-to-date
            TouchRegistry::instance()->addTouchWatcher(touchId, q);
        }

        if (status != WaitingForTouch) {
            ddaDebug("Resetting status because got disabled or made invisible");
            setStatus(WaitingForTouch);
        }
    }
}

void UCDragGesturePrivate::rejectGesture()
{
    if (status == Undecided) {
        ddaDebug("Rejecting gesture because it's taking too long to drag beyond the threshold.");

        TouchRegistry::instance()->removeCandidateOwnerForTouch(touchId, q);
        // We still wanna know when it ends for keeping the composition time window up-to-date
        TouchRegistry::instance()->addTouchWatcher(touchId, q);

        setStatus(WaitingForTouch);
    }
}

void UCDragGesturePrivate::setStatus(Status newStatus)
{
    if (newStatus == status)
        return;

    Status oldStatus = status;

    if (oldStatus == Undecided) {
        recognitionTimer->stop();
    }

    status = newStatus;
    Q_EMIT statusChanged(status);

    ddaDebug(statusToString(oldStatus) << " -> " << statusToString(newStatus));

    switch (newStatus) {
        case WaitingForTouch:
            if (oldStatus == Recognized) {
                Q_EMIT q->draggingChanged(false);
            }
            Q_EMIT q->pressedChanged(false);
            break;
        case Undecided:
            recognitionTimer->start();
            Q_EMIT q->pressedChanged(true);
            break;
        case Recognized:
            Q_EMIT q->draggingChanged(true);
            break;
        default:
            // no-op
            break;
    }
}

void UCDragGesturePrivate::setPublicPos(const QPointF &point)
{
    bool xChanged = publicPos.x() != point.x();
    bool yChanged = publicPos.y() != point.y();

    // Public position should not get updated while the gesture is still being recognized
    // (ie, Undecided status).
    Q_ASSERT(status == WaitingForTouch || status == Recognized);

    if (status == Recognized && !recognitionIsDisabled()) {
        // When the gesture finally gets recognized, the finger will likely be
        // reasonably far from the edge. If we made the contentX immediately
        // follow the finger position it would be visually unpleasant as it
        // would appear right next to the user's finger out of nowhere (ie,
        // it would jump). Instead, we make contentX go towards the user's
        // finger in several steps. ie., in an animated way.
        QPointF delta = point - publicPos;
        // the trick is not to go all the way (1.0) as it would cause a sudden jump
        publicPos.rx() += 0.4 * delta.x();
        publicPos.ry() += 0.4 * delta.y();
    } else {
        // no smoothing when initializing or if gesture recognition was immediate as there will
        // be no jump.
        publicPos = point;
    }

    if (xChanged) {
        Q_EMIT q->touchPosChanged(publicPos);
        if (Direction::isHorizontal(direction))
            Q_EMIT q->distanceChanged(q->distance());
    }

    if (yChanged) {
        Q_EMIT q->touchPosChanged(publicPos);
        if (Direction::isVertical(direction))
            Q_EMIT q->distanceChanged(q->distance());
    }
}

void UCDragGesturePrivate::setPublicScenePos(const QPointF &point)
{
    bool xChanged = publicScenePos.x() != point.x();
    bool yChanged = publicScenePos.y() != point.y();

    if (!xChanged && !yChanged)
        return;

    // Public position should not get updated while the gesture is still being recognized
    // (ie, Undecided status).
    Q_ASSERT(status == WaitingForTouch || status == Recognized);

    qreal oldSceneDistance = sceneDistance;

    if (status == Recognized && !recognitionIsDisabled()) {
        // When the gesture finally gets recognized, the finger will likely be
        // reasonably far from the edge. If we made the contentX immediately
        // follow the finger position it would be visually unpleasant as it
        // would appear right next to the user's finger out of nowhere (ie,
        // it would jump). Instead, we make contentX go towards the user's
        // finger in several steps. ie., in an animated way.
        QPointF delta = point - publicScenePos;
        // the trick is not to go all the way (1.0) as it would cause a sudden jump
        publicScenePos.rx() += 0.4 * delta.x();
        publicScenePos.ry() += 0.4 * delta.y();
    } else {
        // no smoothing when initializing or if gesture recognition was immediate as there will
        // be no jump.
        publicScenePos = point;
    }

    updateSceneDistance();

    if (oldSceneDistance != sceneDistance) {
        Q_EMIT q->sceneDistanceChanged(sceneDistance);
    }

    if (xChanged || yChanged) {
        Q_EMIT q->touchScenePosChanged(publicScenePos);
    }
}

bool UCDragGesturePrivate::isWithinTouchCompositionWindow()
{
    return
        compositionTime > 0 &&
        !activeTouches.isEmpty() &&
        timeSource->msecsSinceReference() <=
            activeTouches.mostRecentStartTime() + (qint64)compositionTime;
}

void UCDragGesture::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == QQuickItem::ItemSceneChange) {
        if (value.window != nullptr) {
            value.window->installEventFilter(TouchRegistry::instance());

            // TODO: Handle window->screen() changes (ie window changing screens)
            qreal pixelsPerMm = value.window->screen()->physicalDotsPerInch() / 25.4;
            d->setPixelsPerMm(pixelsPerMm);
        }
    }
}

void UCDragGesturePrivate::setPixelsPerMm(qreal pixelsPerMm)
{
    dampedScenePos.setMaxDelta(1. * pixelsPerMm);
    setDistanceThreshold(4. * pixelsPerMm);
    maxDistance = 10. * pixelsPerMm;
}

//**************************  ActiveTouchesInfo **************************

ActiveTouchesInfo::ActiveTouchesInfo(const SharedTimeSource &timeSource)
    : m_timeSource(timeSource)
{
}

void ActiveTouchesInfo::update(QTouchEvent *event)
{
    if (!(event->touchPointStates() & (Qt::TouchPointPressed | Qt::TouchPointReleased))) {
        // nothing to update
        #if ACTIVETOUCHESINFO_DEBUG
        qDebug("[DDA::ActiveTouchesInfo] Nothing to Update");
        #endif
        return;
    }

    const QList<QTouchEvent::TouchPoint> &touchPoints = event->touchPoints();
    for (int i = 0; i < touchPoints.count(); ++i) {
        const QTouchEvent::TouchPoint &touchPoint = touchPoints.at(i);
        if (touchPoint.state() == Qt::TouchPointPressed) {
            addTouchPoint(touchPoint.id());
        } else if (touchPoint.state() == Qt::TouchPointReleased) {
            removeTouchPoint(touchPoint.id());
        }
    }
}

#if ACTIVETOUCHESINFO_DEBUG
QString ActiveTouchesInfo::toString()
{
    QString string = "(";

    {
        QTextStream stream(&string);
        m_touchInfoPool.forEach([&](Pool<ActiveTouchInfo>::Iterator &touchInfo) {
            stream << "(id=" << touchInfo->id << ",startTime=" << touchInfo->startTime << ")";
            return true;
        });
    }

    string.append(")");

    return string;
}
#endif // ACTIVETOUCHESINFO_DEBUG

void ActiveTouchesInfo::addTouchPoint(int touchId)
{
    ActiveTouchInfo &activeTouchInfo = m_touchInfoPool.getEmptySlot();
    activeTouchInfo.id = touchId;
    activeTouchInfo.startTime = m_timeSource->msecsSinceReference();

    #if ACTIVETOUCHESINFO_DEBUG
    qDebug() << "[DDA::ActiveTouchesInfo]" << qPrintable(toString());
    #endif
}

qint64 ActiveTouchesInfo::touchStartTime(int touchId)
{
    qint64 result = -1;

    m_touchInfoPool.forEach([&](Pool<ActiveTouchInfo>::Iterator &touchInfo) {
        if (touchId == touchInfo->id) {
            result = touchInfo->startTime;
            return false;
        } else {
            return true;
        }
    });

    Q_ASSERT(result != -1);
    return result;
}

void ActiveTouchesInfo::removeTouchPoint(int touchId)
{
    m_touchInfoPool.forEach([&](Pool<ActiveTouchInfo>::Iterator &touchInfo) {
        if (touchId == touchInfo->id) {
            m_touchInfoPool.freeSlot(touchInfo);
            return false;
        } else {
            return true;
        }
    });

    #if ACTIVETOUCHESINFO_DEBUG
    qDebug() << "[DDA::ActiveTouchesInfo]" << qPrintable(toString());
    #endif
}

qint64 ActiveTouchesInfo::mostRecentStartTime()
{
    Q_ASSERT(!m_touchInfoPool.isEmpty());

    qint64 highestStartTime = -1;

    m_touchInfoPool.forEach([&](Pool<ActiveTouchInfo>::Iterator &activeTouchInfo) {
        if (activeTouchInfo->startTime > highestStartTime) {
            highestStartTime = activeTouchInfo->startTime;
        }
        return true;
    });

    return highestStartTime;
}

void UCDragGesturePrivate::updateSceneDirectionVector()
{
    QPointF localOrigin(0., 0.);
    QPointF localDirection;
    switch (direction) {
        case UCDragGesture::Upwards:
            localDirection.rx() = 0.;
            localDirection.ry() = -1.;
            break;
        case UCDragGesture::Downwards:
        case UCDragGesture::Vertical:
            localDirection.rx() = 0.;
            localDirection.ry() = 1;
            break;
        case UCDragGesture::Leftwards:
            localDirection.rx() = -1.;
            localDirection.ry() = 0.;
            break;
        default: // UCDragGesture::Rightwards || Direction.Horizontal
            localDirection.rx() = 1.;
            localDirection.ry() = 0.;
            break;
    }
    QPointF sceneOrigin = q->mapToScene(localOrigin);
    QPointF sceneDirection = q->mapToScene(localDirection);
    sceneDirectionVector = sceneDirection - sceneOrigin;
}

qreal UCDragGesturePrivate::projectOntoDirectionVector(const QPointF &sceneVector) const
{
    // same as dot product as sceneDirectionVector is a unit vector
    return  sceneVector.x() * sceneDirectionVector.x() +
            sceneVector.y() * sceneDirectionVector.y();
}

UCDragGesturePrivate::UCDragGesturePrivate(UCDragGesture *q)
    : q(q)
    , status(WaitingForTouch)
    , sceneDistance(0)
    , touchId(-1)
    , direction(UCDragGesture::Rightwards)
    , distanceThreshold(0)
    , distanceThresholdSquared(0.)
    , maxTime(400)
    , compositionTime(60)
    , immediateRecognition(false)
    , recognitionTimer(nullptr)
    , timeSource(new RealTimeSource)
    , activeTouches(timeSource)
{
}

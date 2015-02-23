/*
 * Copyright 2014-2015 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ucunits.h"
#include "uctheme.h"
#include "uclistitem.h"
#include "uclistitem_p.h"
#include "propertychange_p.h"
#include "quickutils.h"
#include "i18n.h"
#include "uclistitemstyle.h"
#include "privates/listitemdragarea.h"
#include <QtQuick/private/qquickflickable_p.h>
#include <QtQml/private/qqmlcomponentattached_p.h>
#include <QtQml/QQmlInfo>

/*!
 * \qmltype ListItemDrag
 * \inqmlmodule Ubuntu.Components 1.2
 * \ingroup unstable-ubuntu-listitems
 * \since Ubuntu.Components 1.2
 * \brief Provides information about a ListItem drag event.
 *
 * The object cannot be instantiated and it is passed as parameter to \l ViewItems::draggingUpdated
 * attached signal. Developer can decide whether to accept or restrict the dragging
 * event based on the input provided by this event.
 *
 * The direction of the drag can be found via the \l status property and the
 * source and destination the drag can be applied via \l from and \l to properties.
 * The allowed directions can be configured through \l minimumIndex and \l maximumIndex
 * properties, and the event acceptance through \l accept property. If the event is not
 * accepted, the drag action will be considered as cancelled.
 */

/*!
 * \qmlproperty enum ListItemDrag::status
 * \readonly
 * The property provides information about the status of the drag. Its value can
 * be one of the following:
 * \list
 *  \li \b ListItemDrag.Started - indicates that the dragging is about to start,
 *              giving opportunities to define restrictions on the dragging indexes,
 *              like \l minimumIndex, \l maximumIndex
 *  \li \b ListItemDrag.Moving - the dragged item is moved upwards or downwards
 *              in the ListItem
 *  \li \b ListItemDrag.Dropped - indicates that the drag event is finished, and
 *              the dragged item is about to be dropped to the given position.
 * \endlist
 */

/*!
 * \qmlproperty int ListItemDrag::from
 * \readonly
 * Specifies the source index the ListItem is dragged from.
 */
/*!
 * \qmlproperty int ListItemDrag::to
 * \readonly
 *
 * Specifies the index the ListItem is dragged to or dropped.
 */

/*!
 * \qmlproperty int ListItemDrag::minimumIndex
 */
/*!
 * \qmlproperty int ListItemDrag::maximumIndex
 * These properties configure the minimum and maximum indexes the item can be
 * dragged. The properties can be set in \l ViewItems::draggingUpdated signal.
 * A negative value means no restriction defined on the dragging interval side.
 */

/*!
 * \qmlproperty bool ListItemDrag::accept
 * The property can be used to dismiss the event. By default its value is true,
 * meaning the drag event is accepted. The value of false blocks the drag event
 * to be handled by the ListItem's dragging mechanism.
 */

/*
 * The properties are attached to the ListItem's parent item or to its closest
 * Flickable parent, when embedded in ListView or Flickable. There will be only
 * one attached property per Flickable for all embedded child ListItems, enabling
 * in this way the controlling of the interactive flag of the Flickable and all
 * its ascendant Flickables.
 */
UCViewItemsAttachedPrivate::UCViewItemsAttachedPrivate(UCViewItemsAttached *qq)
    : q_ptr(qq)
    , listView(0)
    , dragArea(0)
    , globalDisabled(false)
    , selectable(false)
    , draggable(false)
    , ready(false)
{
}

UCViewItemsAttachedPrivate::~UCViewItemsAttachedPrivate()
{
    clearChangesList();
    clearFlickablesList();
}

// disconnect all flickables
void UCViewItemsAttachedPrivate::clearFlickablesList()
{
    Q_Q(UCViewItemsAttached);
    Q_FOREACH(const QPointer<QQuickFlickable> &flickable, flickables) {
        if (flickable.data()) {
            QObject::disconnect(flickable.data(), &QQuickFlickable::movementStarted,
                                q, &UCViewItemsAttached::unbindItem);
            QObject::disconnect(flickable.data(), &QQuickFlickable::flickStarted,
                                q, &UCViewItemsAttached::unbindItem);
        }
    }
    flickables.clear();
}

// connect all flickables
void UCViewItemsAttachedPrivate::buildFlickablesList()
{
    Q_Q(UCViewItemsAttached);
    QQuickItem *item = qobject_cast<QQuickItem*>(q->parent());
    if (!item) {
        return;
    }
    clearFlickablesList();
    while (item) {
        QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(item);
        if (flickable) {
            QObject::connect(flickable, &QQuickFlickable::movementStarted,
                             q, &UCViewItemsAttached::unbindItem);
            QObject::connect(flickable, &QQuickFlickable::flickStarted,
                             q, &UCViewItemsAttached::unbindItem);
            flickables << flickable;
        }
        item = item->parentItem();
    }
}

void UCViewItemsAttachedPrivate::clearChangesList()
{
    // clear property change objects
    qDeleteAll(changes);
    changes.clear();
}

void UCViewItemsAttachedPrivate::buildChangesList(const QVariant &newValue)
{
    // collect all ascendant flickables
    Q_Q(UCViewItemsAttached);
    QQuickItem *item = qobject_cast<QQuickItem*>(q->parent());
    if (!item) {
        return;
    }
    clearChangesList();
    while (item) {
        QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(item);
        if (flickable) {
            PropertyChange *change = new PropertyChange(item, "interactive");
            PropertyChange::setValue(change, newValue);
            changes << change;
        }
        item = item->parentItem();
    }
}

/*!
 * \qmltype ViewItems
 * \instantiates UCViewItemsAttached
 * \inqmlmodule Ubuntu.Components 1.2
 * \ingroup unstable-ubuntu-listitems
 * \since Ubuntu.Components 1.2
 * \brief A set of properties attached to the ListItem's parent item or ListView.
 *
 * These properties are attached to the parent item of the ListItem, or to
 * ListView, when the component is used as delegate.
 */
UCViewItemsAttached::UCViewItemsAttached(QObject *owner)
    : QObject(owner)
    , d_ptr(new UCViewItemsAttachedPrivate(this))
{
    if (owner->inherits("QQuickListView")) {
        d_ptr->listView = static_cast<QQuickFlickable*>(owner);
    }
    // listen readyness
    QQmlComponentAttached *attached = QQmlComponent::qmlAttachedProperties(owner);
    connect(attached, &QQmlComponentAttached::completed, this, &UCViewItemsAttached::completed);
}

UCViewItemsAttached::~UCViewItemsAttached()
{
}

UCViewItemsAttached *UCViewItemsAttached::qmlAttachedProperties(QObject *owner)
{
    return new UCViewItemsAttached(owner);
}

// register item to be rebound
bool UCViewItemsAttached::listenToRebind(UCListItem *item, bool listen)
{
    // we cannot bind the item until we have an other one bound
    bool result = false;
    Q_D(UCViewItemsAttached);
    if (listen) {
        if (d->boundItem.isNull() || (d->boundItem == item)) {
            d->boundItem = item;
            // rebuild flickable list
            d->buildFlickablesList();
            result = true;
        }
    } else if (d->boundItem == item) {
        d->boundItem.clear();
        result = true;
    }
    return result;
}

// reports true if any of the ascendant flickables is moving
bool UCViewItemsAttached::isMoving()
{
    Q_D(UCViewItemsAttached);
    Q_FOREACH(const QPointer<QQuickFlickable> &flickable, d->flickables) {
        if (flickable && flickable->isMoving()) {
            return true;
        }
    }
    return false;
}

// returns true if the given ListItem is bound to listen on moving changes
bool UCViewItemsAttached::isBoundTo(UCListItem *item)
{
    Q_D(UCViewItemsAttached);
    return d->boundItem == item;
}

/*
 * Disable/enable interactive flag for the ascendant flickables. The item is used
 * to detect whether the same item is trying to enable the flickables which disabled
 * it before. The enabled/disabled states are not equivalent to the enabled/disabled
 * state of the interactive flag.
 * When disabled, always the last item disabling will be kept as active disabler,
 * and only the active disabler can enable (restore) the interactive flag state.
 */
void UCViewItemsAttached::disableInteractive(UCListItem *item, bool disable)
{
    Q_D(UCViewItemsAttached);
    if (disable) {
        // disabling or re-disabling
        d->disablerItem = item;
        if (d->globalDisabled == disable) {
            // was already disabled, leave
            return;
        }
        d->globalDisabled = true;
    } else if (d->globalDisabled && d->disablerItem == item) {
        // the one disabled it will enable
        d->globalDisabled = false;
        d->disablerItem.clear();
    } else {
        // !disabled && (!globalDisabled || item != d->disablerItem)
        return;
    }
    if (disable) {
        // (re)build changes list with disabling the interactive value
        d->buildChangesList(false);
    } else {
        d->clearChangesList();
    }
}

void UCViewItemsAttached::unbindItem()
{
    Q_D(UCViewItemsAttached);
    if (d->boundItem) {
        // snap out before we unbind

        UCListItemPrivate::get(d->boundItem)->snapOut();
        d->boundItem.clear();
    }
    // clear binding list
    d->clearFlickablesList();
}

// reports completion, and in case the dragMode is turned on, enters drag mode
void UCViewItemsAttached::completed()
{
    Q_D(UCViewItemsAttached);
    d->ready = true;
    if (d->draggable) {
        d->enterDragMode();
    } else {
        d->leaveDragMode();
    }
}

// set the drag area so we can position the handler accordingly
void UCViewItemsAttachedPrivate::watchDragAreaPosition(UCListItemStyle *styleItem)
{
    Q_Q(UCViewItemsAttached);
    if (!styleItem->m_dragPanel) {
        // connect dragPanelChanged() to be able to watch its size changes.
        QObject::connect(styleItem, SIGNAL(dragPanelChanged()),
                q, SLOT(_q_dragPanelUpdated()), Qt::DirectConnection);
    } else {
        _q_dragPanelUpdated(styleItem);
    }
}

// dragPanel updated, watch its x coordinate changes
void UCViewItemsAttachedPrivate::_q_dragPanelUpdated(UCListItemStyle *style)
{
    Q_Q(UCViewItemsAttached);
    if (!style) {
        style = qobject_cast<UCListItemStyle*>(q->sender());
    }
    if (style) {
        QObject::connect(style->m_dragPanel, SIGNAL(xChanged()),
                q, SLOT(_q_setDragAreaPosition()), Qt::DirectConnection);
        _q_setDragAreaPosition(style->m_dragPanel);
    }
}

// dragPanel's coordinates changed, update drag area
void UCViewItemsAttachedPrivate::_q_setDragAreaPosition(QQuickItem *panel)
{
    Q_Q(UCViewItemsAttached);
    if (!panel) {
        panel = qobject_cast<QQuickItem*>(q->sender());
    }
    if (listView && panel) {
        QPointF mappedPos = listView->mapFromItem(panel, panel->position());
        bool updated = false;
        if (mappedPos.x() != dragAreaRect.x()) {
            dragAreaRect.setX(mappedPos.x());
            updated = true;
        }
        if (panel->width() != dragAreaRect.width()) {
            dragAreaRect.setWidth(panel->width());
            updated = true;
        }
        if (dragArea && updated) {
            // update anchors
            dragArea->updateArea(dragAreaRect);
        }
    }
}

/*!
 * \qmlattachedproperty bool ViewItems::selectMode
 * The property drives whether list items are selectable or not.
 *
 * When set, the ListItems of the Item the property is attached to will enter into
 * selection state. ListItems provide a visual clue which can be used to toggle
 * the selection state of each, which in order will be reflected in the
 * \l {ViewItems::selectedIndices}{ViewItems.selectedIndices} list.
 */
bool UCViewItemsAttached::selectMode() const
{
    Q_D(const UCViewItemsAttached);
    return d->selectable;
}
void UCViewItemsAttached::setSelectMode(bool value)
{
    Q_D(UCViewItemsAttached);
    if (d->selectable == value) {
        return;
    }
    d->selectable = value;
    Q_EMIT selectModeChanged();
}

/*!
 * \qmlattachedproperty list<int> ViewItems::selectedIndices
 * The property contains the indexes of the ListItems marked as selected. The
 * indexes are model indexes when used in ListView, and child indexes in other
 * components. The property being writable, initial selection configuration
 * can be provided for a view, and provides ability to save the selection state.
 */
QList<int> UCViewItemsAttached::selectedIndices() const
{
    Q_D(const UCViewItemsAttached);
    return d->selectedList.toList();
}
void UCViewItemsAttached::setSelectedIndices(const QList<int> &list)
{
    Q_D(UCViewItemsAttached);
    if (d->selectedList.toList() == list) {
        return;
    }
    d->selectedList = QSet<int>::fromList(list);
    Q_EMIT selectedIndicesChanged();
}

bool UCViewItemsAttachedPrivate::addSelectedItem(UCListItem *item)
{
    int index = UCListItemPrivate::get(item)->index();
    if (!selectedList.contains(index)) {
        selectedList.insert(index);
        Q_EMIT q_ptr->selectedIndicesChanged();
        return true;
    }
    return false;
}
bool UCViewItemsAttachedPrivate::removeSelectedItem(UCListItem *item)
{
    if (selectedList.remove(UCListItemPrivate::get(item)->index()) > 0) {
        Q_EMIT q_ptr->selectedIndicesChanged();
        return true;
    }
    return false;
}

bool UCViewItemsAttachedPrivate::isItemSelected(UCListItem *item)
{
    return selectedList.contains(UCListItemPrivate::get(item)->index());
}

/*!
 * \qmlattachedproperty bool ViewItems::dragMode
 * The property drives the dragging mode of the ListItems within a ListView. It
 * has no effect on any other parent of the ListItem.
 *
 * When set, ListItem content will be disabled and a panel will be shown enabling
 * the dragging mode. The items can be dragged by dragging this handler only.
 * The feature can be activated same time with \l ListItem::selectMode.
 *
 * The panel is configured by the style.
 *
 * \sa ListItemStyle, draggingUpdated
 */

/*!
 * \qmlattachedsignal ViewItems::draggingUpdated(ListItemDrag event)
 * The signal is emitted whenever a dragging related event occurrs. The \b event.status
 * specifies the dragging event type. Depending on the type, the ListItemDrag event
 * properties will have the following meaning:
 * \table
 * \header
 *  \li status
 *  \li from
 *  \li to
 *  \li minimumIndex
 *  \li maximumIndex
 * \row
 *  \li Started
 *  \li the index of the item to be dragged
 *  \li -1
 *  \li default (-1), can be changed to restrict moves
 *  \li default (-1), can be changed to restrict moves
 * \row
 *  \li Moving
 *  \li source index from where the item dragged from
 *  \li destination index where the item can be dragged to
 *  \li the same value set at \e Started, can be changed
 *  \li the same value set at \e Started, can be changed
 * \row
 *  \li Dropped
 *  \li source index from where the item dragged from
 *  \li destination index where the item can be dragged to
 *  \li the value set at \e Started/Moving, changes are omitted
 *  \li the value set at \e Started/Moving, changes are omitted
 * \endtable
 * Implementations \b {must move the model data} in order to re-order the ListView
 * content. If the move is not acceptable, it must be cancelled by setting
 * \b event.accept to \e false, in which case the dragged index (\b from) will
 * not be updated and next time the signal is emitted will be the same.
 *
 * An example implementation of a live dragging with restrictions:
 * \qml
 * import QtQuick 2.3
 * import Ubuntu.Components 1.2
 *
 * ListView {
 *     width: units.gu(40)
 *     height: units.gu(40)
 *     model: ListModel {
 *         // initiate with random data
 *     }
 *     delegate: ListItem {
 *         // content
 *     }
 *
 *     ViewItems.dragMode: true
 *     ViewItems.onDraggingUpdated: {
 *         if (event.status == ListViewDrag.Started) {
 *             if (event.from < 5) {
 *                 // deny dragging on the first 5 element
 *                 event.accept = false;
 *             } else if (event.from >= 5 && event.from <= 10 &&
 *                        event.to >= 5 && event.to <= 10) {
 *                 // specify the interval
 *                 event.minimumIndex = 5;
 *                 event.maximumIndex = 10;
 *             } else if (event.from > 10) {
 *                 // prevent dragging to the first 11 items area
 *                 event.minimumIndex = 11;
 *             }
 *         } else {
 *             model.move(event.from, event.to, 1);
 *         }
 *     }
 * }
 * \endqml
 *
 * A drag'n'drop implementation might be required when model changes are too
 * expensive, and continuously updating while dragging would cause lot of traffic.
 * The following example illustrates how to implement such a scenario:
 * \qml
 * import QtQuick 2.3
 * import Ubuntu.Components 1.2
 *
 * ListView {
 *    width: units.gu(40)
 *    height: units.gu(40)
 *    model: ListModel {
 *        // initiate with random data
 *    }
 *    delegate: ListItem {
 *        // content
 *    }
 *
 *    ViewItems.dragMode: true
 *    ViewItems.onDraggingUpdated: {
 *        if (event.direction == ListItemDrag.Dropped) {
 *            // this is the last event, so drop the item
 *            model.move(event.from, event.to, 1);
 *        } else if (event.direction != ListItemDrag.Started) {
 *            // do not accept the moving events, so drag.from will
 *            // always contain the original drag index
 *            event.accept = false;
 *        }
 *    }
 * }
 * \endqml
 *
 * \note Do not forget to set \b{event.accept} to false in \b draggingUpdated in
 * case the drag event handling is not accepted, otherwise the system will not
 * know whether the move has been performed or not, and selected indexes will
 * not be synchronized properly.
 */
bool UCViewItemsAttached::dragMode() const
{
    Q_D(const UCViewItemsAttached);
    return d->draggable;
}
void UCViewItemsAttached::setDragMode(bool value)
{
    Q_D(UCViewItemsAttached);
    if (d->draggable == value) {
        return;
    }
    if (value) {
        /*
         * The dragging works only if the ListItem is used inside a ListView, and the
         * model used is a list, a ListModel or a derivate of QAbstractItemModel. Do
         * not enable dragging if these conditions are not fulfilled.
         */
        if (!d->listView) {
            qmlInfo(parent()) << UbuntuI18n::instance().tr("dragging mode requires ListView");
            return;
        }
        QVariant modelValue = d->listView->property("model");
        if (!modelValue.isValid()) {
            return;
        }
    }
    d->draggable = value;
    if (d->draggable) {
        d->enterDragMode();
    } else {
        d->leaveDragMode();
    }
    Q_EMIT dragModeChanged();
}

void UCViewItemsAttachedPrivate::enterDragMode()
{
    if (dragArea) {
        dragArea->reset();
        return;
    }
    dragArea = new ListItemDragArea(listView);
    dragArea->init(dragAreaRect);
}

void UCViewItemsAttachedPrivate::leaveDragMode()
{
    if (dragArea) {
        dragArea->setEnabled(false);
    }
}

// returns true when the draggingUpdated signal handler is implemented or a function is connected to it
bool UCViewItemsAttachedPrivate::isDraggingUpdatedConnected()
{
    Q_Q(UCViewItemsAttached);
    static QMetaMethod method = QMetaMethod::fromSignal(&UCViewItemsAttached::draggingUpdated);
    static int signalIdx = QMetaObjectPrivate::signalIndex(method);
    return QObjectPrivate::get(q)->isSignalConnected(signalIdx);
}

// updates the selected indices list in ViewAttached which is changed due to dragging
void UCViewItemsAttachedPrivate::updateSelectedIndices(int fromIndex, int toIndex)
{
    if (selectedList.count() == listView->property("count").toInt()) {
        // all indices selected, no need to reorder
        return;
    }

    Q_Q(UCViewItemsAttached);
    bool isFromSelected = selectedList.contains(fromIndex);
    if (isFromSelected) {
        selectedList.remove(fromIndex);
        Q_EMIT q->selectedIndicesChanged();
    }
    // direction is -1 (forwards) or 1 (backwards)
    int direction = (fromIndex < toIndex) ? -1 : 1;
    int i = (direction < 0) ? fromIndex + 1 : fromIndex - 1;
    // loop thru the selectedIndices and fix all indices
    while (1) {
        if (((direction < 0) && (i > toIndex)) ||
            ((direction > 0) && (i < toIndex))) {
            break;
        }

        if (selectedList.contains(i)) {
            selectedList.remove(i);
            selectedList.insert(i + direction);
            Q_EMIT q->selectedIndicesChanged();
        }
        i -= direction;
    }
    if (isFromSelected) {
        selectedList.insert(toIndex);
        Q_EMIT q->selectedIndicesChanged();
    }
}

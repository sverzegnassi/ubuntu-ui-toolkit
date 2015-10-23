/*
 * Copyright 2015 Canonical Ltd.
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

import QtQuick 2.4
import Ubuntu.Components 1.3
import "../ListItems/1.3"

/*!
    \qmltype BottomEdgeHint
    \inqmlmodule Ubuntu.Components 1.3
    \ingroup ubuntu
    \inherits Item
    \brief The BottomEdgeHint shows the availability of extra features
    available from the bottom edge of the application.

    It displays either a label or an icon at the bottom of the application.

    It has 3 states: Faded, Idle or Hinted. When Idle, part of it is still visible
    hinting the existence of the bottom edge.

    When used with a mouse it acts like a button. The typical action associated
    with clicking on it should be revealing the extra features provided by the
    bottom edge.

    Example:
    \qml
    BottomEdgeHint {
        id: bottomEdgeHint
        text: i18n.tr("Favorites")
        onClicked: revealBottomEdge()
    }
    \endqml

    The component is styled through \b BottomEdgeHintStyle.
*/
StyledItem {
    id: bottomEdgeHint

    anchors.bottom: parent.bottom

    /*!
       This handler is called when there is a mouse click on the BottomEdgeHint
       and the BottomEdgeHint is not disabled.
    */
    signal clicked()

    Keys.onEnterPressed: clicked()
    Keys.onReturnPressed: clicked()

    /*!
      \qmlproperty string text
      The label displayed by the BottomEdgeHint.
     */
    property string text

    /*!
      \qmlproperty url iconSource
      The icon displayed by the BottomEdgeHint.

      This is the URL of any image file.
      If both iconSource and iconName are defined, iconName will be ignored.
     */
    property url iconSource

    /*!
      \qmlproperty string iconName
      The icon associated with the BottomEdgeHint in the icon theme.

      If both iconSource and iconName are defined, iconName will be ignored.
     */
    property string iconName

    /*!
      The property holds the flickable, which when flicked hides the hint.
      \e Faded state is reached when this property is set to a Flickable
      which is flicking or moving. It is recommended to set the property
      when the hint is placed above a flickable content. Defaults to null.
      */
    property Flickable flickable: null

    /*!
      \qmlproperty string state
      BottomEdgeHint can take 3 states of visibility: "Faded", "Idle" and "Hinted".

      When \e Faded, the hint is not shown at all. When \e Hinted, the full hint
      with its content is shown. When \e Idle, only part of the hint is visible
      leaving more space for application content. \e Idle extends the empty state.

      Defaults to \e Idle.
     */
    state: "Idle"

    styleName: "BottomEdgeHintStyle"
}

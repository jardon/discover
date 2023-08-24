/*
 *   SPDX-FileCopyrightText: 2012 Aleix Pol Gonzalez <aleixpol@blue-systems.com>
 *   SPDX-FileCopyrightText: 2023 ivan tkachenko <me@ratijas.tk>
 *
 *   SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami 2 as Kirigami

QQC2.Control {
    id: root

    property alias text: theLabel.text
    property real progress: 1.0

    readonly property bool inProgress: progress > 0

    padding: Kirigami.Units.smallSpacing * 1.5

    background: Item {
        visible: root.inProgress
        Rectangle {
            color: Kirigami.Theme.disabledTextColor
            border.width: 1
            border.color: Qt.darker(Kirigami.Theme.disabledTextColor)
            anchors.fill: parent
            radius: root.padding
        }

        Rectangle {
            anchors {
                top: parent.top
                left: parent.left
                bottom: parent.bottom
                margins: 1
            }
            color: Kirigami.Theme.highlightColor
            radius: root.padding - 2
            width: Math.round((parent.width - anchors.margins * 2) * Math.max(0, Math.min(1, root.progress)))
            visible: width >= radius * 2
        }
    }

    contentItem: QQC2.Label {
        id: theLabel
        horizontalAlignment: Text.AlignHCenter
        color: root.inProgress ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
    }
}

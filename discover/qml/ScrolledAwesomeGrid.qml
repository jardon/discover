/*
 *   Copyright (C) 2012 Aleix Pol Gonzalez <aleixpol@blue-systems.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library/Lesser General Public License
 *   version 2, or (at your option) any later version, as published by the
 *   Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library/Lesser General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 2.1
import org.kde.plasma.components 2.0

Item {
    property alias header: gridRepeater.header
    property alias footer: gridRepeater.footer
    property alias delegate: gridRepeater.delegate
    property alias model: gridRepeater.model
    property alias actualWidth: gridRepeater.actualWidth
    property alias cellWidth: gridRepeater.cellWidth
    property alias minCellWidth: gridRepeater.minCellWidth
    
    AwesomeGrid {
        id: gridRepeater
        anchors {
            fill: parent
            rightMargin: scroll.width
        }
        header: parentItem.header
    }
    
    Rectangle {
        id: scroll
        color: "red"
        width: 5
        height: 100
//         orientation: Qt.Vertical
//         flickableItem: gridRepeater
//         anchors {
//             top: parent.top
//             right: parent.right
//             bottom: parent.bottom
//         }
    }
}

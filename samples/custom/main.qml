/*
    This file is part of Quick Qanava library.

    Copyright (C) 2008-2015 Benoit AUTHEMAN

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.5
import QtQuick.Extras 1.4
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.2

import QuickQanava 2.0 as Qan
import "qrc:/QuickQanava" as Qan
import "." as Qan

Qan.Navigable {
    anchors.fill: parent
    id: container

    Qan.Graph {
        parent: container.containerItem
        id: graph
        objectName: "graph"
        anchors.fill: parent

        property var customNodeDelegate: Qt.createComponent( "qrc:/CustomNode.qml" )
        property var diamondNodeDelegate: Qt.createComponent( "qrc:/DiamondNode.qml" )

        Component.onCompleted: {
            var n1 = graph.insertNode( )
            n1.label = "Rectangle"
            graph.insertNode( customNodeDelegate )
            var n2 = graph.insertNode( diamondNodeDelegate )
            n2.label = "Diamond"

            graph.insertEdge( edgeDelegate, n1, n2 )

            var g = graph.insertGroup( )
        }
    }
}


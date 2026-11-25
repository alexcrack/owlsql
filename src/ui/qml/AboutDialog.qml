// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import OwlSql.Style
import OwlSql.Theme

Dialog {
    id: root

    title: qsTr("About OwlSql")
    modal: true
    standardButtons: Dialog.Ok
    width: 400

    RowLayout {
        anchors.fill: parent
        spacing: Theme.space.l

        Image {
            Layout.alignment: Qt.AlignTop
            source: "qrc:/owlsql/logo/owlsql.svg"
            sourceSize: Qt.size(48, 48)
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.xs

            Label {
                text: qsTr("OwlSql %1").arg(Qt.application.version)
                font: Theme.font.title
            }
            Label {
                Layout.fillWidth: true
                text: qsTr(
                          "A fast, modern database client for PostgreSQL, MySQL / MariaDB and SQLite.")
                color: Theme.color.textSecondary
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.s
                text: qsTr("Free software under the GNU General Public License v3 or later.")
                color: Theme.color.textMuted
                font: Theme.font.uiSmall
                wrapMode: Text.Wrap
            }
        }
    }
}

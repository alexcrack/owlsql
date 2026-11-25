# SPDX-License-Identifier: GPL-3.0-or-later
# QML module OwlSql.Style — the custom Qt Quick Controls style built on QtQuick.Templates and
# Theme tokens. App QML imports it instead of QtQuick.Controls (compile-time style selection);
# it is also set as the runtime style so attached ToolTips and Qt's own dialogs match.
# Controls not implemented here fall back to the Basic style.

qt_add_library(owlsql_style STATIC)

set_source_files_properties(
    qml/ButtonContent.qml qml/FocusRing.qml qml/PopupBackground.qml
    PROPERTIES QT_QML_INTERNAL_TYPE TRUE)

qt_add_qml_module(owlsql_style
    URI OwlSql.Style
    VERSION 1.0
    IMPORTS
        QtQuick.Controls.Basic
    DEPENDENCIES
        OwlSql.Theme
    QML_FILES
        qml/ApplicationWindow.qml
        qml/BusyIndicator.qml
        qml/Button.qml
        qml/ButtonContent.qml
        qml/CheckBox.qml
        qml/ComboBox.qml
        qml/Dialog.qml
        qml/DialogButtonBox.qml
        qml/FocusRing.qml
        qml/Frame.qml
        qml/ItemDelegate.qml
        qml/Label.qml
        qml/Menu.qml
        qml/MenuItem.qml
        qml/MenuSeparator.qml
        qml/Pane.qml
        qml/Popup.qml
        qml/PopupBackground.qml
        qml/ProgressBar.qml
        qml/RadioButton.qml
        qml/ScrollBar.qml
        qml/ScrollView.qml
        qml/SpinBox.qml
        qml/SplitView.qml
        qml/Switch.qml
        qml/TabBar.qml
        qml/TabButton.qml
        qml/TextArea.qml
        qml/TextField.qml
        qml/ToolButton.qml
        qml/ToolTip.qml
)

target_link_libraries(owlsql_style PRIVATE Qt6::Quick Qt6::QuickControls2 owlsql_theme)
owl_set_target_options(owlsql_style)

# SPDX-License-Identifier: GPL-3.0-or-later
# M1.2 editor feasibility spike (throwaway; lives on branch spike/m1.2-editor only).
qt_add_executable(editor_spike main.cpp sqlcorpus.h sqlcorpus.cpp)
qt_add_qml_module(editor_spike
    URI EditorSpike
    VERSION 1.0
    QML_FILES Spike.qml)
find_package(Qt6 REQUIRED COMPONENTS Test)
target_link_libraries(editor_spike PRIVATE
    Qt6::Quick Qt6::QuickControls2 Qt6::Test KF6::SyntaxHighlighting)

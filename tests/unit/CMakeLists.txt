# SPDX-License-Identifier: GPL-3.0-or-later
owl_add_unit_test(tst_version SOURCES tst_version.cpp LIBS Owl::Core)
owl_add_unit_test(tst_theme SOURCES tst_theme.cpp LIBS owlsql_theme)
owl_add_unit_test(tst_ui_smoke SOURCES tst_ui_smoke.cpp
    LIBS owlsql_theme owlsql_themeplugin owlsql_uiplugin Qt6::Quick Qt6::QuickControls2)

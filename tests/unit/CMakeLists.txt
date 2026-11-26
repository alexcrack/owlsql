# SPDX-License-Identifier: GPL-3.0-or-later
owl_add_unit_test(tst_version SOURCES tst_version.cpp LIBS Owl::Core)
owl_add_unit_test(tst_theme SOURCES tst_theme.cpp LIBS owlsql_theme)
owl_add_unit_test(tst_ui_smoke SOURCES tst_ui_smoke.cpp
    LIBS owlsql_theme owlsql_themeplugin owlsql_styleplugin owlsql_app owlsql_appplugin owlsql_uiplugin Qt6::Quick Qt6::QuickControls2)
owl_add_unit_test(tst_commands SOURCES tst_commands.cpp LIBS owlsql_app)
owl_add_unit_test(tst_appdata SOURCES tst_appdata.cpp LIBS owlsql_app Qt6::Sql)
owl_add_unit_test(tst_settings SOURCES tst_settings.cpp LIBS owlsql_app)
owl_add_unit_test(tst_singleinstance SOURCES tst_singleinstance.cpp LIBS owlsql_app Qt6::Network)

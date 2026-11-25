# SPDX-License-Identifier: GPL-3.0-or-later
# owl_set_target_options(<target>): warnings and Qt hygiene defines shared by all OwlSql targets.

function(owl_set_target_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(OWL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
            -Wcast-align -Wnull-dereference -Wimplicit-fallthrough)
        if(OWL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    target_compile_definitions(${target} PRIVATE
        QT_NO_CAST_TO_ASCII
        QT_NO_URL_CAST_FROM_STRING
        QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
        QT_NO_FOREACH
        QT_USE_QSTRINGBUILDER
        QT_DISABLE_DEPRECATED_UP_TO=0x060800)
endfunction()

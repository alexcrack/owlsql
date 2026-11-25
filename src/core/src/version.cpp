// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/version.h"

namespace owl::core {

QString versionString()
{
    return QStringLiteral("%1.%2.%3").arg(versionMajor).arg(versionMinor).arg(versionPatch);
}

} // namespace owl::core

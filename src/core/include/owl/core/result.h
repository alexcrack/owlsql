// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/dberror.h"

#include <tl/expected.hpp>

namespace owl {

// Result of a driver operation: a value or a DbError (decision 078: tl::expected while on C++20;
// switch to std::expected with C++23).
//
//   Result<int> count() { if (bad) return fail(DbError{...}); return 42; }
//   if (auto r = count(); r) use(*r); else report(r.error());
template<typename T>
using Result = tl::expected<T, DbError>;

inline tl::unexpected<DbError> fail(DbError error)
{
    return tl::unexpected<DbError>(std::move(error));
}

inline tl::unexpected<DbError> fail(DbError::Category category, QString message)
{
    DbError error;
    error.category = category;
    error.message = std::move(message);
    return tl::unexpected<DbError>(std::move(error));
}

} // namespace owl

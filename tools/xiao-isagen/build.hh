#pragma once

#include "ast.hh"
#include "sexp.hh"

#include <vector>

namespace isagen
{
    [[nodiscard]] Isa build( const List& forms );
} // namespace isagen

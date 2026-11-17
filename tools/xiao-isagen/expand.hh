#pragma once

#include "sexp.hh"

#include <vector>

namespace isagen
{
    [[nodiscard]] std::vector< SExp > expand( std::vector< SExp > forms );
} // namespace isagen

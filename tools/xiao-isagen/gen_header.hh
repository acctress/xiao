#pragma once

#include "ast.hh"

#include <string>
#include <vector>

namespace isagen
{
    [[nodiscard]] std::string gen_header( const Isa& isa );
} // namespace isagen

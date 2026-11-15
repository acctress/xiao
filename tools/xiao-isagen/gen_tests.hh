#pragma once

#include "ast.hh"

#include <string>
#include <vector>

namespace isagen
{
    [[nodiscard]] std::string gen_tests( const std::vector< Inst >& insts );
} // namespace isagen

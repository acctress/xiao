#pragma once

#include <unordered_map>
#include <cstdint>
#include <string>

namespace xiao::jit
{
    class JITBuilder
    {
    public:
        ~JITBuilder( ) = default;

        JITBuilder& symbol( const std::string& name, void* ptr )
        {
            m_symbols[ name ] = ptr;
            return *this;
        }

        [[nodiscard]] void* lookup( const std::string& name ) const
        {
            if ( auto iter = m_symbols.find( name ); iter != m_symbols.end( ) ) return iter->second;
            return nullptr;
        }

    private:
        std::unordered_map< std::string, void* > m_symbols { };
    };
} // namespace xiao::jit
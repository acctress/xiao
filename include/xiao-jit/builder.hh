#pragma once

#include <xiao/codegen/isa.hh>
#include <xiao/module/module.hh>
#include <xiao-jit/memory.hh>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#if !defined( _WIN32 )
#include <dlfcn.h>
#endif

namespace xiao::jit
{
    using SymbolLookupFn = std::function< const void*( std::string_view ) >;

    namespace detail
    {
        [[nodiscard]] inline const void* default_symbol_lookup( const std::string_view name )
        {
            const std::string n { name };
#if defined( _WIN32 )
            for ( const auto* mod : { static_cast< const wchar_t* >( nullptr ), L"ucrtbase.dll", L"ucrtbased.dll" } )
            {
                const auto handle = GetModuleHandleW( mod );
                if ( !handle ) continue;
                if ( const auto p = GetProcAddress( handle, n.c_str( ) ) ) return reinterpret_cast< const void* >( p );
            }
            return nullptr;
#else
            return dlsym( RTLD_DEFAULT, n.c_str( ) );
#endif
        }
    } // namespace detail

    class JITBuilder
    {
    public:
        explicit JITBuilder( std::unique_ptr< codegen::TargetIsa > isa ) : m_isa { std::move( isa ) }
        { m_lookup_fns.emplace_back( detail::default_symbol_lookup ); }

        JITBuilder& symbol( std::string name, const void* ptr )
        {
            m_symbols.insert_or_assign( std::move( name ), ptr );
            return *this;
        }

        JITBuilder& symbol_lookup_fn( SymbolLookupFn fn )
        {
            m_lookup_fns.push_back( std::move( fn ) );
            return *this;
        }

    private:
        friend class JITModule;

        std::unique_ptr< codegen::TargetIsa >                                               m_isa;
        std::unordered_map< std::string, const void*, module::StringHash, std::equal_to<> > m_symbols { };
        std::vector< SymbolLookupFn >                                                       m_lookup_fns { };
    };
} // namespace xiao::jit

#include "expand.hh"

#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace isagen
{
    namespace
    {
        struct Binding
        {
            std::string_view name { };
            const SExp*      value { };
        };

        using Env = std::vector< Binding >;

        constexpr bool is_ident_char( const char c ) noexcept
        { return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_'; }

        [[nodiscard]] bool is_for( const SExp& form ) noexcept
        {
            if ( const auto* list = std::get_if< List >( &form.value ); list && !list->empty( ) )
            {
                const auto* head = std::get_if< Symbol >( &list->front( ).value );
                return head && head->name == "for";
            }

            return false;
        }

        [[nodiscard]] const SExp* lookup( const Env& env, const std::string_view name ) noexcept
        {
            const auto iter = std::ranges::find( env, name, &Binding::name );
            return iter != env.end( ) ? iter->value : nullptr;
        }

        [[nodiscard]] std::optional< std::string_view > whole_var( const std::string_view text ) noexcept
        {
            if ( !text.starts_with( '$' ) ) return std::nullopt;
            const auto rest = text.substr( 1 );

            if ( rest.starts_with( '{' ) && rest.ends_with( '}' ) )
            {
                const auto name = rest.substr( 1, rest.size( ) - 2 );
                if ( name.empty( ) || name.contains( '}' ) ) return std::nullopt;
                return name;
            }

            if ( !rest.empty( ) && std::ranges::all_of( rest, is_ident_char ) ) return rest;
            return std::nullopt;
        }

        void append_text( std::string& out, const SExp& value, const Location& loc )
        {
            std::visit(
                [ &out, loc ]< typename Ty >( Ty&& val )
                {
                    using T = std::decay_t< Ty >;
                    if constexpr ( std::is_same_v< T, Symbol > ) { out += val.name; }
                    else if constexpr ( std::is_same_v< T, String > ) { out += val.value; }
                    else if constexpr ( std::is_same_v< T, List > )
                        fail( loc, "a list value cannot be used inside text" );
                },
                value.value );
        }

        [[nodiscard]] std::string interpolate( const std::string_view text, const Env& env, const Location& loc )
        {
            std::string out { };
            out.reserve( text.size( ) );

            std::size_t i { };
            for ( auto pos = text.find( '$' ); pos != std::string_view::npos; pos = text.find( '$', i ) )
            {
                out += text.substr( i, pos - i );

                std::string_view name { };
                std::size_t      end { };
                if ( pos + 1 < text.size( ) && text[ pos + 1 ] == '{' )
                {
                    const auto close = text.find( '}', pos + 2 );
                    if ( close == std::string_view::npos ) fail( loc, "missing '}' in variable reference" );
                    name = text.substr( pos + 2, close - pos - 2 );
                    end  = close + 1;
                }
                else
                {
                    end = pos + 1;
                    while ( end < text.size( ) && is_ident_char( text[ end ] ) ) ++end;
                    name = text.substr( pos + 1, end - pos - 1 );
                }

                if ( const auto* value = name.empty( ) ? nullptr : lookup( env, name ) ) append_text( out, *value, loc );
                else
                    out += text.substr( pos, end - pos );

                i = end;
            }

            out += text.substr( i );
            return out;
        }

        void subst( const SExp& node, const Env& env, List& out )
        {
            std::visit(
                Overloaded {
                    [ & ]( const Symbol& sym )
                    {
                        if ( const auto var = whole_var( sym.name ) )
                        {
                            if ( const auto* value = lookup( env, *var ) )
                            {
                                if ( const auto* list = std::get_if< List >( &value->value ) ) out.append_range( *list );
                                else
                                {
                                    out.push_back( *value );
                                    out.back( ).loc = node.loc;
                                }
                                return;
                            }
                        }

                        out.push_back( SExp { Symbol { interpolate( sym.name, env, node.loc ) }, node.loc } );
                    },
                    [ & ]( const String& str ) { out.push_back( SExp { String { interpolate( str.value, env, node.loc ) }, node.loc } ); },
                    [ & ]( const List& list )
                    {
                        List items;
                        items.reserve( list.size( ) );
                        for ( const auto& child : list ) subst( child, env, items );
                        out.push_back( SExp { std::move( items ), node.loc } );
                    } },
                node.value );
        }

        void expand_into( SExp form, List& out );

        void expand_for( const SExp& form, List& out )
        {
            const auto& items = std::get< List >( form.value );
            if ( items.size( ) < 4 ) fail( form.loc, "for needs variables, rows and at least one body form" );

            const auto* vars = std::get_if< List >( &items[ 1 ].value );
            if ( !vars ) fail( items[ 1 ].loc, "for variables must be a list" );

            std::vector< std::string_view > names;
            names.reserve( vars->size( ) );
            for ( const auto& [ value, loc ] : *vars )
            {
                const auto* sym = std::get_if< Symbol >( & value );
                if ( !sym ) fail( loc, "for variable must be a symbol" );
                if ( std::ranges::contains( names, sym->name ) ) fail( loc, std::format( "duplicate for variable '{}'", sym->name ) );
                names.push_back( sym->name );
            }

            const auto* rows = std::get_if< List >( &items[ 2 ].value );
            if ( !rows ) fail( items[ 2 ].loc, "for rows must be a list" );

            for ( const auto& row_form : *rows )
            {
                const auto* row = std::get_if< List >( &row_form.value );
                if ( !row ) fail( row_form.loc, "for row must be a list" );
                if ( row->size( ) != names.size( ) )
                    fail( row_form.loc, std::format( "row has {} values but for has {} variables", row->size( ), names.size( ) ) );

                Env env;
                env.reserve( names.size( ) );
                for ( const auto& [ name, value ] : std::views::zip( names, *row ) ) env.push_back( { name, &value } );

                for ( const auto& body : items | std::views::drop( 3 ) )
                {
                    List produced;
                    subst( body, env, produced );
                    for ( auto& result : produced ) expand_into( std::move( result ), out );
                }
            }
        }

        void expand_into( SExp form, List& out )
        {
            if ( is_for( form ) ) expand_for( form, out );
            else
                out.push_back( std::move( form ) );
        }

        void check_unbound( const SExp& node )
        {
            const auto check = [ & ]( const std::string& text )
            {
                for ( auto pos = text.find( '$' ); pos != std::string::npos; pos = text.find( '$', pos + 1 ) )
                    if ( pos + 1 < text.size( ) && ( text[ pos + 1 ] == '{' || is_ident_char( text[ pos + 1 ] ) ) )
                        fail( node.loc, std::format( "unbound variable in '{}'", text ) );
            };

            std::visit(
                Overloaded {
                    [ & ]( const Symbol& sym ) { check( sym.name ); },
                    [ & ]( const String& str ) { check( str.value ); },
                    []( const List& list )
                    {
                        for ( const auto& child : list ) check_unbound( child );
                    } },
                node.value );
        }
    } // namespace

    std::vector< SExp > expand( std::vector< SExp > forms )
    {
        std::vector< SExp > out;
        for ( auto& form : forms ) expand_into( std::move( form ), out );
        for ( const auto& form : out ) check_unbound( form );
        return out;
    }
} // namespace isagen

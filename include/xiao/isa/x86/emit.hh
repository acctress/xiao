#pragma once

#include <xiao/regalloc/regalloc.hh>
#include <xiao/codegen/machbuffer.hh>
#include <xiao/isa/x86/emit_impl.hh>
#include <xiao/isa/x86/inst.hh>
#include <cstdint>

namespace xiao::x86_64
{
    inline void emit( const VCode< X86Inst >& vcode, const regalloc::Allocations& allocs, MachBuffer< LabelUse >& buf )
    {
        for ( uint32_t i { 0 }; i < vcode.blocks.size( ); ++i ) ( void ) buf.new_label( );

        for ( uint32_t bi { 0 }; bi < vcode.blocks.size( ); ++bi )
        {
            buf.bind_label( bi );

            const auto& block = vcode.blocks[ bi ];
            for ( uint32_t ii { 0 }; ii < block.insts_count; ++ii )
            {
                const auto i_idx        = block.insts_offset + ii;
                const auto [ off, cnt ] = vcode.operand_ranges[ i_idx ];
                const auto pregs        = std::span { allocs.ops }.subspan( off, cnt );

                vcode.insts[ i_idx ].emit( buf, pregs );
            }
        }

        buf.resolve_labels( );
    }

} // namespace xiao::x86_64

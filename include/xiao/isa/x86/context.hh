// #pragma once
//
// #include <ranges>
// #include <xiao/codegen/machbuffer.hh>
// #include <xiao/isa/x86/machenv.hh>
// #include <xiao/isa/x86/lower.hh>
// #include <xiao/isa/x86/emit.hh>
// #include <xiao/ir/function.hh>
// #include <xiao/isa/x86/abi.hh>
//
// namespace xiao::x86_64
// {
//     class Context
//     {
//     public:
//         ir::IRFunction func;
//
//         [[nodiscard]] codegen::MachBuffer compile( )
//         {
//             auto env = host_machine_env( );
//             auto abi = host_abi( );
//
//             IRLowerer lowerer { env, abi };
//             auto vcode = lowerer.lower( func );
//
//             auto allocated_pregs = alloc( vcode, env );
//
//             MachBuffer buf;
//             x86_64::emit( vcode, allocs.ops, buf  );
//
//             return buf;
//         }
//
//     private:
//         [[nodiscard]] std::vector< PReg > alloc(
//             const VCode<X86Inst>& vcode,
//             const MachineEnv&     env
//         )
//         {
//             std::vector<PReg> ops;
//             for ( const auto& cnt : vcode.operand_ranges | std::views::values )
//             {
//                 for ( auto i { 0uz }; i < cnt; ++i ) ops.push_back( PReg{ } );
//             }
//
//             return ops;
//         }
//     };
// } // namespace xiao::x86_64
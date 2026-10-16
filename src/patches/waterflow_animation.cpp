#include "waterflow_animation.h"

namespace Patches::WaterflowAnimation::detail
{
    void PatchWaterflowAnimation()
    {
        // patch the main game loop to also update our timer
        {
            REL::Relocation mainLoopTarget{ RELOCATION_ID(35565, 36564), VAR_NUM(0x252, 0x26B) };

            struct MainUpdateCode : Xbyak::CodeGenerator
            {
                explicit MainUpdateCode(const std::uintptr_t a_addr)
                {
                    Xbyak::Label retnLabel;
                    Xbyak::Label funcLabel;
                    Xbyak::Label applicationRunTimeLabel;

                    // Preserve volatile GPRs
                    push(rax);
                    push(rcx);
                    push(rdx);
                    push(r8);
                    push(r9);
                    push(r10);
                    push(r11);

                    // Allocate space for 6 XMM registers (0x60) + 32-byte shadow space (0x20) = 0x80 bytes (maintains 16-byte stack alignment)
                    sub(rsp, 0x80);
                    movdqu(ptr[rsp + 0x20], xmm0);
                    movdqu(ptr[rsp + 0x30], xmm1);
                    movdqu(ptr[rsp + 0x40], xmm2);
                    movdqu(ptr[rsp + 0x50], xmm3);
                    movdqu(ptr[rsp + 0x60], xmm4);
                    movdqu(ptr[rsp + 0x70], xmm5);

                    call(ptr[rip + funcLabel]);

                    movdqu(xmm0, ptr[rsp + 0x20]);
                    movdqu(xmm1, ptr[rsp + 0x30]);
                    movdqu(xmm2, ptr[rsp + 0x40]);
                    movdqu(xmm3, ptr[rsp + 0x50]);
                    movdqu(xmm4, ptr[rsp + 0x60]);
                    movdqu(xmm5, ptr[rsp + 0x70]);
                    add(rsp, 0x80);

                    // Restore volatile GPRs
                    pop(r11);
                    pop(r10);
                    pop(r9);
                    pop(r8);
                    pop(rdx);
                    pop(rcx);
                    pop(rax);

                    // orig code
                    mov(rdx, ptr[rip + applicationRunTimeLabel]);
                    mov(edx, dword[rdx]);

                    jmp(ptr[rip + retnLabel]);

                    L(funcLabel);
                    dq(REX::UNRESTRICTED_CAST<uintptr_t>(update_timer));

                    L(retnLabel);
                    dq(a_addr + 0x6);

                    L(applicationRunTimeLabel);
                    dq(g_ApplicationRunTime.address());
                }
            };

            MainUpdateCode p(mainLoopTarget.address());
            p.ready();

            auto& trampoline = REL::GetTrampoline();
            trampoline.write_jmp6(mainLoopTarget.address(), reinterpret_cast<std::uintptr_t>(trampoline.allocate(p)));
        }

        // patch the water shader to use our timer
        // 107363
        {
            REL::Relocation waterShaderSetupMaterialTarget{ RELOCATION_ID(100602, 107363), VAR_NUM(0x4A9, 0x4BC) };

            struct WaterShaderCode : Xbyak::CodeGenerator
            {
                explicit WaterShaderCode(const std::uintptr_t a_addr)
                {
                    Xbyak::Label retnLabel;
                    Xbyak::Label timerLabel;

                    // enter 14E167C
                    // .text:000000014141CE5C                 movss   xmm0, cs:dword_141EA2E40
                    // .text:000000014141CE64                 movss   dword ptr [r10+rax*4+0Ch], xmm0
                    mov(r9, ptr[rip + timerLabel]);  // r9 is safe to use, unused again until .text:000000014130E13C                 mov     r9, r12
#ifdef SKYRIM_AE
                    movss(xmm0, dword[r9]);
                    movss(dword[r10 + rax * 4 + 0xC], xmm0);
#else
                    movss(xmm1, dword[r9]);
                    movss(dword[rdx + rax * 4 + 0xC], xmm1);
#endif
                    // exit 14E168B
                    jmp(ptr[rip + retnLabel]);

                    L(retnLabel);
                    dq(a_addr + 0xE);

                    L(timerLabel);
                    dq(REX::UNRESTRICTED_CAST<std::uintptr_t>(&g_Timer));
                }
            };

            WaterShaderCode p(waterShaderSetupMaterialTarget.address());
            p.ready();

            auto& trampoline = REL::GetTrampoline();
            trampoline.write_jmp6(waterShaderSetupMaterialTarget.address(), reinterpret_cast<std::uintptr_t>(trampoline.allocate(p)));
        }
    }
}

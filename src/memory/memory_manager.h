#pragma once

namespace Memory::MemoryManager
{
    namespace detail
    {
        // when the global memory manager has a zero-size allocation it returns scratch memory
        // replicate this behavior
        inline std::byte* g_ZeroAddress{ nullptr };

        inline bool IsZeroAddress(void* a_mem) noexcept
        {
            if (a_mem == nullptr) {
                return true;
            }
            if (g_ZeroAddress != nullptr && a_mem == g_ZeroAddress) {
                return true;
            }
            REL::Relocation<std::byte**> zeroAddressPtr{ RELOCATION_ID(514118, 410163) };
            const auto engineZero = *zeroAddressPtr;
            if (engineZero != nullptr) {
                if (a_mem == engineZero) {
                    return true;
                }
                const auto altZero = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(engineZero) ^ 0x10);
                if (a_mem == altZero) {
                    return true;
                }
            }
            return false;
        }

        // MemoryManager::AutoScrapBuffer impl
        // ScrapHeap RAII wrapper
        struct AutoScrapBuffer
        {
            static AutoScrapBuffer* Ctor(AutoScrapBuffer* a_self, const std::size_t a_size, const std::size_t a_alignment)
            {
                if (a_size == 0) {
                    a_self->p_Memory = g_ZeroAddress;
                } else {
                    a_self->p_Memory = Allocator::GetAllocator()->AllocateAligned(a_size, a_alignment);
                }
                return a_self;
            }

            static void Dtor(AutoScrapBuffer* a_self)
            {
                if (!IsZeroAddress(a_self->p_Memory)) {
                    Allocator::GetAllocator()->DeallocateAligned(a_self->p_Memory);
                }
                a_self->p_Memory = nullptr;
            }

            static void Install()
            {
                REL::Relocation ctor{ RELOCATION_ID(66853, 68108) };
                REL::Relocation dtor{ RELOCATION_ID(66854, 68109) };

                ctor.replace_func(0x7B, Ctor);
                dtor.replace_func(VAR_NUM(0x58, 0x54), Dtor);
            }

            void* p_Memory;
        };

        inline void* Allocate(RE::MemoryManager*, const std::size_t a_size, const std::uint32_t a_alignment, const bool a_alignmentRequired)
        {
            if (a_size > 0) {
                return a_alignmentRequired ? Allocator::GetAllocator()->AllocateAligned(a_size, a_alignment) : Allocator::GetAllocator()->Allocate(a_size);
            }

            return g_ZeroAddress;
        }

        inline void* Reallocate(RE::MemoryManager* a_self, void* a_oldMem, const std::size_t a_newSize, const std::uint32_t a_alignment, const bool a_alignmentRequired)
        {
            if (IsZeroAddress(a_oldMem))
                return Allocate(a_self, a_newSize, a_alignment, a_alignmentRequired);

            return a_alignmentRequired ? Allocator::GetAllocator()->ReallocateAligned(a_oldMem, a_newSize, a_alignment) : Allocator::GetAllocator()->Reallocate(a_oldMem, a_newSize);
        }

        inline void Deallocate(RE::MemoryManager*, void* a_mem, const bool a_alignmentRequired)
        {
            if (!IsZeroAddress(a_mem)) {
                if (a_alignmentRequired)
                    Allocator::GetAllocator()->DeallocateAligned(a_mem);
                else
                    Allocator::GetAllocator()->Deallocate(a_mem);
            }
        }

        inline std::size_t Size(RE::MemoryManager*, void* a_mem)
        {
            if (IsZeroAddress(a_mem))
                return 0;

            return Allocator::GetAllocator()->Size(a_mem);
        }

        inline void ReplaceAllocRoutines()
        {
            REL::Relocation allocate{ RELOCATION_ID(66859, 68115) };
            REL::Relocation reallocate{ RELOCATION_ID(66860, 68116) };
            REL::Relocation deallocate{ RELOCATION_ID(66861, 68117) };
            REL::Relocation size{ RELOCATION_ID(66849, 68100) };

            allocate.replace_func(0x247, Allocate);
            reallocate.replace_func(VAR_NUM(0xA7, 0x1F6), Reallocate);
            deallocate.replace_func(0x114, Deallocate);
            size.replace_func(VAR_NUM(0x12A, 0x156), Size);
        }

        inline void StubInit()
        {
            REL::Relocation target{ RELOCATION_ID(66862, 68121) };

            target.write_fill(REL::INT3, VAR_NUM(0x9F, 0x1A7));
            target.write(REL::RET);

            REL::Relocation<std::uint32_t*> initFence{ RELOCATION_ID(514112, 400190) };
            *initFence = 2;

            REL::Relocation<std::byte**> zeroAddressPtr{ RELOCATION_ID(514118, 410163) };
            *zeroAddressPtr = g_ZeroAddress;
        }

        inline void Install()
        {
            g_ZeroAddress = static_cast<std::byte*>(Allocator::GetAllocator()->AllocateAligned(1u << 10, 64));
            std::memset(g_ZeroAddress, 0, 1u << 10);

            StubInit();
            ReplaceAllocRoutines();
            RE::MemoryManager::GetSingleton()->RegisterMemoryManager();
            RE::BSThreadEvent::InitSDM();
        }
    }

    inline void Install()
    {
        detail::Install();
        detail::AutoScrapBuffer::Install();
        REX::INFO("installed global memory manager patch"sv);
    }
}
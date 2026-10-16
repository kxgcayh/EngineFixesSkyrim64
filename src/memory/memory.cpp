#include "memory.h"
#include "allocator.h"
#include "havok_memory_system.h"
#include "memory_manager.h"
#include "renderpass_cache.h"
#include "scaleform_allocator.h"
#include "scrapheap.h"
#include "util/environment.h"

namespace Memory
{
    void Install()
    {
        const bool wineCompat = Settings::General::bWineCompatibility.GetValue() && Util::Environment::IsRunningUnderWine();
        if (Settings::Debug::bDisableTBB.GetValue() || wineCompat) {
            Allocator::SetAllocator(Allocator::CRT);
            REX::INFO("set allocator to CRT"sv);
            if (wineCompat) {
                REX::INFO("wine/crossover detected: using CRT allocator for Darwin/Apple Silicon unified memory optimization"sv);
            }
        } else {
            Allocator::SetAllocator(Allocator::TBB);
            REX::INFO("set allocator to TBB"sv);
        }

        if (Settings::MemoryManager::bOverrideMemoryManager.GetValue())
            MemoryManager::Install();

        if (Settings::MemoryManager::bOverrideScrapHeap.GetValue())
            ScrapHeap::Install();

        if (Settings::MemoryManager::bOverrideScaleformAllocator.GetValue())
            ScaleformAllocator::Install();

        if (Settings::MemoryManager::bOverrideRenderPassCache.GetValue())
            RenderPassCache::Install();

        if (Settings::MemoryManager::bOverrideHavokMemorySystem.GetValue())
            HavokMemorySystem::Install();

        if (Settings::MemoryManager::bReplaceImports.GetValue())
            Allocator::GetAllocator()->ReplaceImports();
    }
}
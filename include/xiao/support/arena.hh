//
// Created by griffith on 17/10/2026.
//

#pragma once

#include <string_view>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include <bit>

namespace xiao::support
{
    //! @brief Heap allocated slab of memory, tracked bump pointer.
    struct Slab {
        std::unique_ptr<std::byte[]> data;
        std::byte*                   cursor;
        std::byte*                   end;

        explicit Slab(const std::size_t size)
            : data{ std::make_unique_for_overwrite<std::byte[]>(size) }
            , cursor{ data.get() }
            , end { data.get() + size } {}

        [[nodiscard]] std::size_t remaining() const noexcept {
            return static_cast<std::size_t>(end - cursor);
        }
    };

    //! @brief Saved position for rewind and marking.
    struct Checkpoint {
        std::size_t slab;       //! Index of the slab in the arena
        std::byte*  cursor;
    };

    class Arena {
    public:
        static constexpr std::size_t default_slab_size = 4096 * 4;

        explicit Arena(const std::size_t slab_size = default_slab_size) noexcept
            : m_slab_size{ slab_size }
        {}

        Arena(Arena&&) noexcept = default;
        Arena& operator=(Arena&&) noexcept = default;

        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;

        ~Arena() = default;

        [[nodiscard]] void* allocate(
            const std::size_t size,
            const std::size_t align = alignof(std::max_align_t)
        ) {
            assert(std::has_single_bit(align) && "alignment must be n^2");

            if (size == 0) [[unlikely]] return nullptr;

            for (;;) {
                if (!m_slabs.empty()) {
                    auto& s = m_slabs.back();
                    auto* p = std::bit_cast<std::byte*>(
                        (std::bit_cast<std::uintptr_t>(s.cursor) + align - 1) & ~(align - 1)
                    );

                    if (p + size <= s.end) {
                        s.cursor = p + size;
                        return p;
                    }
                }

                m_slabs.emplace_back(std::max(size + align, m_slab_size));
            }
        }

        //! @brief Typed allocation of T, constructed in place.
        template <typename T, typename... Args>
        requires std::is_constructible_v<T, Args...> &&
                 std::is_trivially_destructible_v<T>
        [[nodiscard]] T* alloc(Args&&... args) {
            void* mem = allocate(sizeof(T), alignof(T));
            return ::new (mem) T(std::forward<Args>(args)...);
        }

        //! @brief N default initialised array of T
        template <typename T>
        requires std::is_trivially_destructible_v<T>
        [[nodiscard]] std::span<T> alloc_array(const std::size_t n) {
            if (n == 0) return {};

            void* mem = allocate(sizeof(T) * n, alignof(T));
            return { ::new (mem) T[n](), n };
        }

        //! @brief N uninitialised array of T
        template <typename T>
        requires std::is_trivially_destructible_v<T>
        [[nodiscard]] std::span<T> alloc_array_uninit(const std::size_t n) {
            if (n == 0) return {};
            void* mem = allocate(sizeof(T) * n, alignof(T));
            return { static_cast<T*>(mem), n };
        }

        //! @brief Copies `sv` into the arena and returns a string_view over it.
        //! @note The returned `string_view`'s lifetime is the same as the Arena's lifetime.
        [[nodiscard]] std::string_view alloc_str(const std::string_view sv) {
            if (sv.empty()) return {};
            auto* p = static_cast<char*>(allocate(sv.size(), alignof(char)));
            std::ranges::copy(sv, p);
            return { p, sv.size() };
        }

        //! @brief Save the current bump position to a Checkpoint.
        [[nodiscard]] Checkpoint mark() const noexcept {
            if (m_slabs.empty()) return { .slab = 0, .cursor = nullptr };
            return { .slab = m_slabs.size() - 1, .cursor = m_slabs.back().cursor };
        }

        //! @brief Rewind to a checkpoint, discarding all allocations made after a `mark`
        void rewind(const Checkpoint& c) noexcept {
            if (m_slabs.empty()) return;
            for (auto i { c.slab + 1}; i < m_slabs.size(); ++i)
                m_slabs[i].cursor = m_slabs[i].data.get();
            if (c.cursor != nullptr) m_slabs[c.slab].cursor = c.cursor;
        }

        //! @brief Reset all slabs without releasing memory.
        void reset() noexcept {
            for (auto& s : m_slabs) s.cursor = s.data.get();
        }

        //! @brief Release all slabs.
        void release() noexcept { m_slabs.clear(); }

        [[nodiscard]] std::size_t slab_count() const noexcept { return m_slabs.size(); }

        [[nodiscard]] std::size_t bytes_allocated()  const noexcept {
            std::size_t total = 0;
            for (const auto& s : m_slabs)
                total += static_cast<std::size_t>(s.end - s.data.get());
            return total;
        }

        [[nodiscard]] std::size_t bytes_used() const noexcept {
            std::size_t total = 0;
            for (const auto& s : m_slabs)
                total += static_cast<std::size_t>(s.cursor - s.data.get());
            return total;
        }
    private:
        std::vector<Slab> m_slabs       {};
        std::size_t       m_slab_size   {};
    };
}

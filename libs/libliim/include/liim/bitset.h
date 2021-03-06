#pragma once

#include <liim/pointers.h>
#include <limits.h>
#include <string.h>

namespace LIIM {

template<typename T>
class Bitset {
public:
    Bitset(size_t num_bits) : m_bit_count(num_bits) { m_bits = new T[(num_bits + sizeof(T) * CHAR_BIT - 1) / (sizeof(T) * CHAR_BIT)]; }

    Bitset(Bitset&& other) : m_should_deallocate(other.m_should_deallocate), m_bits(other.m_bits), m_bit_count(other.m_bit_count) {
        other.m_should_deallocate = false;
        other.m_bits = nullptr;
        other.m_bit_count = 0;
    }

    template<typename U>
    static SharedPtr<Bitset<U>> wrap(U* bits, size_t num_bits) {
        auto bitset = make_shared<Bitset<U>>();
        bitset->m_should_deallocate = false;
        bitset->m_bits = bits;
        bitset->m_bit_count = num_bits;
        return bitset;
    }

    ~Bitset() {
        if (m_should_deallocate) {
            delete[] m_bits;
        }
    }

    bool get(int bit_index) const {
        int long_index = bit_index / (sizeof(T) * CHAR_BIT);
        bit_index %= sizeof(T) * CHAR_BIT;
        return (m_bits[long_index] & (1UL << bit_index)) ? true : false;
    }

    void set(int bit_index) {
        int long_index = bit_index / (sizeof(T) * CHAR_BIT);
        bit_index %= sizeof(T) * CHAR_BIT;
        m_bits[long_index] |= (1UL << bit_index);
    }

    void unset(int bit_index) {
        int long_index = bit_index / (sizeof(T) * CHAR_BIT);
        bit_index %= sizeof(T) * CHAR_BIT;
        m_bits[long_index] &= ~(1UL << bit_index);
    }

    void flip(int bit_index) {
        int long_index = bit_index / (sizeof(T) * CHAR_BIT);
        bit_index %= sizeof(T) * CHAR_BIT;
        m_bits[long_index] ^= (1UL << bit_index);
    }

    T* bitset() { return m_bits; }
    const T* bitset() const { return m_bits; }

    size_t bit_count() const { return m_bit_count; }

    template<typename Callback>
    void for_each_storage_part(Callback&& callback) {
        for (size_t i = 0; i < ((m_bit_count / CHAR_BIT) + sizeof(T) - 1) / sizeof(T); i++) {
            callback(m_bits[i]);
        }
    }

    template<typename Callback>
    void for_each_storage_part(Callback&& callback) const {
        for (size_t i = 0; i < ((m_bit_count / CHAR_BIT) + sizeof(T) - 1) / sizeof(T); i++) {
            callback(m_bits[i]);
        }
    }

    Bitset() {}

private:
    bool m_should_deallocate { true };
    T* m_bits { nullptr };
    size_t m_bit_count { 0 };
};

}

using LIIM::Bitset;

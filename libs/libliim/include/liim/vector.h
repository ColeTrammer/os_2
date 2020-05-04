#pragma once

#include <assert.h>
#include <liim/traits.h>
#include <liim/utilities.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace LIIM {

template<typename VectorType, typename T>
class VectorIterator;

template<typename T>
class Vector {
public:
    explicit Vector(int capacity = 20) : m_capacity(capacity) {}

    Vector(const Vector& to_copy) : m_capacity(to_copy.capacity()), m_size(to_copy.size()) {
        allocate_vector();
        if (m_size == 0) {
            return;
        }
        if constexpr (Traits<T>::is_simple()) {
            memmove(m_vector, to_copy.m_vector, sizeof(T) * m_size);
        } else {
            for (int i = 0; i < m_size; i++) {
                new (&m_vector[i]) T(to_copy.m_vector[i]);
            }
        }
    }

    Vector(Vector&& other) : m_capacity(other.capacity()), m_size(other.size()), m_vector(other.vector()) {
        other.m_vector = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    Vector(const T* buffer, int num_elements) : m_capacity(num_elements), m_size(num_elements) {
        assert(buffer);
        allocate_vector();
        if constexpr (Traits<T>::is_simple()) {
            memcpy(m_vector, buffer, sizeof(T) * num_elements);
        } else {
            for (int i = 0; i < m_size; i++) {
                new (&m_vector[i]) T(buffer[i]);
            }
        }
    }

    Vector<T>& operator=(const Vector<T>& other) {
        if (this != &other) {
            Vector<T> temp(other);
            swap(temp);
        }

        return *this;
    }

    Vector<T>& operator=(Vector<T>&& other) {
        if (this != &other) {
            Vector<T> temp(LIIM::move(other));
            swap(other);
        }

        return *this;
    }

    // Example of this function:
    // this = { 1, 2, 3, 4, 5 }
    // start_index=2, end_index=5
    // result = { 1, 2, 4, 5, 3 }
    void rotate_left(int start_index, int end_index) {
        assert(start_index <= end_index);
        assert(start_index >= 0);
        assert(end_index <= size());
        if (end_index - start_index <= 1) {
            return;
        }

        T temp(LIIM::move(get(start_index)));
        for (int i = start_index; i < end_index - 1; i++) {
            get(i) = LIIM::move(get(i + 1));
        }
        get(end_index - 1) = LIIM::move(temp);
    }

    ~Vector() {
        for (int i = 0; i < m_size; i++) {
            get(i).~T();
        }
        m_size = 0;
        if (m_vector) {
            free(m_vector);
            m_vector = nullptr;
        }
    }

    void clear() {
        for (int i = 0; i < m_size; i++) {
            get(i).~T();
        }
        m_size = 0;
    }

    bool empty() const { return size() == 0; }

    int size() const { return m_size; }
    int capacity() const { return m_capacity; }

    void add(const T& t) {
        if (!m_vector || m_size >= m_capacity) {
            increase_capacity();
            allocate_vector();
        }

        new (&m_vector[m_size++]) T(t);
    }

    void add(T&& t) {
        if (!m_vector || m_size >= m_capacity) {
            increase_capacity();
            allocate_vector();
        }

        new (&m_vector[m_size++]) T(LIIM::move(t));
    }

    void remove_last() {
        assert(m_size > 0);
        get(m_size - 1).~T();
        m_size--;
    }

    T& get(int i) {
        assert(i >= 0 && i < m_size);
        return m_vector[i];
    }

    const T& get(int i) const {
        assert(i >= 0 && i < m_size);
        return m_vector[i];
    }

    T& get_or(int i, T& val) {
        if (i < 0 || i >= m_size) {
            return val;
        }

        return get(i);
    }

    const T& get_or(int i, const T& val) const {
        if (i < 0 || i >= m_size) {
            return val;
        }

        return get(i);
    }

    T& operator[](int i) { return get(i); }
    const T& operator[](int i) const { return get(i); }

    T& first() { return get(0); }
    const T& first() const { return get(0); }

    T& last() { return get(m_size - 1); }
    const T& last() const { return get(m_size - 1); }

    T* vector() { return m_vector; }
    const T* vector() const { return m_vector; }

    template<typename C>
    void for_each(C callback) {
        for (int i = 0; i < size(); i++) {
            callback(get(i));
        }
    }

    template<typename C>
    void for_each(C callback) const {
        for (int i = 0; i < size(); i++) {
            callback(get(i));
        }
    }

    template<typename C>
    void for_each_reverse(C callback) const {
        for (int i = size() - 1; i >= 0; i--) {
            callback(get(i));
        }
    }

    template<typename C>
    bool any_match(C callback) const {
        for (int i = 0; i < size(); i++) {
            if (callback(get(i))) {
                return true;
            }
        }

        return false;
    }

    template<typename C>
    const T* first_match(C callback) const {
        for (int i = 0; i < size(); i++) {
            if (callback(get(i))) {
                return &get(i);
            }
        }

        return nullptr;
    }

    template<typename C>
    T* first_match(C callback) {
        for (int i = 0; i < size(); i++) {
            if (callback(get(i))) {
                return &get(i);
            }
        }

        return nullptr;
    }

    int index_of(const T& val) const {
        for (int i = 0; i < size(); i++) {
            if (get(i) == val) {
                return i;
            }
        }

        return -1;
    }

    void remove(int index) {
        get(index).~T();
        if (index != size() - 1) {
            if constexpr (Traits<T>::is_simple()) {
                memcpy(m_vector + index, m_vector + index + 1, sizeof(T) * (size() - index - 1));
            } else {
                for (int j = index; j < size() - 1; j++) {
                    new (m_vector + j) T(LIIM::move(get(j + 1)));
                    get(j + 1).~T();
                }
            }
        }

        m_size--;
    }

    void remove_element(const T& val) {
        for (int i = size() - 1; i >= 0; i--) {
            if (get(i) == val) {
                remove(i);
            }
        }
    }

    void insert(const T& val, int position) {
        assert(position >= 0 && position <= size());
        if (position == size()) {
            add(val);
            return;
        }

        if (!m_vector || m_size >= m_capacity) {
            increase_capacity();
            allocate_vector();
        }
        m_size++;

        if constexpr (Traits<T>::is_simple()) {
            memmove(m_vector + position + 1, m_vector + position, sizeof(T) * (size() - position - 1));
        } else {
            for (int j = size() - 1; j > position; j--) {
                new (m_vector + j) T(LIIM::move(get(j - 1)));
                get(j - 1).~T();
            }
        }

        new (m_vector + position) T(val);
    }

    void insert(T&& val, int position) {
        assert(position >= 0 && position <= size());
        if (position == size()) {
            add(val);
            return;
        }

        if (!m_vector || m_size >= m_capacity) {
            increase_capacity();
            allocate_vector();
        }
        m_size++;

        if constexpr (Traits<T>::is_simple()) {
            memmove(m_vector + position + 1, m_vector + position, sizeof(T) * (size() - position - 1));
        } else {
            for (int j = size() - 1; j > position; j--) {
                new (m_vector + j) T(LIIM::move(get(j - 1)));
                get(j - 1).~T();
            }
        }

        new (m_vector + position) T(LIIM::move(val));
    }

    template<typename C>
    bool remove_if(C callback) {
        bool removed = false;
        for_each_reverse([&](auto& elem) {
            if (callback(elem)) {
                remove_element(elem);
                removed = true;
            }
        });

        return removed;
    }

    bool includes(const T& value) const {
        for (int i = 0; i < size(); i++) {
            if (value == get(i)) {
                return true;
            }
        }

        return false;
    }

    bool operator==(const Vector<T>& other) const {
        if (this->size() != other.size()) {
            return false;
        }

        for (int i = 0; i < size(); i++) {
            if (this->get(i) != other.get(i)) {
                return false;
            }
        }

        return true;
    }

    using Iterator = VectorIterator<Vector, T>;
    Iterator begin() { return Iterator(*this, 0); }
    Iterator end() { return Iterator(*this, size()); }

    using ConstIterator = VectorIterator<const Vector, const T>;
    ConstIterator begin() const { return ConstIterator(*this, 0); }
    ConstIterator end() const { return ConstIterator(*this, size()); }

    void swap(Vector<T>& other) {
        LIIM::swap(this->m_capacity, other.m_capacity);
        LIIM::swap(this->m_size, other.m_size);
        LIIM::swap(this->m_vector, other.m_vector);
    }

    void resize(int new_size) {
        if (new_size == size()) {
            return;
        }

        while (new_size < size()) {
            remove_last();
        }

        while (new_size > size()) {
            add(T());
        }
    }

private:
    void increase_capacity() {
        if (m_size > 0) {
            m_capacity *= 2;
        }
    }

    void allocate_vector() {
        T* replacement = static_cast<T*>(malloc(m_capacity * sizeof(T)));
        if (!m_vector) {
            m_vector = replacement;
            return;
        }

        if constexpr (Traits<T>::is_simple()) {
            memmove(replacement, m_vector, sizeof(T) * m_size);
        } else {
            for (int i = 0; i < m_size; i++) {
                new (&replacement[i]) T(LIIM::move(m_vector[i]));
                get(i).~T();
            }
        }

        free(m_vector);
        m_vector = replacement;
    }

    int m_capacity { 0 };
    int m_size { 0 };
    T* m_vector { nullptr };
};

template<typename T>
void swap(Vector<T>& a, Vector<T>& b) {
    a.swap(b);
}

template<typename VectorType, typename T>
class VectorIterator {
public:
    VectorIterator(VectorType& vector, int offset) : m_vector(vector), m_offset(offset) {}

    bool operator!=(const VectorIterator& other) const { return !(*this == other); }
    bool operator==(const VectorIterator& other) const { return m_offset == other.m_offset; }
    bool operator<(const VectorIterator& other) const { return m_offset < other.m_offset; }
    bool operator>(const VectorIterator& other) const { return m_offset > other.m_offset; }
    bool operator<=(const VectorIterator& other) const { return !(*this > other); }
    bool operator>=(const VectorIterator& other) const { return !(*this < other); }

    VectorIterator& operator++() {
        m_offset++;
        return *this;
    }

    VectorIterator operator++(int) {
        VectorIterator ret(*this);
        m_offset++;
        return ret;
    }

    VectorIterator operator--() {
        m_offset--;
        return *this;
    }

    VectorIterator operator--(int) {
        VectorIterator ret(*this);
        m_offset--;
        return ret;
    }

    VectorIterator operator+(int offset) { return { m_vector, m_offset + offset }; }
    VectorIterator operator-(int offset) { return { m_vector, m_offset - offset }; }

    VectorIterator& operator=(const VectorIterator& other) {
        m_offset = other.m_offset;
        return *this;
    }

    T& operator*() { return m_vector[m_offset]; }
    T* operator->() { return &m_vector[m_offset]; }

private:
    VectorType& m_vector;
    int m_offset;
};

}

using LIIM::Vector;
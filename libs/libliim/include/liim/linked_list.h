#pragma once

#include <assert.h>
#include <liim/utilities.h>

namespace LIIM {

template<typename T>
struct LinkedListObj {
    LinkedListObj(const T& val) : m_val(val) {}
    LinkedListObj(T&& val) : m_val(move(val)) {}
    ~LinkedListObj() {}

    T m_val;
    LinkedListObj<T>* m_next { nullptr };
};

template<typename T>
class LinkedList {
public:
    LinkedList() {}

    explicit LinkedList(const T& first) : m_head(new LinkedListObj(first)), m_size(1) {}

    LinkedList(const LinkedList<T>& other) {
        if (!other.size()) {
            return;
        }

        other.for_each([&](const T& val) {
            add(val);
        });
    }

    LinkedList(LinkedList<T>&& other) : m_size(other.m_size), m_head(other.m_head) {
        other.m_head = nullptr;
        other.m_size = 0;
    }

    ~LinkedList() { clear(); }

    LinkedList<T>& operator=(const LinkedList<T>& other) {
        if (this != &other) {
            LinkedList<T> temp(other);
            swap(temp);
        }

        return *this;
    }

    LinkedList<T>& operator=(LinkedList<T>&& other) {
        if (this != other) {
            LinkedList<T> temp(other);
            swap(temp);
        }

        return *this;
    }

    int size() const { return m_size; }
    bool empty() const { return size() == 0; }

    void prepend(const T& to_add) {
        auto* next = new LinkedListObj<T>(to_add);
        next->m_next = m_head;
        m_head = next;
        m_size++;
    }

    void prepend(T&& to_add) {
        auto* next = new LinkedListObj<T>(move(to_add));
        next->m_next = m_head;
        m_head = next;
        m_size++;
    }

    void add(const T& to_add) {
        auto* next = new LinkedListObj<T>(to_add);
        LinkedListObj<T>** iter = &m_head;
        while (*iter) {
            iter = &(*iter)->m_next;
        }
        *iter = next;

        m_size++;
    }

    void remove(const T& to_remove) {
        LinkedListObj<T>** iter = &m_head;
        while (*iter && (*iter)->m_val != to_remove) {
            iter = &(*iter)->m_next;
        }

        if (*iter) {
            auto* save = *iter;
            *iter = save->m_next;
            delete save;
        }

        m_size--;
    }

    void clear() {
        auto* iter = m_head;
        while (iter) {
            auto* save = iter->m_next;
            delete iter;
            iter = save;
        }

        m_size = 0;
        m_head = nullptr;
    }

    bool is_empty() { return m_size == 0; }

    template<typename C>
    void for_each(C callback) {
        auto* iter = m_head;
        while (iter) {
            callback(iter->m_val);
            iter = iter->m_next;
        }
    }

    template<typename C>
    void for_each(C callback) const {
        auto* iter = m_head;
        while (iter) {
            callback(iter->m_val);
            iter = iter->m_next;
        }
    }

    template<typename C>
    bool remove_if(C test) {
        auto** iter = &m_head;
        bool removed = false;
        while (*iter) {
            if (test((*iter)->m_val)) {
                if (*iter) {
                    auto* save = *iter;
                    *iter = save->m_next;
                    delete save;
                }

                m_size--;
                removed = true;
                continue;
            }
            iter = &(*iter)->m_next;
        }
        return removed;
    }

    T& head() {
        assert(m_head);
        return m_head->m_val;
    }
    const T& head() const {
        assert(m_head);
        return m_head->m_val;
    }

    T& tail() {
        LinkedListObj<T>* obj = m_head;
        while (obj->m_next) {
            obj = obj->m_next;
        }

        assert(obj);
        return obj->m_val;
    }

    bool includes(const T& val) const {
        LinkedListObj<T>* iter = m_head;
        while (iter) {
            if (val == iter->m_val) {
                return true;
            }
            iter = iter->m_next;
        }
        return false;
    }

    void swap(LinkedList<T>& other) {
        LIIM::swap(m_size, other.m_size);
        LIIM::swap(m_head, other.m_head);
    }

private:
    int m_size { 0 };
    LinkedListObj<T>* m_head { nullptr };
};

template<typename T>
void swap(LinkedList<T>& a, LinkedList<T>& b) {
    a.swap(b);
}

}

using LIIM::LinkedList;

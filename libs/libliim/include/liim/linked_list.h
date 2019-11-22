#pragma once

#include <assert.h>
#include <new>

namespace LIIM {

template<typename T>
struct LinkedListObj {
    LinkedListObj(const T& val)
        : m_val(val)
    {
    }

    T m_val;
    LinkedListObj<T>* m_next { nullptr };
};

template<typename T>
class LinkedList {
public:
    LinkedList()
    {
    }

    explicit LinkedList(const T& first)
        : m_head(new LinkedListObj(first))
        , m_size(1)
    {
    }

    ~LinkedList()
    {
        clear();
    }

    int size() const { return m_size; }

    void prepend(const T& to_add)
    {
        auto* next = new LinkedListObj<T>(to_add);
        next->m_next = m_head;
        m_head = next;
        m_size++;
    }

    void add(const T& to_add)
    {
        auto* next = new LinkedListObj<T>(to_add);
        LinkedListObj<T>** iter = &m_head;
        while (*iter) {
            iter = &(*iter)->m_next;
        }
        *iter = next;

        m_size++;
    }

    void remove(const T& to_remove)
    {
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

    void clear()
    {
        auto* iter = m_head;
        while (iter) {
            auto* save = iter->m_next;
            delete iter;
            iter = save;
        }

        m_size = 0;
        m_head = nullptr;
    }

    bool is_empty()
    {
        return m_size == 0;
    }

    template<typename C>
    void for_each(C callback)
    {
        auto* iter = m_head;
        while (iter) {
            callback(iter->m_val);
            iter = iter->m_next;
        }
    }

    template<typename C>
    void remove_if(C test) {
        auto** iter = &m_head;
        while (*iter) {
            if (test((*iter)->m_val)) {
                if (*iter) {
                    auto* save = *iter;
                    *iter = save->m_next;
                    delete save;
                }

                m_size--;
                continue; 
            }
            iter = &(*iter)->m_next;
        }
    }

private:
    int m_size { 0 };
    LinkedListObj<T>* m_head { nullptr };
};

}

using LIIM::LinkedList;
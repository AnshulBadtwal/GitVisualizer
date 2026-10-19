#ifndef CUSTOM_CONTAINERS_H
#define CUSTOM_CONTAINERS_H

#include <stdexcept>
#include <utility>

namespace Git {

template <typename T>
class CustomStack {
private:
    struct Node {
        T data;
        Node* next;
    };

    Node* m_top{nullptr};
    size_t m_size{0};

public:
    CustomStack() = default;
    ~CustomStack() { clear(); }

    // Prevent copying to manage raw node memory safely
    CustomStack(const CustomStack&) = delete;
    CustomStack& operator=(const CustomStack&) = delete;

    // Allow move semantics
    CustomStack(CustomStack&& other) noexcept 
        : m_top(other.m_top), m_size(other.m_size) {
        other.m_top = nullptr;
        other.m_size = 0;
    }

    CustomStack& operator=(CustomStack&& other) noexcept {
        if (this != &other) {
            clear();
            m_top = other.m_top;
            m_size = other.m_size;
            other.m_top = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    void push(const T& item) {
        m_top = new Node{item, m_top};
        ++m_size;
    }

    void push(T&& item) {
        m_top = new Node{std::move(item), m_top};
        ++m_size;
    }
    
    void pop() {
        if (!m_top) {
            throw std::out_of_range("Stack underflow");
        }
        Node* temp = m_top;
        m_top = m_top->next;
        delete temp;
        --m_size;
    }

    [[nodiscard]] T& top() {
        if (!m_top) {
            throw std::out_of_range("Stack is empty");
        }
        return m_top->data;
    }

    [[nodiscard]] const T& top() const {
        if (!m_top) {
            throw std::out_of_range("Stack is empty");
        }
        return m_top->data;
    }

    [[nodiscard]] bool isEmpty() const { return m_top == nullptr; }
    [[nodiscard]] size_t size() const { return m_size; }
    
    void clear() {
        while (m_top) {
            Node* temp = m_top;
            m_top = m_top->next;
            delete temp;
        }
        m_size = 0;
    }
};

template <typename T>
class CustomQueue {
private:
    struct Node {
        T data;
        Node* next;
    };

    Node* m_front{nullptr};
    Node* m_tail{nullptr};
    size_t m_size{0};

public:
    CustomQueue() = default;
    ~CustomQueue() { clear(); }

    // Prevent copying to manage raw node memory safely
    CustomQueue(const CustomQueue&) = delete;
    CustomQueue& operator=(const CustomQueue&) = delete;

    // Allow move semantics
    CustomQueue(CustomQueue&& other) noexcept 
        : m_front(other.m_front), m_tail(other.m_tail), m_size(other.m_size) {
        other.m_front = nullptr;
        other.m_tail = nullptr;
        other.m_size = 0;
    }

    CustomQueue& operator=(CustomQueue&& other) noexcept {
        if (this != &other) {
            clear();
            m_front = other.m_front;
            m_tail = other.m_tail;
            m_size = other.m_size;
            other.m_front = nullptr;
            other.m_tail = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    void enqueue(const T& item) {
        Node* newNode = new Node{item, nullptr};
        if (!m_tail) {
            m_front = m_tail = newNode;
        } else {
            m_tail->next = newNode;
            m_tail = newNode;
        }
        ++m_size;
    }

    void enqueue(T&& item) {
        Node* newNode = new Node{std::move(item), nullptr};
        if (!m_tail) {
            m_front = m_tail = newNode;
        } else {
            m_tail->next = newNode;
            m_tail = newNode;
        }
        ++m_size;
    }

    void dequeue() {
        if (!m_front) {
            throw std::out_of_range("Queue underflow");
        }
        Node* temp = m_front;
        m_front = m_front->next;
        if (!m_front) {
            m_tail = nullptr; // Queue is now empty
        }
        delete temp;
        --m_size;
    }

    [[nodiscard]] T& front() {
        if (!m_front) {
            throw std::out_of_range("Queue is empty");
        }
        return m_front->data;
    }

    [[nodiscard]] const T& front() const {
        if (!m_front) {
            throw std::out_of_range("Queue is empty");
        }
        return m_front->data;
    }

    [[nodiscard]] bool isEmpty() const { return m_front == nullptr; }
    [[nodiscard]] size_t size() const { return m_size; }
    
    void clear() {
        while (m_front) {
            Node* temp = m_front;
            m_front = m_front->next;
            delete temp;
        }
        m_tail = nullptr;
        m_size = 0;
    }
};

} // namespace Git

#endif // CUSTOM_CONTAINERS_H

#ifndef CUSTOM_CONTAINERS_H
#define CUSTOM_CONTAINERS_H
#include <vector>
#include <stdexcept>

namespace Git {

template <typename T>
class CustomStack {
public:
    void push(const T& item) { m_data.push_back(item); }
    void push(T&& item) { m_data.push_back(std::move(item)); }
    
    void pop() {
        if (m_data.empty()) {
            throw std::out_of_range("Stack underflow");
        }
        m_data.pop_back();
    }

    [[nodiscard]] T& top() {
        if (m_data.empty()) {
            throw std::out_of_range("Stack is empty");
        }
        return m_data.back();
    }

    [[nodiscard]] const T& top() const {
        if (m_data.empty()) {
            throw std::out_of_range("Stack is empty");
        }
        return m_data.back();
    }

    [[nodiscard]] bool isEmpty() const { return m_data.empty(); }
    [[nodiscard]] size_t size() const { return m_data.size(); }
    void clear() { m_data.clear(); }

private:
    std::vector<T> m_data;
};

template <typename T>
class CustomQueue {
public:
    void enqueue(const T& item) { m_data.push_back(item); }
    void enqueue(T&& item) { m_data.push_back(std::move(item)); }

    void dequeue() {
        if (m_data.empty()) {
            throw std::out_of_range("Queue underflow");
        }
        m_data.erase(m_data.begin());
    }

    [[nodiscard]] T& front() {
        if (m_data.empty()) {
            throw std::out_of_range("Queue is empty");
        }
        return m_data.front();
    }

    [[nodiscard]] const T& front() const {
        if (m_data.empty()) {
            throw std::out_of_range("Queue is empty");
        }
        return m_data.front();
    }

    [[nodiscard]] bool isEmpty() const { return m_data.empty(); }
    [[nodiscard]] size_t size() const { return m_data.size(); }
    void clear() { m_data.clear(); }

private:
    std::vector<T> m_data;
};

} // namespace Git

#endif // CUSTOM_CONTAINERS_H

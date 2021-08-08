/*
 * MIT License
 *
 * Copyright(c) 2021 Mikhail Gorobets
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this softwareand associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright noticeand this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#pragma once

#include <Hawk/Common/Defines.hpp>
#include <queue>
#include <mutex>

namespace Hawk {
    namespace Containers {
        template<typename T>
        class ThreadSafeQueue final {
        public:
            ThreadSafeQueue() = default;

            auto Push(T&& value) -> void {
                std::unique_lock<std::mutex> lock(m_Mutex);
                m_Queue.push(value);
                lock.unlock();
                m_Condition.notify_one();
            }

            auto Push(const T& value) -> void {
                std::unique_lock<std::mutex> lock(m_Mutex);
                assert(m_IsValid);
                m_Queue.push(value);
                lock.unlock();
                m_Condition.notify_one();
            }

            auto Pop() -> std::optional<T> {
                std::unique_lock<std::mutex> lock(m_Mutex);
                m_Condition.wait(lock, [this]() { return !m_Queue.empty() || !m_IsValid; });
                if (!m_IsValid)
                    return std::nullopt;

                auto value = std::move(m_Queue.front());
                m_Queue.pop();
                return value;
            }

            auto IsEmpty() const -> bool {
                std::unique_lock<std::mutex> lock(m_Mutex);
                return m_Queue.empty();
            }

            auto Invalidate() -> void {
                m_IsValid = false;
                m_Condition.notify_all();
            }

            auto Clear() -> void {
                std::lock_guard<std::mutex> lock(m_Mutex);
                while (!m_Queue.empty())
                    m_Queue.pop();
                m_Condition.notify_all();
            }

        private:
            std::atomic_bool        m_IsValid = {true};
            std::queue<T>           m_Queue = {};
            mutable std::mutex      m_Mutex = {};
            std::condition_variable m_Condition = {};
        };
    }
}

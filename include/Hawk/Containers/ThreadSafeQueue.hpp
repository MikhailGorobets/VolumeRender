#pragma once

#include <Hawk/Common/Defines.hpp>

#include <queue>
#include <mutex>


namespace Hawk {
	namespace Math {

		template<typename T>
		class ThreadSafeQueue final {
		public:
			ThreadSafeQueue();
			ThreadSafeQueue(ThreadSafeQueue const& rhs);

			auto Push(T const& v) noexcept -> void;
			auto Pop()            noexcept -> T;
			auto IsEmpty()  const noexcept -> bool;
			auto Size()     const noexcept -> U32;

		private:
			std::queue<T> m_Queue;
			mutable std::mutex m_Mutex;
			std::condition_variable m_Signal;
		};
		

	}
}

namespace Hawk {
	namespace Math {
		template<typename T>
		ILINE ThreadSafeQueue<T>::ThreadSafeQueue() {}

		template<typename T>
		ILINE ThreadSafeQueue<T>::ThreadSafeQueue(ThreadSafeQueue const & rhs) {
			std::lock_guard<std::mutex> lock(rhs.m_Mutex);
			m_Queue = rhs.m_Queue;
		}

	
		template<typename T>
		ILINE auto ThreadSafeQueue<T>::Push(T const & v) noexcept -> void {
			std::lock_guard<std::mutex> lock(rhs.m_Mutex);
			m_Queue.push(v);
			m_Signal.notify_one();
		}

		template<typename T>
		[[nodiscard]] ILINE auto ThreadSafeQueue<T>::Pop() noexcept -> T {
			std::unique_lock<std::mutex> lock(m_Mutex);
			while (m_Queue.empty()) 
				m_Signal.wait(lock);			
			auto v = m_Queue.front();
			m_Queue.pop();
			return v;
		}

		template<typename T>
		[[nodiscard]] ILINE auto ThreadSafeQueue<T>::IsEmpty() const noexcept -> bool {
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_Queue.empty();
		}

		template<typename T>
		[[nodiscard]] ILINE auto ThreadSafeQueue<T>::Size() const noexcept -> U32 {
			std::lock_guard<std::mutex> lock(m_Mutex);
			return static_cast<U32>(m_Queue.size());
		}


	}
}
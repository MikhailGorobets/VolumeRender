#pragma once

namespace Hawk {

	template<typename T>
	class Singleton {
	public:
		static T& Instance() {
			static const std::unique_ptr<T> instance{ new T{ token{} } };
			return *instance;
		};
	protected:
		Singleton(const Singleton&) = delete;
		Singleton& operator=(const Singleton) = delete;
		Singleton() { }
		struct token { };
	};
}
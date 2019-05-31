#pragma once

namespace Hawk {

	class NonCopyable {
	protected:
		NonCopyable() = default;
		~NonCopyable() = default;
	private:
		NonCopyable(NonCopyable const&) = delete;
		NonCopyable& operator=(NonCopyable const &) = delete;
	};

}
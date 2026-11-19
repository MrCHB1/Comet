#pragma once
#include <vector>
#include <cstdint>

template <class T>
struct DefaultInitAlloc : std::allocator<T>
{
	template <class U> struct rebind { using other = DefaultInitAlloc<U>; };
	using std::allocator<T>::allocator;

	template <class U>
	void construct(U* p) noexcept(std::is_nothrow_constructible_v<U>)
	{
		::new ((void*)p) U;
	}

	template <class U, class... A>
	void construct(U* p, A&&... a)
	{
		::new ((void*)p) U(std::forward<A>(a)...);
	}
};

using U32Vec = std::vector<uint32_t, DefaultInitAlloc<uint32_t>>;
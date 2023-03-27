#pragma once

#include <boost/preprocessor.hpp>

// Static reflection based on: https://stackoverflow.com/a/11748131 .

/// Declare public data members with support for reflection.
///     REFLECT((TYPE) (NAME) (INITIALIZER), (TYPE) (NAME) (INITIALIZER), ...)
/// Brackets around each TYPE, NAME, and INITIALIZER are needed.
/// Can be used only once per class/struct.
#define REFLECT(...) \
	friend class reflector; \
private: \
	static constexpr int _REFLECTION_FIELD_COUNT = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__); \
	template<int N, class Self> \
	struct _reflection_data {}; \
	BOOST_PP_SEQ_FOR_EACH_I(_REFLECT_EACH, data, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define _REFLECT_EACH(r, data, i, x) \
public: \
	BOOST_PP_SEQ_ELEM(0, x) BOOST_PP_SEQ_ELEM(1, x) = BOOST_PP_SEQ_ELEM(2, x); \
private: \
	template<class Self> \
	struct _reflection_data<i, Self> \
	{ \
		static constexpr auto name = BOOST_PP_STRINGIZE(BOOST_PP_SEQ_ELEM(1, x)); \
		BOOST_PP_SEQ_ELEM(0, x) &value; \
		_reflection_data(Self &self) : value{ self.BOOST_PP_SEQ_ELEM(1, x) } {} \
	};

// Implements static reflection methods.
class reflector
{
public:
	// Invoke given callback for every field of the object.
	static void for_each(auto &object, auto callback)
	{
		for_each<0>(object, callback);
	}

	// Get Nth field of the object.
	template<int N, class T>
	static typename T::template _reflection_data<N, T> get(T &object)
	{
		return typename T::template _reflection_data<N, T>(object);
	}

	// Get the number of fields.
	template<class T>
	static constexpr int count() { return T::_REFLECTION_FIELD_COUNT; }

private:
	template<int I, class T>
	static void for_each(T &object, auto callback)
	{
		if constexpr (I < count<T>())
		{
			callback(get<I>(object));
			for_each<I + 1>(object, callback);
		}
	}
};

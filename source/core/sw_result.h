#pragma once

// We can't use std::expected because we only require C++17.

#include <stdexcept>

class swUnexpected
{
public:
	explicit swUnexpected(const QString& Error)
	    : mError(Error)
	{
	}

	explicit swUnexpected(QString&& Error)
	    : mError(std::move(Error))
	{
	}

	const QString& error() const
	{
		return mError;
	}

	QString& error()
	{
		return mError;
	}

protected:
	QString mError;
};

template <typename T>
class [[nodiscard]] swResult
{
	using internal_t = std::conditional_t<std::is_void_v<T>, char, T>;

public:
	template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U> && std::is_convertible_v<U, T>>>
	swResult(U&& Value)
	    : mStorage(std::in_place_index<0>, std::forward<U>(Value))
	{
	}

	template <typename U = T, typename = std::enable_if_t<std::is_void_v<U>>>
	swResult()
	    : mStorage(std::in_place_index<0>)
	{
	}

	swResult(swUnexpected&& Unexpected)
	    : mStorage(std::in_place_index<1>, std::move(Unexpected))
	{
	}

	swResult(const swResult& Other) = default;
	swResult& operator=(const swResult& Other) = default;

	swResult(swResult&& Other) = default;
	swResult& operator=(swResult&& Other) = default;

	~swResult() = default;

	bool has_value() const
	{
		return mStorage.index() == 0;
	}

	explicit operator bool() const
	{
		return has_value();
	}

	template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U> && std::is_same_v<U, T>>>
	constexpr const U& value() const
	{
		if (!has_value())
			throw std::logic_error("bad swResult access");

		return std::get<0>(mStorage);
	}

	constexpr const QString& error() const
	{
		if (mStorage.index() != 1)
			throw std::logic_error("bad swResult access");

		return std::get<1>(mStorage).error();
	}

protected:
	std::variant<internal_t, swUnexpected> mStorage;
};

// Copyright (c) 2021, Andrey Shvyrkin. All rights reserved.

#pragma once

#include <mutex>
#include <shared_mutex>
#include <type_traits>

template <typename ValueType>
class thread_safe
{
	using remove_ref_value_type = std::remove_reference_t<ValueType>;
	using const_value_ref_type = const remove_ref_value_type&;
	using value_ref_type = remove_ref_value_type&;

public:
	template <template<typename> class LockType, typename MutexType = std::shared_mutex>
	class access
	{
		using value_type = std::conditional_t<std::is_same_v<LockType<MutexType>, std::shared_lock<MutexType>>,
		                                      const remove_ref_value_type, remove_ref_value_type>;

	public:
		template <typename... LockArgs>
		explicit access(thread_safe& safe, LockArgs&&... args) :
			value_(safe.value_),
			lock_(safe.mutex_, std::forward<LockArgs>(args)...)
		{
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		const value_type* operator->() const noexcept
		{
			return &value_;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		value_type* operator->() noexcept
		{
			return &value_;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		const value_type& operator*() const noexcept
		{
			return value_;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		value_type& operator*() noexcept
		{
			return value_;
		}

		/// <summary>
		/// 
		/// </summary>
		void lock() const
		{
			if (!lock_)
				lock_.lock();
		}

		/// <summary>
		/// 
		/// </summary>
		void unlock() const {
            if (lock_)
                lock_.unlock();
        }

	private:
		value_type& value_;
		mutable LockType<MutexType> lock_;
	};

	template <typename... ValueArgs>
	explicit thread_safe(ValueArgs&&... args) noexcept :
		value_(std::forward<ValueArgs>(args)...)
	{
	}

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	access<std::shared_lock> read_lock() { return access<std::shared_lock>(*this); }
	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	access<std::unique_lock> write_lock() { return access<std::unique_lock>(*this); }
	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	const_value_ref_type unsafe() const noexcept { return value_; }
	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	value_ref_type unsafe() noexcept { return value_; }

    std::shared_mutex& unsafe_mutex() const noexcept { return mutex_; }

private:
	ValueType value_;
	mutable std::shared_mutex mutex_;
};

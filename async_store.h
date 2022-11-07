// Copyright (c) 2022, Vladimir Sviridov. All rights reserved.
#pragma once

#include "utils/thread_safe.h"
#include <unordered_map>
#include <map>

#include <chrono>
using namespace std::chrono;

#include <set>
#include <future>
#include <variant>
#include <iostream>

//implicit container

template<typename... Args>
struct variant_helper {
	using variant_t = std::variant<Args...>;
};

template<typename T>
struct variant_helper<T> {
	using variant_t = T;
};

template<typename... Args>
class async_store {
public:
	typedef std::integral_constant<size_t, sizeof...(Args)> args_count;	

	using variant_t = typename variant_helper<Args...>::variant_t;
	using promise_t = std::promise<variant_t>;

    explicit async_store(uint32_t wait_t = 60):
        terminate_(false),
        expiration_control_thread_(&async_store::check_expired, this),
        wait_time{wait_t}
        {}

    ~async_store() {
        try {
            {
                std::unique_lock<std::mutex> lock(store_mutex_);
                terminate_ = true;
            }
            time_points_condition_.notify_all();
            if (expiration_control_thread_.joinable()) {
                expiration_control_thread_.join();
            }
        } catch(...) {}
    }

	void init(uint32_t id) {
        std::unique_lock<std::mutex> lock(store_mutex_);
		store_.emplace(id, std::pair{steady_clock::now(), promise_t{}});
	}

	template<class T>
	void set_value(uint32_t id, T value) {
        std::unique_lock<std::mutex> lock(store_mutex_);
        store_.at(id).second.set_value(std::forward<T>(value));
        update_time_point(id);
		lock.unlock();
        switch_future_set(id, is_get_future, is_set_value);
	}

	void set_value(uint32_t id) {
        std::scoped_lock<std::mutex> lock(store_mutex_);
		store_.at(id).second.set_value();
        update_time_point(id);
        switch_future_set(id, is_get_future, is_set_value);
	}		

	void set_exception(uint32_t id, std::exception_ptr p) {
        std::scoped_lock<std::mutex> lock(store_mutex_);
		store_.at(id).set_exception(p);
        update_time_point(id);
        switch_future_set(id, is_get_future, is_set_value);
	}

	auto get_future(uint32_t id) {
        std::scoped_lock<std::mutex> lock(store_mutex_);
		auto r_ = store_.at(id).second.get_future();
        update_time_point(id);
        switch_future_set(id, is_set_value, is_get_future);
		return r_;		
	}
	auto get_shared_future(uint32_t id) {
        std::scoped_lock<std::mutex> lock(store_mutex_);
		auto r_ = std::shared_future<variant_t>(store_.at(id).get_future());
        update_time_point(id);
        switch_future_set(id, is_set_value, is_get_future);
        return r_;
	}

	void erase(uint32_t id) {
        std::scoped_lock<std::mutex> lock(store_mutex_);
		store_.erase(id);
        if (is_get_future.count(id)) {
            is_get_future.erase(id);
        }
        if (is_set_value.count(id)) {
            is_set_value.erase(id);
        }
	}

	void dispose() {
        std::scoped_lock<std::mutex> lock(store_mutex_);
		for (auto it = store_.begin(); it != store_.end();) {
			auto& [id, f] = *it;

			if (is_get_future.count(id) && !is_set_value.count(id)) {
				f.set_exception(std::make_exception_ptr(std::logic_error("Dispose async store")));
			}
			it = store_.erase(it);
		}
		is_get_future.clear();
		is_set_value.clear();
    }

private:
    void switch_future_set(uint32_t id, std::set<uint32_t>& from, std::set<uint32_t>& to) {
        if (from.count(id)) {
            store_.erase(id);
            from.erase(id);
        } else {
            to.emplace(id);
        }
    }

    void update_time_point(uint32_t id) {
        auto& da = store_.at(id);
        da.first = steady_clock::now();
    }

    void check_expired() {
        while (true) {
            const auto now = steady_clock::now();
            auto awakening_time = now;
            std::unique_lock<std::mutex> lock(store_mutex_);
            for (auto i = store_.begin(); i != store_.end();) {
                if ((now - i->second.first) > wait_time) {
                    i->second.second.set_exception(
                            std::make_exception_ptr(
                                    std::runtime_error("Exception expired")
                                    )
                            );
                    std::cout << "Promise " << i->first << " expired" << std::endl;
                    if (is_get_future.count(i->first)) {
                        is_get_future.erase(i->first);
                    }
                    if (is_set_value.count(i->first)) {
                        is_set_value.erase(i->first);
                    }

                    i = store_.erase(i);
                    continue;
                }
                awakening_time = std::min(awakening_time, i->second.first);
                ++i;
            }

            if (time_points_condition_.wait_until(lock, steady_clock::time_point(awakening_time + wait_time), [this](){ return terminate_; })) {
                return;
            }
        }
    }

private:
	mutable std::unordered_map<std::size_t, std::pair<steady_clock::time_point, promise_t>> store_;
    std::mutex store_mutex_;

    std::condition_variable time_points_condition_;
    bool terminate_;
    const std::chrono::seconds wait_time;
    std::thread expiration_control_thread_;

	std::set<uint32_t> is_get_future;
	std::set<uint32_t> is_set_value;
};

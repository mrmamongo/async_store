#include <iostream>
#include "async_store.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct RandomClass {
    int RandomValue = 10;
    std::string RandomString = "String";
};

void use_value(async_store<std::string, RandomClass>& s, uint32_t id) {
    try {
        auto future = s.get_future(id);
        std::visit(overloaded{
                [](const std::string &s) {
                    std::cout << "This is a string from another thread" << std::endl;
                },
                [](const RandomClass &s) {
                    std::cout << "This is a random value from another thread " << s.RandomValue << ' ' << s.RandomString
                              << std::endl;
                }
        }, future.get());
    } catch (std::out_of_range& e) {}
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}

int main() {
    async_store<std::string, RandomClass> store{2};
    store.init(10);
    store.init(20);
    store.init(12);
    store.init(13);
    store.init(14);
    store.init(23);
    store.init(25);
    store.init(26);

    store.set_value(10, "Value got from second tread");
    store.set_value(20, "Value 20");
    store.set_value(12, RandomClass{12, "Halo"});
    store.set_value(13, RandomClass{ 13, "Helo"});
    store.set_value(14, RandomClass{12, "Hallo"});
    store.set_value(23, "Value 235");
    store.set_value(25, "Value 25");

    std::thread second([&](){
        for (int i = 1; i < 26; i++) {
            use_value(store, i);
        }
    });

    second.join();


    return 0;
}

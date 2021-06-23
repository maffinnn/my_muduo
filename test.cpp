#include <memory>
#include <iostream>
#include <functional>

struct Foo {
    Foo(int num):num_(num){}
    void print_add(int val) { std::cout << num_+val << std::endl; }
    int num_;
};

void print_num(int val) { std::cout << val << std::endl; }

struct PrintNum{
    void operator() (int val) const { std::cout << val << std::endl; }
};


int main()
{
    Foo* f = new Foo(2);
    std::function<void(Foo&, int)> functor = &Foo::print_add;
    functor(*f, 1);
    //std::cout << &(Foo::print_add) << std::endl;
    //printf("%p\n", &Foo::print_add);
    return 0;
}
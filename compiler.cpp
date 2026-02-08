#include <cassert>
#include <cstdio>
#include <iostream>
#include <sstream>

#include "project.h"

// This is just a file for testing out examples.

int main(const int argc, const char** argv) {
    std::cout << "Hello world\n";

    Index i{"i"};
    Array A{"A"};
    Array B{"B"};
    Array C{"C"};
    Array D{"D"};

    Expr e = A(i);
    FormatMap formats = {
        {"A", {Format::Compressed}},
        {"B", {Format::Compressed}},
        {"C", {Format::Compressed}},
        {"D", {Format::Compressed}}
    };

    std::cout << e << "\n";

    {
        auto a = (C(i) = e);
        std::cout << a << "\n";
        auto stmt = lower(a);
        std::cout << stmt << "\n";
        std::cout << lower(stmt, formats) << "\n";
    }

    e = e * B(i);

    {
        auto a = (C(i) = e);
        std::cout << a << "\n";
        auto stmt = lower(a);
        std::cout << stmt << "\n";
        std::cout << lower(stmt, formats) << "\n";
    }

    return 0;
}


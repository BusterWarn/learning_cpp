# Simple Modules

My first attempt att [C++20 Modules](https://en.cppreference.com/w/cpp/language/modules). I got inspired after watching [Practical C++ Modules - Boris Kolpackov - CppCon 2019](https://youtu.be/szHV6RdQdg8). However on Windows with Mingw64 this hardly works (for me) even with clang version 15.0.7. I think it have trouble finding the cpp modules library, or connecting the cpplib with modules somehow. It works with some libraries like `<cstdint>` but not others like `<string>`.
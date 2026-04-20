/*
MIT License

Copyright (c) 2022-2025 jffordem

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef DIAGNOSTICS_HPP
#define DIAGNOSTICS_HPP

/*
freeMemory() returns the number of bytes available between the stack and
the heap. Call it from setup() or periodically to detect memory pressure
from linked-list allocations or deep call chains.

  Serial.print("Free RAM: ");
  Serial.println(freeMemory());

Returns -1 on platforms where the measurement is not supported.
*/

#if defined(__AVR__)
extern int __heap_start;
extern int *__brkval;
inline int freeMemory() {
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
#elif defined(__arm__)
#include <malloc.h>
inline int freeMemory() {
    struct mallinfo mi = mallinfo();
    return (int)mi.fordblks;
}
#else
inline int freeMemory() { return -1; }
#endif

#endif // DIAGNOSTICS_HPP

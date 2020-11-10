# deflate_decompressor

This is a pure C++ implementation of the decompression side of the DEFLATE compression method. In other words, it's an "inflate-only" implementation of the deflate algorithm.

It can handle raw deflate streams, and streams wrapped in zlib or gzip formatted headers. The stream type is detected automatically.

As you may have guessed, this is yet another subproject of my Optical Character Reader (OCR) project.

## Why a new implementation?

I usually use the tried and true [zlib]( http://zlib.net/) for all my compression and decompression needs. It's reliable, fast, and has an API that makes it easy to use for all possible scenarios. The only drawback is that under Windows, one has to see some extra trouble to make the library available. The zlib headers or the library itself aren't part of the default package of libraries available under the Windows environment.

My implementation is a single C++ class completely contained in the header file `deflate_decompressor.h` and the implementation file `deflate_decompressor.cpp`.

It doesn't rely on any external non-standard libraries, so it can easily be included in any C++ project.

## My use-case: Decompression from memory to memory

The class is specifically designed for my particular use-case: Decompressing a reasonably small deflate stream, already present in memory, into an ordinary `std::vector<char>` buffer.

It naturally follows that this decompressor isn't the best choice for streams that can't comfortably reside completely in memory.

## More info

Wikipedia has a good introduction to the DEFLATE algorithm and to the data format here: https://en.wikipedia.org/wiki/DEFLATE

The official specifications for the deflate algorithm and for the two common file formats `zlib` and `gzip` used with the deflate compression method are given in RFC-1950, RFC-1951, and RFC-1952.

[RFC-1950 ZLIB Compressed Data Format Specification version 3.3](https://tools.ietf.org/html/rfc1950)

[RFC-1951 DEFLATE Compressed Data Format Specification version 1.3](https://tools.ietf.org/html/rfc1951)

[RFC-1952 GZIP file format specification version 4.3](https://tools.ietf.org/html/rfc1952)

## The reference implementation

The well known [zlib](http://zlib.net/) by Jean-loup Gailly and Mark Adler is regarded as the one true implementation and embodyment of the above mentioned standards.

In all the cases where the standards are hard to follow, open to several interpretations, or otherwise unclear, I have shamelessly checked how zlib handles the situation and tried to follow the example.

I have also included my usual `zlib_interface` in the source package to have it easily available for cases where the original zlib is still a better choice than my own class. It bridges the gap between the C API of zlib, and C++ of my client code.

In `testmain.cpp` I use the original [zlib](http://zlib.net/) to create "random" test cases to check my own implementation.

A big Thank You to Jean-loup and Mark!

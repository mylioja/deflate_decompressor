/*
    Copyright (C) 2020 Martti Ylioja
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <vector>

namespace ZlibInterface {

    enum  {
        //  Compression modes (select one)
        //
        Zlib = 0x0100,
        Gzip = 0x0200,
        Raw =  0x0300,

        //  Compression levels (select one)
        //
        BestSpeed = 1,
        BestCompression = 9,

        //  This is the default combination
        Default = Zlib + BestCompression
    };

    bool deflate(const std::vector<char>& input, std::vector<char>& output, int mode_and_level = Default);

    bool inflate(const char* input, size_t size, std::vector<char>& output);
}

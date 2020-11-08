//
//    Copyright (C) 2020 Martti Ylioja
//    SPDX-License-Identifier: GPL-3.0-or-later
//
#include "zlib_interface.h"

#include "zlib.h"

namespace ZlibInterface {

    bool deflate(const std::vector<char>& input, std::vector<char>& output, int mode_and_level)
    {
        z_stream zs;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = input.size();
        zs.next_in = (Bytef*)input.data();

        gz_header header;
        header.text = 0;
        header.time = 0x01020304;
        header.os = 3;
        header.extra = (Bytef*)"Extra info";
        header.extra_len = 11;  // Length including the '\0' at end
        header.name = (Bytef*)"test/file/name.txt";
        header.comment = (Bytef*)"This a a comment";
        header.hcrc = 1;

        int mode = mode_and_level & 0xff00;
        int window_bits = 15;
        if (mode == Raw)
        {
            window_bits = -window_bits;
        }
        else if (mode == Gzip)
        {
            window_bits += 16;
        }

        int level = mode_and_level & 0x0f;
        if (level < 1 || level > Z_BEST_COMPRESSION)
        {
            level = Z_BEST_COMPRESSION;
        }

        int ret = deflateInit2(
            &zs,
            level,
            Z_DEFLATED,
            window_bits,
            8,
            Z_DEFAULT_STRATEGY
        );

        if (ret != Z_OK)
        {
            return false;
        }

        if (mode == Gzip)
        {
            ret = deflateSetHeader(&zs, &header);
        }

        constexpr int chunk_size = 8*1024;
        Bytef buffer[chunk_size];

        enum class State { Running, Done, Error };
        State state = State::Running;
        do {
            zs.avail_out = chunk_size;
            zs.next_out = buffer;

            ret = ::deflate(&zs, zs.avail_in ? Z_NO_FLUSH : Z_FINISH);
            switch (ret)
            {
            case Z_STREAM_END:
                state = State::Done;
                break;

            case Z_OK:
                break;

            default:
                state = State::Error;
                break;
            }

            if (state != State::Error)
            {
                int available = chunk_size - zs.avail_out;
                output.insert(output.end(), buffer, buffer+available);
            }

        } while (state == State::Running);

        deflateEnd(&zs);

        if (state != State::Done)
        {
            return false;
        }

        return true;
    }


    bool inflate(const char* input, size_t size, std::vector<char>& output)
    {
        z_stream zs;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = size;
        zs.next_in = (Bytef*)input;

        int ret = inflateInit(&zs);
        if (ret != Z_OK)
        {
            return false;
        }

        output.clear();

        constexpr int chunk_size = 8*1024;
        Bytef buffer[chunk_size];

        enum class State { RUNNING, DONE, ERROR };
        State status = State::RUNNING;
        do {
            zs.avail_out = chunk_size;
            zs.next_out = buffer;

            ret = ::inflate(&zs, Z_NO_FLUSH);
            switch (ret)
            {
            case Z_STREAM_END:
                status = State::DONE;
                break;

            case Z_OK:
                break;

            default:
                status = State::ERROR;
                break;
            }

            if (status != State::ERROR)
            {
                int available = chunk_size - zs.avail_out;
                output.insert(output.end(), buffer, buffer+available);
            }

        } while (status == State::RUNNING);

        inflateEnd(&zs);

        if (status != State::DONE)
        {
            return false;
        }

        return true;
    }

} // namespace ZlibInterface


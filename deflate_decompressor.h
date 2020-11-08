/*
    Copyright (C) 2020 Martti Ylioja
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <cstdint>
#include <vector>

class DeflateDecompressor
{
public:
    DeflateDecompressor();

    enum {
        //  No problems detected
        eSUCCESS = 0,

        //  Checksum doesn't match
        eCHECKSUM,

        //  Some problem with the input
        eINVALID_INPUT,
    };

    int decompress(const char* input, size_t size, std::vector<char>& out);

    const char* error_message() const { return m_error_message; }

    static uint32_t adler32(uint32_t adler, const char* input, size_t size);
    static uint32_t crc32(uint32_t crc_in, const char* input, size_t size);

    static const char* get_build_info();

private:

    int report_error(const char* message);
    int report_invalid_codeword();

    //  Possible input formats
    enum class Format { Invalid, Raw, Zlib, Gzip };

    Format skip_gzip_wrapper();
    Format skip_zlib_wrapper();

    int process_uncompressed_block();
    int process_static_huffman_block();
    int process_dynamic_huffman_block();
    int decompress_the_block();

    unsigned next_byte();
    void make_available(int count);
    unsigned peek_bits(int count);
    void drop_bits(int count);
    unsigned get_bits(int count);
    void align_input();
    unsigned read_le_uint16();
    unsigned in_bytes_available();

    bool build_decode_tables(const uint8_t* lengths, int literals_size, int distances_size);

    const uint8_t* m_input = nullptr;
    const uint8_t* m_input_end = nullptr;

    unsigned m_bits = 0;
    int m_bits_available = 0;

    uint32_t* m_code_length_decode_table = nullptr;
    uint32_t* m_literal_length_decode_table = nullptr;
    uint32_t* m_distance_decode_table = nullptr;

    std::vector<char>* m_out;

    std::vector<uint32_t> m_tables;
    const char* m_error_message;
};


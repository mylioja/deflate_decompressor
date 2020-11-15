//
//    Copyright (C) 2020 Martti Ylioja
//    SPDX-License-Identifier: GPL-3.0-or-later
//
#include "deflate_decompressor.h"

#include <algorithm>
#include <climits>
#include <cstring>

namespace {

    //  If build system doesn't provide a timestamp, use this instead
    #ifndef BUILD_DATETIME
    #define BUILD_DATETIME __DATE__ " " __TIME__
    #endif

    //  Release builds set these variables to indicate status of the workspace.
    //  Set them to default values for builds that don't provide the info.
    //
    #ifndef GIT_REVISION
    #define GIT_REVISION "unknown"
    #endif

    #ifndef GIT_STATUS
    #define GIT_STATUS "unknown"
    #endif

    //  This string contains some basic info about the program.
    //
    const char build_info[] =
        "\n"
        "NAME: deflate_decompressor\n"
        "VERSION: 1.0\n"
        "COPYRIGHT: Copyright (C) 2020 Martti Ylioja\n"
        "SPDX-License-Identifier: GPL-3.0-or-later\n"
        "BUILD_DATETIME: " BUILD_DATETIME "\n"
        "GIT_REVISION: " GIT_REVISION "\n"
        "GIT_STATUS: " GIT_STATUS "\n";


    constexpr int code_length_codeword_max_length = 7;
    constexpr int max_possibe_codeword_length = 15;

    //  Maximum counts for each type of codeword
    constexpr int max_code_length_codewords = 19;
    constexpr int max_literal_length_codewords = 288;
    constexpr int max_distance_codewords = 32;
    constexpr int max_possible_codewords = max_literal_length_codewords;

    constexpr int code_length_table_bits = 7;
    constexpr int literal_length_table_bits = 10;
    constexpr int distance_table_bits = 8;

    constexpr int lengths_array_size = max_literal_length_codewords + max_distance_codewords;

    constexpr unsigned subtable_flag = 0x80;
    constexpr unsigned literal_flag = 0x40;
    constexpr unsigned extra_mask = 0x3f;
    constexpr unsigned data_shift = 8;

    constexpr unsigned invalid_codeword = 0xff;

    constexpr uint32_t pack(unsigned data) { return data << data_shift; }
    constexpr uint32_t literal(unsigned data) { return pack(data) | literal_flag; }
    constexpr uint32_t pack2(unsigned data, unsigned extra) { return pack(pack(data) | extra); }

    static const uint32_t code_length_values[max_code_length_codewords] = {
        pack(0), pack(1), pack(2), pack(3), pack(4), pack(5), pack(6),
        pack(7), pack(8), pack(9), pack(10), pack(11), pack(12), pack(13),
        pack(14), pack(15), pack(16), pack(17), pack(18)
    };

    static const uint32_t literal_length_values[max_literal_length_codewords] = {
        literal(0), literal(1), literal(2), literal(3), literal(4), literal(5),
        literal(6), literal(7), literal(8), literal(9), literal(10),
        literal(11), literal(12), literal(13), literal(14), literal(15),
        literal(16), literal(17), literal(18), literal(19), literal(20),
        literal(21), literal(22), literal(23), literal(24), literal(25),
        literal(26), literal(27), literal(28), literal(29), literal(30),
        literal(31), literal(32), literal(33), literal(34), literal(35),
        literal(36), literal(37), literal(38), literal(39), literal(40),
        literal(41), literal(42), literal(43), literal(44), literal(45),
        literal(46), literal(47), literal(48), literal(49), literal(50),
        literal(51), literal(52), literal(53), literal(54), literal(55),
        literal(56), literal(57), literal(58), literal(59), literal(60),
        literal(61), literal(62), literal(63), literal(64), literal(65),
        literal(66), literal(67), literal(68), literal(69), literal(70),
        literal(71), literal(72), literal(73), literal(74), literal(75),
        literal(76), literal(77), literal(78), literal(79), literal(80),
        literal(81), literal(82), literal(83), literal(84), literal(85),
        literal(86), literal(87), literal(88), literal(89), literal(90),
        literal(91), literal(92), literal(93), literal(94), literal(95),
        literal(96), literal(97), literal(98), literal(99), literal(100),
        literal(101), literal(102), literal(103), literal(104), literal(105),
        literal(106), literal(107), literal(108), literal(109), literal(110),
        literal(111), literal(112), literal(113), literal(114), literal(115),
        literal(116), literal(117), literal(118), literal(119), literal(120),
        literal(121), literal(122), literal(123), literal(124), literal(125),
        literal(126), literal(127), literal(128), literal(129), literal(130),
        literal(131), literal(132), literal(133), literal(134), literal(135),
        literal(136), literal(137), literal(138), literal(139), literal(140),
        literal(141), literal(142), literal(143), literal(144), literal(145),
        literal(146), literal(147), literal(148), literal(149), literal(150),
        literal(151), literal(152), literal(153), literal(154), literal(155),
        literal(156), literal(157), literal(158), literal(159), literal(160),
        literal(161), literal(162), literal(163), literal(164), literal(165),
        literal(166), literal(167), literal(168), literal(169), literal(170),
        literal(171), literal(172), literal(173), literal(174), literal(175),
        literal(176), literal(177), literal(178), literal(179), literal(180),
        literal(181), literal(182), literal(183), literal(184), literal(185),
        literal(186), literal(187), literal(188), literal(189), literal(190),
        literal(191), literal(192), literal(193), literal(194), literal(195),
        literal(196), literal(197), literal(198), literal(199), literal(200),
        literal(201), literal(202), literal(203), literal(204), literal(205),
        literal(206), literal(207), literal(208), literal(209), literal(210),
        literal(211), literal(212), literal(213), literal(214), literal(215),
        literal(216), literal(217), literal(218), literal(219), literal(220),
        literal(221), literal(222), literal(223), literal(224), literal(225),
        literal(226), literal(227), literal(228), literal(229), literal(230),
        literal(231), literal(232), literal(233), literal(234), literal(235),
        literal(236), literal(237), literal(238), literal(239), literal(240),
        literal(241), literal(242), literal(243), literal(244), literal(245),
        literal(246), literal(247), literal(248), literal(249), literal(250),
        literal(251), literal(252), literal(253), literal(254), literal(255),

        pack(0), // end of block

        pack2(3, 0), pack2(4, 0), pack2(5, 0), pack2(6, 0), pack2(7, 0), pack2(8, 0), pack2(9, 0),
        pack2(10, 0), pack2(11, 1), pack2(13, 1), pack2(15, 1), pack2(17, 1), pack2(19, 2),
        pack2(23, 2), pack2(27, 2), pack2(31, 2), pack2(35, 3), pack2(43, 3), pack2(51, 3),
        pack2(59, 3), pack2(67, 4), pack2(83, 4), pack2(99, 4), pack2(115, 4), pack2(131, 5),
        pack2(163, 5), pack2(195, 5), pack2(227, 5), pack2(258, 0), pack2(258, 0), pack2(258, 0),
    };

    static const uint32_t distance_values[max_distance_codewords] = {
        pack2(1, 0), pack2(2, 0), pack2(3, 0), pack2(4, 0), pack2(5, 1), pack2(7, 1),
        pack2(9, 2), pack2(13, 2), pack2(17, 3), pack2(25, 3), pack2(33, 4), pack2(49, 4),
        pack2(65, 5), pack2(97, 5), pack2(129, 6), pack2(193, 6), pack2(257, 7),
        pack2(385, 7), pack2(513, 8), pack2(769, 8), pack2(1025, 9), pack2(1537, 9),
        pack2(2049, 10), pack2(3073, 10), pack2(4097, 11), pack2(6145, 11), pack2(8193, 12),
        pack2(12289, 12), pack2(16385, 13), pack2(24577, 13), pack2(32769, 14), pack2(49153, 14),
    };


    //  Increment a bit reversed codeword of a given length
    unsigned bit_reversed_increment(unsigned codeword, int length)
    {
        unsigned result = 0;
        unsigned bit = 1 << (length - 1);
        while (codeword & bit)
        {
            bit >>= 1;
        }

        if (bit != 0)
        {
            result = codeword & (bit - 1);
            result |= bit;
        }

        return result;
    }


    //  Fill words of the decode table with a given value.
    void fill_decode_table(uint32_t* table, int size, uint32_t value, int stride)
    {
        for (int ix = 0; ix < size; ix += stride)
        {
            table[ix] = value;
        }
    }


    //  Double the given decode table by appending current contents to the end.
    //  Doesn't check for overflow: The required space must be available.
    void double_the_decode_table(uint32_t* table, int size)
    {
        std::memcpy(table + size, table, size * sizeof table[0]);
    }


    bool build_decode_table(
        uint32_t* decode_table,
        const int table_bits,
        const uint8_t* codeword_lengths,
        const uint32_t* symbol_values,
        const int number_of_symbols)
    {
        unsigned symbols_array[max_possible_codewords];
        unsigned* symbols = symbols_array;

        //  Number of codewords with each length
        int length_counts[max_possibe_codeword_length + 1];
        std::memset(length_counts, 0, sizeof length_counts);
        for (int symbol = 0; symbol < number_of_symbols; symbol++)
        {
            length_counts[codeword_lengths[symbol]]++;
        }

        //  Find length of the longest codeword present
        int longest_codeword_length = max_possibe_codeword_length;
        while (longest_codeword_length && length_counts[longest_codeword_length] == 0)
        {
            --longest_codeword_length;
        }

        //  No symbols defined
        if (longest_codeword_length == 0)
        {
            //  Fine so far, but attempting to use any codeword will trigger an error
            fill_decode_table(decode_table, 1 << table_bits, invalid_codeword, 1);
            return true;
        }

        int codespace_used = 0;
        for (int length = 1; length <= longest_codeword_length; ++length)
        {
            codespace_used = 2*codespace_used + length_counts[length];
        }

        int codespace_size = 1 << longest_codeword_length;

        //  More symbols defined than needed to fill the codespace
        if (codespace_used > codespace_size)
        {
            return false;
        }

        //  The codespace not completely covered
        if (codespace_used < codespace_size)
        {
            //  Accept the special case of only one symbol
            if (longest_codeword_length == 1)
            {
                int size = 1 << table_bits;

                //  Mark codeword "0" as invalid
                fill_decode_table(decode_table, size, invalid_codeword, 2);

                //  Add codeword "1" with the given value
                uint32_t entry = symbol_values[1] | 1;
                fill_decode_table(decode_table+1, size-1, entry, 2);
                return true;
            }

            return false;
        }

        //  Compute offsets into the table for each codeword length
        int offsets[max_possibe_codeword_length + 1];
        offsets[0] = 0;
        offsets[1] = length_counts[0];
        for (int length = 1; length < longest_codeword_length; ++length)
        {
            offsets[length + 1] = offsets[length] + length_counts[length];
        }

        //  Arrange symbols by length, and by symbol order within the same length
        for (int symbol = 0; symbol < number_of_symbols; ++symbol)
        {
            symbols[offsets[codeword_lengths[symbol]]++] = symbol;
        }

        //  Skip past unused symbols
        symbols += offsets[0];

        //  Find length of the shortest defined codeword
        int codeword_length = 1;
        while (length_counts[codeword_length] == 0)
        {
            ++codeword_length;
        }

        int count = length_counts[codeword_length];
        int end_index = 1 << codeword_length;

        //  Add all the codewords that don't need extra tables
        unsigned codeword = 0;
        while (codeword_length <= table_bits)
        {
            unsigned all_ones = end_index-1;

            //  Add all codewords with the current length
            while (count--)
            {
                decode_table[codeword] = symbol_values[*symbols++] | codeword_length;

                //  The last codeword is all ones
                if (codeword == all_ones)
                {
                    //  Expand the decode table up to the full size
                    while (codeword_length < table_bits)
                    {
                        double_the_decode_table(decode_table, end_index);
                        end_index *= 2;
                        ++codeword_length;
                    }

                    //  This is the normal exit if no subtables are required.
                    return true;  // OK exit
                }

                codeword = bit_reversed_increment(codeword, codeword_length);
            };

            //  Proceed to next length, skipping lengths without any symbols
            for (;;)
            {
                ++codeword_length;
                if (codeword_length <= table_bits)
                {
                    double_the_decode_table(decode_table, end_index);
                    end_index *= 2;
                }

                count = length_counts[codeword_length];
                if (count)
                {
                    break;
                }
            };
        }

        //  Add the codewords that require subtables
        end_index = 1 << table_bits;
        unsigned prefix = end_index;
        unsigned prefix_mask = end_index - 1;
        int begin_index = 0;
        int subtable_size = 0;
        for (;;)
        {
            //  Number of extra bits needed in addition to the table bits
            int extra_bits = codeword_length - table_bits;

            //  If there's a new prefix, begin a new subtable
            unsigned next_prefix = codeword & prefix_mask;
            if (next_prefix != prefix)
            {
                prefix = next_prefix;
                begin_index = end_index;

                //  Compute the required size
                int subtable_bits = extra_bits;
                subtable_size = 1 << subtable_bits;
                int codespace_used = count;
                while (codespace_used < subtable_size)
                {
                    subtable_bits++;
                    subtable_size = 1 << subtable_bits;
                    int total_bits = table_bits + subtable_bits;
                    codespace_used = 2*codespace_used + length_counts[total_bits];
                }

                //  Update end_index past the new subtable
                end_index = begin_index + subtable_size;

                //  Create a link from the main table to the subtable
                decode_table[prefix] = pack(begin_index) | subtable_flag | subtable_bits;
            }

            //  Fill subtable entries for the current codeword.
            uint32_t* subtable = decode_table + begin_index;
            uint32_t entry = symbol_values[*symbols++] | extra_bits;
            const int stride = 1 << extra_bits;
            for (int ix = codeword >> table_bits; ix < subtable_size; ix += stride)
            {
                subtable[ix] = entry;
            };

            //  The last codeword is all ones
            unsigned all_ones = (1 << codeword_length) - 1;
            if (codeword == all_ones)
            {
                //  This is the normal exit if subtables were needed.
                return true;  // OK exit
            }

            //  Advance to the next codeword
            codeword = bit_reversed_increment(codeword, codeword_length);

            //  Reduce the remaing count of codewords with this length
            --count;

            //  If the count went to zero, advance to the next length present
            while (!count)
            {
                //  This isn't an infinite loop, even though it looks like one.
                //  The "all_ones" condition gets triggered if no codewords with longer
                //  lengths exist, so a nonzero count is guaranteed to be found.
                ++codeword_length;
                count = length_counts[codeword_length];
            }
        }

        //  Should never happen
        return false;
    }


    uint32_t get_little_endian_uint32(const unsigned char* ptr)
    {
        return ptr[0] + (ptr[1] << 8) + (ptr[2] << 16) + (ptr[3] << 24);
    }


    uint32_t get_big_endian_uint32(const unsigned char* ptr)
    {
        return ptr[3] + (ptr[2] << 8) + (ptr[1] << 16) + (ptr[0] << 24);
    }

} // namespace


DeflateDecompressor::DeflateDecompressor()
{
    //  Sizes for the three decode tables
    constexpr int code_lengths = 128;   // from zlib enough 19 7 7
    constexpr int literal_lengths_max = 1334;  // from zlib enough 288 10 15
    constexpr int distances_max = 402;  // from zlib enough 32 8 15

    //  Place them all into a single vector
    m_tables.resize(code_lengths + literal_lengths_max + distances_max);
    m_code_length_decode_table = m_tables.data();
    m_literal_length_decode_table = m_code_length_decode_table + code_lengths;
    m_distance_decode_table = m_literal_length_decode_table + literal_lengths_max;
}


const char* DeflateDecompressor::get_build_info()
{
    return build_info;
}


unsigned DeflateDecompressor::next_byte()
{
    if (m_input != m_input_end)
    {
        return *m_input++;
    }

    return 0;
}


void DeflateDecompressor::make_available(int count)
{
    while (m_bits_available < count)
    {
        unsigned byte = next_byte();
        m_bits |= (byte << m_bits_available);
        m_bits_available += 8;
    }
}


unsigned DeflateDecompressor::peek_bits(int count)
{
    //  Precomputed values for ((1 << count)-1)
    //  The x86 has very slow shifts, so this simple
    //  low level optimization is worthwhile.
    //  Probably has the opposite effect on ARM.
    static const int low_bits_mask[17] = {
        0, 1, 3, 7, 15, 31, 63, 127,
        255, 511, 1023, 2047, 4095,
        8191, 16383, 32767, 65535,
    };

    make_available(count);
    return m_bits & low_bits_mask[count];
}


void DeflateDecompressor::drop_bits(int count)
{
    m_bits = (m_bits >> count);
    m_bits_available -= count;
}


unsigned DeflateDecompressor::get_bits(int count)
{
    unsigned bits = peek_bits(count);
    m_bits = (m_bits >> count);
    m_bits_available -= count;
    return bits;
}


void DeflateDecompressor::align_input()
{
    int bytes_loaded = m_bits_available/8;
    m_input -= bytes_loaded;
    m_bits = 0;
    m_bits_available = 0;
}


unsigned DeflateDecompressor::read_le_uint16()
{
    unsigned result = next_byte();
    result += next_byte() << 8;
    return result;
}


unsigned DeflateDecompressor::in_bytes_available()
{
    return m_input_end - m_input;
}


bool DeflateDecompressor::build_decode_tables(const uint8_t* lengths, int literals_size, int distances_size)
{
    if (!build_decode_table(
        m_distance_decode_table,
        distance_table_bits,
        lengths + literals_size,
        distance_values,
        distances_size))
    {
        return false;
    }

    if (!build_decode_table(
        m_literal_length_decode_table,
        literal_length_table_bits,
        lengths,
        literal_length_values,
        literals_size))
    {
        return false;
    }

    return true;
}


int DeflateDecompressor::decompress(const char* input, size_t size, std::vector<char>& out)
{
    m_error_message = nullptr;

    m_input = reinterpret_cast<const uint8_t*>(input);
    m_input_end = m_input + size;
    m_bits = 0;
    m_bits_available = 0;
    m_out = &out;

    //  Make sure the output is empty
    m_out->clear();

    //  Detect format and skip the wrapper if present
    Format format = skip_gzip_wrapper();
    if (format == Format::Raw)
    {
        format = skip_zlib_wrapper();
    }

    //  A wrapper was detected, but had problems
    if (format == Format::Invalid)
    {
        return eInvalidInput;
    }

    //  The gzip wrapper contains original uncompressed data size.
    //  Use it to reserve the output vector.
    if (format == Format::Gzip)
    {
        m_out->reserve(get_little_endian_uint32(m_input_end+4));
    }

    //  Valid block types
    constexpr int uncompressed = 0;
    constexpr int static_huffman = 1;
    constexpr int dynamic_huffman = 2;

    int err = 0;
    for (;;)
    {
        bool is_final_block = get_bits(1);
        unsigned block_type = get_bits(2);

        switch (block_type)
        {
        case uncompressed:
            err = process_uncompressed_block();
            break;

        case static_huffman:
            err = process_static_huffman_block();
            break;

        case dynamic_huffman:
            err = process_dynamic_huffman_block();
            break;

        default:
            err = eInvalidInput;
            break;
        }

        if (err || is_final_block)
        {
            break;
        }
    }

    if (err)
    {
        return err;
    }

    //  Verify the checksum if it's available
    uint32_t expected = 0;
    uint32_t computed = 0;
    switch (format)
    {
    case Format::Zlib:
        expected = get_big_endian_uint32(m_input_end);
        computed = adler32(1, m_out->data(), m_out->size());
        break;

    case Format::Gzip:
        expected = get_little_endian_uint32(m_input_end);
        computed = crc32(0, m_out->data(), m_out->size());
        break;

    default:
        break;
    }

    if (expected != computed)
    {
        report_error("ERR15: Data checksum mismatch");
        return eChecksum;
    }

    return eSuccess;
}


int DeflateDecompressor::report_error(const char* message)
{
    m_error_message = message;
    return eInvalidInput;
}


int DeflateDecompressor::report_invalid_codeword()
{
    return report_error("ERR01: Invalid codeword in input data");
}


DeflateDecompressor::Format DeflateDecompressor::skip_gzip_wrapper()
{
    //  The rfc1952 defines these flags
    //constexpr int text_flag = 0x01;
    constexpr int header_crc_flag = 0x02;
    constexpr int extra_info_flag = 0x04;
    constexpr int name_flag = 0x08;
    constexpr int comment_flag = 0x10;

    //  All known flags together
    constexpr int known_flags = 0x1f;

    //  Must have at least a ten byte header, a four byte checksum,
    //  and a four byte size word, assuming no data at all.
    if (in_bytes_available() < 18)
    {
        return Format::Raw;
    }

    const unsigned char* header = m_input;

    //  id1, id2, and "compression method" have fixed values
    if (header[0] != 31 || header[1] != 139 || header[2] != 8)
    {
        return Format::Raw;
    }

    //  Unknown flags are an error, according to the rfc1952
    int flags = header[3];
    if (flags & ~known_flags)
    {
        report_error("ERR10: Unknown flags in gzip header");
        return Format::Invalid;
    }

    //  Skip the header and the trailer
    m_input += 10;
    m_input_end -= 8;

    //  Skip the "extra field"
    if (flags & extra_info_flag)
    {
        m_input += read_le_uint16();
    }

    //  Skip file name
    if (flags & name_flag)
    {
        while (next_byte())
        {
        }
    }

    //  Skip comment
    if (flags & comment_flag)
    {
        while (next_byte())
        {
        }
    }

    //  Verify the header checksum if it's present
    if (flags & header_crc_flag)
    {
        uint32_t computed = crc32(0, reinterpret_cast<const char*>(header), m_input - header);
        uint32_t expected = read_le_uint16();
        if (expected != (computed & 0xffff))
        {
            report_error("ERR11: Incorrect checksum in gzip header");
            return Format::Invalid;
        }
    }

    return Format::Gzip;
}


DeflateDecompressor::Format DeflateDecompressor::skip_zlib_wrapper()
{
    //  Must have at least a two byte header, and a four byte checksum,
    //  meaning a minimum of 6 bytes even without any data
    if (in_bytes_available() < 6)
    {
        return Format::Raw;
    }

    //  First byte has the "method and info"
    int method_and_info = m_input[0];

    //  The method must be 8
    if ((method_and_info & 0x0f) != 8)
    {
        return Format::Raw;
    }

    //  Already at this point it's quite certain that a zlib header is present.
    //  Decoding the data as raw DEFLATE input would interpret it as starting
    //  with an uncompressed block. A situation that any reasonable encoder would
    //  have encoded with a first byte of plain zero.

    //  Second byte is the flags
    int flags = m_input[1];

    //  The two bytes, taken as a 16 bit integer, must be a multiple of 31
    if ((256*method_and_info + flags) % 31)
    {
        report_error("ERR12: Incorrect FCHECK value in zlib header");
        return Format::Invalid;
    }

    //  CINFO must not be above 7
    if ((method_and_info >> 4) > 7)
    {
        report_error("ERR13: Incorrect CINFO value in zlib header");
        return Format::Invalid;
    }

    //  A preset dictionary (FDICT flag, bit 5) isn't allowed
    if (flags & (1 << 5))
    {
        report_error("ERR14: A preset dictionary (FDICT flag in zlib header) not supported");
        return Format::Invalid;
    }

    m_input += 2;       // skip the zlib header
    m_input_end -= 4;   // skip the checksum

    return Format::Zlib;
}


int DeflateDecompressor::process_uncompressed_block()
{
    align_input();

    if (in_bytes_available() < 4)
    {
        return report_error("ERR02: Not enough input for an uncompressed block");
    }

    unsigned len = read_le_uint16();
    unsigned nlen = read_le_uint16();

    if ((~nlen & 0xffff) != len)
    {
        return report_error("ERR03: Uncompressed block length mismatch");
    }

    if (len > in_bytes_available())
    {
        return report_error("ERR04: Uncompressed block size more than input bytes available");
    }

    m_out->insert(m_out->end(), m_input, m_input+len);
    m_input += len;

    return eSuccess;
}


int DeflateDecompressor::process_static_huffman_block()
{
    uint8_t lengths[lengths_array_size];

    int ix = 0;
    for (; ix < 144; ix++) lengths[ix] = 8;
    for (; ix < 256; ix++) lengths[ix] = 9;
    for (; ix < 280; ix++) lengths[ix] = 7;
    for (; ix < 288; ix++) lengths[ix] = 8;

    for (; ix < 288 + 32; ix++) lengths[ix] = 5;

    if (!build_decode_tables(lengths, 288, 32))
    {
        return eInvalidInput;
    }

    return decompress_the_block();
}


int DeflateDecompressor::process_dynamic_huffman_block()
{
    constexpr int max_code_length_codewords = 19;
    static const uint8_t code_length_code_order[max_code_length_codewords] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    uint8_t lengths[lengths_array_size];

    //  Counts of various codes
    int literal_length_codes = get_bits(5) + 257;
    int distance_codes = get_bits(5) + 1;
    int code_length_codes = get_bits(4) + 4;

    //  Codeword lengths for the code length alphabet in the special order
    //  given by the code_length_code_order array
    int ix = 0;
    std::memset(lengths, 0, max_code_length_codewords);
    for (; ix < code_length_codes; ++ix)
    {
        lengths[code_length_code_order[ix]] = get_bits(3);
    }

    //  Build the decode table for the code length alphabet
    if (!build_decode_table(
        m_code_length_decode_table,
        code_length_table_bits,
        lengths,
        code_length_values,
        max_code_length_codewords))
    {
        return eInvalidInput;
    }

    //  Get the literal length and distance codeword sizes
    int expected_count = literal_length_codes + distance_codes;
    if (expected_count > lengths_array_size)
    {
        return report_error("ERR05: Too many codeword lengths in a dynamic block");
    }

    ix = 0;
    while (ix < expected_count)
    {
        uint32_t entry = m_code_length_decode_table[peek_bits(code_length_codeword_max_length)];
        if (entry == invalid_codeword)
        {
            return report_invalid_codeword();
        }

        drop_bits(entry & extra_mask);
        unsigned symbol = entry >> data_shift;

        //  single explicit length value
        if (symbol < 16)
        {
            lengths[ix++] = symbol;
            continue;
        }

        //  repeated value
        int count = 0;
        uint32_t value = 0;
        switch (symbol)
        {
        case 16:
            //  A previous value must exist.
            if (ix == 0)
            {
                return report_error("ERR06: Repeat value without a value to repeat");
            }

            //  Repeat the value 3 to 6 times
            value = lengths[ix - 1];
            count = 3 + get_bits(2);
            break;

        case 17:
            count = 3 + get_bits(3);  // repeat zero 3 to 10 times
            break;

        case 18:
            count = 11 + get_bits(7); // repeat zero 11 to 138 times
            break;

        default:
            return report_error("ERR07: Invalid repeat encoding");
            break;
        }

        //  Should stay within array limits
        if (ix + count > expected_count)
        {
            return report_error("ERR08: Repeat value too big for length table size");
        }

        std::fill(lengths+ix, lengths+ix+count, value);
        ix += count;
    }

    if (!build_decode_tables(lengths, literal_length_codes, distance_codes))
    {
        return eInvalidInput;
    }

    return decompress_the_block();
}


int DeflateDecompressor::decompress_the_block()
{
    for (;;)
    {
        //  Decode the length
        unsigned index = peek_bits(literal_length_table_bits);
        uint32_t entry = m_literal_length_decode_table[index];
        if (entry == invalid_codeword)
        {
            return report_invalid_codeword();
        }

        int bit_count = entry & extra_mask;
        if (entry & subtable_flag)
        {
            drop_bits(literal_length_table_bits);
            index = (entry >> data_shift) + peek_bits(bit_count);
            entry = m_literal_length_decode_table[index];
            if (entry == invalid_codeword)
            {
                return report_invalid_codeword();
            }

            bit_count = entry & extra_mask;
        }
        drop_bits(bit_count);
        if (entry & literal_flag)
        {
            m_out->push_back(entry >> data_shift);
            continue;
        }

        entry >>= data_shift;

        //  End of block
        if (entry == 0)
        {
            return eSuccess;
        }

        unsigned length = entry >> data_shift;
        int extra = entry & extra_mask;
        if (extra)
        {
            length += get_bits(extra);
        }

        //  distance

        index = peek_bits(distance_table_bits);
        entry = m_distance_decode_table[index];
        if (entry == invalid_codeword)
        {
            return report_invalid_codeword();
        }

        bit_count = entry & extra_mask;
        if (entry & subtable_flag)
        {
            drop_bits(distance_table_bits);
            index = (entry >> data_shift) + peek_bits(bit_count);
            entry = m_distance_decode_table[index];
            if (entry == invalid_codeword)
            {
                return report_invalid_codeword();
            }

            bit_count = entry & extra_mask;
        }
        drop_bits(bit_count);

        entry >>= data_shift;

        unsigned distance = entry >> data_shift;
        extra = entry & extra_mask;
        if (extra)
        {
            distance += get_bits(extra);
        }

        //  Current size of the output
        unsigned size = m_out->size();

        //  Distance must be within the existing buffer
        if (distance > size)
        {
            return report_error("ERR09: Encoded distance not within buffer limits");
        }

        //  Special case of one repeating character: Let resize do the copying.
        //  This happens really often.
        if (distance == 1)
        {
            const char ch = m_out->back();
            m_out->resize(size + length, ch);
            continue;
        }

        //  Make sure that pointers stay valid during copying
        m_out->reserve(size + length);

        //  Data to be copied
        const char* source = m_out->data() + size - distance;

        //  Use "insert" if the the complete source text is within the current buffer.
        if (length <= distance)
        {
            m_out->insert(m_out->end(), source, source + length);
            continue;
        }

        //  The normal "non-special" case, which is in fact rather rare.
        //  Copy character by character, possibly overlapping the new data.
        while (length--)
        {
            m_out->push_back(*source++);
        }
    }

    return eSuccess;
}


uint32_t DeflateDecompressor::adler32(uint32_t adler, const char* input, size_t size)
{
    constexpr uint32_t divisor = 65521;
    constexpr uint32_t max_batch = 5552;

    uint32_t s1 = adler & 0xFFFF;
    uint32_t s2 = adler >> 16;

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(input);
    const uint8_t* const input_end = ptr + size;
    while (ptr != input_end)
    {
        unsigned batch_size = input_end - ptr;
        if (batch_size > max_batch)
        {
            batch_size = max_batch;
        }

        const uint8_t* batch_end = ptr + batch_size;
        int unrolled = batch_size / 4;

        while (unrolled--)
        {
            s1 += ptr[0];
            s2 += s1;
            s1 += ptr[1];
            s2 += s1;
            s1 += ptr[2];
            s2 += s1;
            s1 += ptr[3];
            ptr += 4;
            s2 += s1;
        }
        while (ptr != batch_end)
        {
            s1 += *ptr++;
            s2 += s1;
        }
        s1 %= divisor;
        s2 %= divisor;
    }

    return s1 | (s2 << 16);
}


uint32_t DeflateDecompressor::crc32(uint32_t crc, const char* input, size_t size)
{
    static const uint32_t table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535,
        0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd,
        0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d,
        0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
        0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac,
        0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
        0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
        0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb,
        0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea,
        0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce,
        0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
        0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409,
        0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739,
        0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268,
        0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0,
        0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8,
        0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
        0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703,
        0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
        0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae,
        0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6,
        0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d,
        0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5,
        0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
        0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(input);

    crc = ~crc;
    while (size--)
    {
        crc = (crc >> 8) ^ table[(crc ^ *ptr++) & 0xff];
    }

    return ~crc;
}

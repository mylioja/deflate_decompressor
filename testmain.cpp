//
//    Copyright (C) 2020 Martti Ylioja
//    SPDX-License-Identifier: GPL-3.0-or-later
//
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "deflate_decompressor.h"
#include "performance_timer.h"
#include "zlib_interface.h"

namespace {

    constexpr int max_count = 258;
    constexpr int max_distance = 32768;

    class DeflateTester
    {
    public:
        DeflateTester(int max_size);

        bool run_all_tests();

        bool generate_data();

        //  Compress the test data with the official zlib deflate.
        bool compress();

        //  Possible test data generators
        enum {
            kEmptyInput,
            kRandomMix,
            kRandomAlphabet,
            kRandomZeroes,
            kSimpleFill,
            kRandomBytes,
            kRepeatedSequences,
            kRandomRepeats,
            kSpecialCases,
            kAllDone,
        };

        bool compare_performance(int input_size);

    private:
        int data_size() const { return int(m_test_data.size()); }

        //  For speed comparison
        void decompress_with_own_code();
        void decompress_with_zlib();

        //  Return a positive integer less than a limit
        int random_int(int limit);

        //  Return true with the given probability percentage
        int random_bool(int percent);

        uint32_t xorshift32();

        void begin_test(const char* name);
        bool test_done();

        bool simple_fill();
        bool random_mix_fill();
        bool random_alphabet_fill();
        bool random_zeroes_fill();
        bool random_bytes_fill();
        bool repeated_sequences_fill();
        bool random_repeats_fill();
        bool special_cases_fill();

        int m_max_size;
        int m_compressed_size = 0;
        int m_generator = kEmptyInput;
        int m_generator_state = 0;
        int m_mode = 0;

        const char* m_test_name = "";

        uint32_t m_xorshift32_state = 0x1234567;

        std::vector<char> m_test_data;
        std::vector<char> m_compressed;
        std::vector<char> m_decompressed;
    };


    DeflateTester::DeflateTester(int max_size)
        : m_max_size(max_size)
    {
        m_test_data.reserve(max_size);
        m_compressed.reserve(max_size + max_size/200 + 100);  // About 0.5% extra
    }


    bool DeflateTester::run_all_tests()
    {
        DeflateDecompressor deflate;

        m_generator = kEmptyInput;
        m_test_data.clear();

        while (generate_data())
        {
            if (!compress())
            {
                return false;
            }

            const char* input = m_compressed.data();
            size_t input_size = m_compressed_size;

            m_decompressed.clear();
            int err = deflate.decompress(input, input_size, m_decompressed);
            if (err)
            {
                std::cerr << "DeflateDecompressor decompress error code: " << err << "\n";
                return false;
            }

            if (m_decompressed != m_test_data)
            {
                std::cerr << "Error: Invalid decompress result\n";
                return false;
            }

        }

        return true;
    }


    bool DeflateTester::generate_data()
    {
        switch (m_generator)
        {
        case kEmptyInput:
            ++m_generator;
            return true;
            break;

        case kRandomMix:
            return random_mix_fill();
            break;

        case kRandomAlphabet:
            return random_alphabet_fill();
            break;

        case kRandomZeroes:
            return random_zeroes_fill();
            break;

        case kSimpleFill:
            return simple_fill();
            break;

        case kRandomBytes:
            return random_bytes_fill();
            break;

        case kRepeatedSequences:
            return repeated_sequences_fill();
            break;

        case kRandomRepeats:
            return random_repeats_fill();
            break;

        case kSpecialCases:
            return special_cases_fill();
            break;

        case kAllDone:
            break;
        };

        return false;
    }


    bool DeflateTester::compress()
    {
        static const int mode[] = {
            ZlibInterface::Raw,
            ZlibInterface::Zlib,
            ZlibInterface::Gzip,
        };

        ++m_mode;
        if (m_mode == sizeof mode / sizeof mode[0])
        {
            m_mode = 0;
        }

        m_compressed_size = 0;
        m_compressed.clear();

        if (!ZlibInterface::deflate(
            m_test_data,
            m_compressed,
            mode[m_mode] + ZlibInterface::BestCompression))
        {
            return false;
        }

        m_compressed_size = m_compressed.size();

        return true;
    }


    bool DeflateTester::compare_performance(int input_size)
    {
        if (input_size > m_max_size)
        {
            input_size = m_max_size;
        }

        int64_t own_elapsed_time = 0;
        int64_t zlib_elapsed_time = 0;

        int loop_count = 200;
        while (loop_count--)
        {
            m_test_data.clear();
            int count = input_size;
            while (count--)
            {
                m_test_data.push_back(' ' + (random_bool(30) ? random_int(5) : 0));
            }

            m_mode = 2;
            compress();

            int64_t start_time = PerformanceTimer::get_timestamp();
            if (loop_count & 1)
            {
                decompress_with_own_code();
                own_elapsed_time += PerformanceTimer::get_elapsed_time(start_time);

                decompress_with_zlib();
                zlib_elapsed_time += PerformanceTimer::get_elapsed_time(start_time);
            }
            else
            {
                decompress_with_zlib();
                zlib_elapsed_time += PerformanceTimer::get_elapsed_time(start_time);

                decompress_with_own_code();
                own_elapsed_time += PerformanceTimer::get_elapsed_time(start_time);
            }
        }

        std::cout << "Own:  " << std::setw(8) << own_elapsed_time << " microseconds\n";
        std::cout << "Zlib: " << std::setw(8) << zlib_elapsed_time << " microseconds\n";

        return true;
    }


    void DeflateTester::decompress_with_own_code()
    {
        DeflateDecompressor deflate;

        const char* input = m_compressed.data();
        size_t input_size = m_compressed_size;

        deflate.decompress(input, input_size, m_decompressed);
    }


    void DeflateTester::decompress_with_zlib()
    {
        const char* input = m_compressed.data();
        size_t input_size = m_compressed_size;

        ZlibInterface::inflate(input, input_size, m_decompressed);
    }


    int DeflateTester::random_int(int limit)
    {
        int result = int(xorshift32() % limit);
        return result;
    }


    int DeflateTester::random_bool(int percent)
    {
        if (percent < 1)
        {
            return false;
        }

        if (percent >= 100)
        {
            return true;
        }

        constexpr uint32_t one_percent = 0xffffffff / 100;
        uint32_t limit = percent * one_percent;
        return  xorshift32() < limit;
    }


    uint32_t DeflateTester::xorshift32()
    {
        // Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
        uint32_t x = m_xorshift32_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        m_xorshift32_state = x;
        return x;
    }

    void DeflateTester::begin_test(const char* name)
    {
        if (m_test_data.empty())
        {
            m_test_name = name;
            std::cout << name << '\n';
        }
    }


    bool DeflateTester::test_done()
    {
        std::cout << m_test_name << " OK\n";
        m_test_data.clear();
        ++m_generator;
        return true;
    }


    bool DeflateTester::random_alphabet_fill()
    {
        begin_test("Random alphabet test");

        int space_available = m_max_size - data_size();
        if (space_available > 0)
        {
            int count = random_int(200)+1;
            if (count > space_available)
            {
                count = space_available;
            }

            while (count--)
            {
                m_test_data.push_back('A' + (random_bool(30) ? random_int(15) : 0));
            }

            return true;
        }

        return test_done();
    }


    bool DeflateTester::random_zeroes_fill()
    {
        begin_test("Random zeroes test");

        int space_available = m_max_size - data_size();
        if (space_available > 2)
        {
            int count = random_int(140)+2;
            if (count > space_available)
            {
                count = space_available;
            }

            m_test_data.insert(m_test_data.end(), count-1, '\0');
            m_test_data.push_back('A' + random_int(19));
            return true;
        }

        return test_done();
    }


    bool DeflateTester::simple_fill()
    {
        begin_test("Simple fill test");

        if (data_size() < max_count + max_distance)
        {
            m_test_data.push_back('A');
            return true;
        }

        return test_done();
    }


    bool DeflateTester::random_mix_fill()
    {
        begin_test("Random mix test");

        int space_available = m_max_size - data_size();
        if (space_available > 0)
        {
            int count = random_int(300)+1;
            if (count > space_available)
            {
                count = space_available;
            }

            int offset = m_test_data.size();
            bool copy_existing = offset && random_bool(80);
            if (copy_existing)
            {
                if (offset > 32*1024)
                {
                    offset = 32*1024;
                }

                offset = random_int(offset+1);

                //  This is safe because the vector has been reserved big enough
                const char* from = m_test_data.data() + data_size() - offset;
                m_test_data.insert(m_test_data.end(), from, from + count);
                return true;
            }

            while (count--)
            {
                m_test_data.push_back(xorshift32() & 0xff);
            }

            return true;
        }

        return test_done();
    }


    bool DeflateTester::random_bytes_fill()
    {
        begin_test("Random bytes test");

        int space_available = m_max_size - data_size();
        if (space_available > 0)
        {
            int count = random_int(200)+1;
            if (count > space_available)
            {
                count = space_available;
            }

            while (count--)
            {
                m_test_data.push_back(xorshift32() & 0xff);
            }

            return true;
        }

        return test_done();
    }


    bool DeflateTester::repeated_sequences_fill()
    {
        begin_test("Repeated sequences test");

        constexpr int prefix_length = 4*255;
        if (m_test_data.empty())
        {
            for (int ch = 1; ch < 256; ++ch)
            {
                m_test_data.push_back(ch);
                m_test_data.push_back(ch);
                m_test_data.push_back(ch);
                m_test_data.push_back(ch);
            }

            int count = max_distance - prefix_length;
            while (count--)
            {
                m_test_data.push_back(0);
            }

            m_generator_state = data_size();
        }

        int space_available = m_max_size - data_size();
        if (space_available > 1)
        {
            m_test_data.resize(m_generator_state);
            ++m_generator_state;

            m_test_data.push_back(0);

            while ((space_available = m_max_size - data_size()) > max_count)
            {
                //  This is safe because the vector has been reserved big enough
                const char* from = m_test_data.data() + data_size() - max_distance;
                m_test_data.insert(m_test_data.end(), from, from + max_count);
            }

            return true;
        }

        return test_done();
    }


    bool DeflateTester::random_repeats_fill()
    {
        begin_test("Random repeats test");

        int space_available = m_max_size - data_size();
        if (space_available > 1)
        {
            int count = random_int(260)+1;
            if (count > space_available)
            {
                count = space_available;
            }

            m_test_data.insert(m_test_data.end(), count, '!' + random_int(60));
            return true;
        }

        return test_done();
    }


    bool DeflateTester::special_cases_fill()
    {
        static const char text0[] =
            "abcdefgABCDEFGhijklmnHIJKLMN1234567"
            "ABCDEFGabcdefgHIJKLMNhijklmn1234567"
            "hijklmnABCDEFG1234567HIJKLMNabcdefg";

        begin_test("Special cases test");
        if (m_test_data.empty())
        {
            m_generator_state = 0;
        }

        switch (m_generator_state)
        {
        case 0:
            m_test_data.assign(text0, text0 + sizeof text0);
            ++m_generator_state;
            return true;

        default:
            break;
        }


        return test_done();
    }


    bool run_tests()
    {
        DeflateTester tester(80*1024);

        tester.compare_performance(75*1024);

        bool result = tester.run_all_tests();
        if (result)
        {
            std::cout << "All tests OK\n";
        }

         return result;
    }

}


int main()
{
    bool ok = run_tests();

    return ok ? 0 : 1;
}

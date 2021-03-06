
/* Copyright (c) 2010-2016, Stefan Eilemann <eile@equalizergraphics.com>
 *                          Juan Hernando <jhernando@fi.upm.es>
 *
 * This file is part of Servus <https://github.com/HBPVIS/Servus>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// Tests the functionality of universally unique identifiers and 128 bit ints

#define BOOST_TEST_MODULE servus_uint128_t
#include <boost/test/unit_test.hpp>

#include <boost/functional/hash.hpp>
#include <boost/unordered_map.hpp>
#include <iostream>
#include <servus/uint128_t.h>

#include <mutex>
#include <random>
#include <thread>
const size_t N_THREADS = 10;

const size_t N_UUIDS = 10000;

typedef boost::unordered_map<servus::uint128_t, bool> TestHash;
namespace boost
{
template <>
struct hash<servus::uint128_t>
{
    std::size_t operator()(const servus::uint128_t& in) const
    {
        hash<uint64_t> forward;
        return forward(in.high() ^ in.low());
    }
};
}

void testConvertUint128ToUUID();
void testIncrement();

BOOST_AUTO_TEST_CASE(basic)
{
    servus::uint128_t id1 = servus::make_UUID();
    servus::uint128_t id2;

    BOOST_CHECK(id1 != servus::uint128_t());
    BOOST_CHECK(id1 != id2);
    BOOST_CHECK(id1.isUUID());
    BOOST_CHECK(!id2.isUUID());

    id2 = servus::make_UUID();
    BOOST_CHECK(id1 != id2);
    BOOST_CHECK(id2.isUUID());

    id1 = id2;
    BOOST_CHECK_EQUAL(id1, id2);

    servus::uint128_t* id3 = new servus::uint128_t(id1);
    servus::uint128_t* id4 = new servus::uint128_t(servus::make_UUID());

    BOOST_CHECK_EQUAL(id1, *id3);
    BOOST_CHECK(*id4 != *id3);

    *id4 = *id3;
    BOOST_CHECK_EQUAL(*id4, *id3);

    delete id3;
    delete id4;

    servus::uint128_t id5, id6;
    BOOST_CHECK_EQUAL(id5, servus::uint128_t());
    BOOST_CHECK_EQUAL(id5, id6);

    const servus::uint128_t& empty = servus::make_uint128("");
    const servus::uint128_t& fox =
        servus::make_uint128("The quick brown fox jumps over the lazy dog.");
    // Values from http://en.wikipedia.org/wiki/MD5#MD5_hashes
    BOOST_CHECK(empty != fox);
    BOOST_CHECK_EQUAL(empty, servus::uint128_t(0xD41D8CD98F00B204ull,
                                               0xE9800998ECF8427Eull));

    BOOST_CHECK_EQUAL(fox, servus::uint128_t(0xE4D909C290D0FB1Cull,
                                             0xA068FFADDF22CBD0ull));

    const servus::uint128_t& stringFox = servus::make_uint128(
        std::string("The quick brown fox jumps over the lazy dog."));
    BOOST_CHECK_EQUAL(fox, stringFox);

    const servus::uint128_t random = servus::make_UUID();
    const uint16_t high = random.high();
    const int32_t low = random.low();
    id6 = servus::uint128_t(high, low);
    BOOST_CHECK_EQUAL(id6.high(), high);
    BOOST_CHECK_EQUAL(id6.low(), uint64_t(low));

    id6 = servus::uint128_t(low);
    BOOST_CHECK_EQUAL(id6.high(), 0);
    BOOST_CHECK_EQUAL(id6.low(), uint64_t(low));

    id6 = std::string("0xD41D8CD98F00B204");
    BOOST_CHECK_EQUAL(id6.high(), 0);
    BOOST_CHECK_EQUAL(id6.low(), 0xD41D8CD98F00B204ull);

    id6 = std::string("0xD41D8CD98F00B204:0xE9800998ECF8427E");
    BOOST_CHECK_EQUAL(id6.high(), 0xD41D8CD98F00B204ull);
    BOOST_CHECK_EQUAL(id6.low(), 0xE9800998ECF8427Eull);

    id6 = std::string();
    BOOST_CHECK_EQUAL(id6.high(), 0);
    BOOST_CHECK_EQUAL(id6.low(), 0);

    std::cout << id6 << std::endl; // for coverage

    id6 = std::string("0xD41D8CD98F00B204\\0580xE9800998ECF8427E");
    BOOST_CHECK_EQUAL(id6.high(), 0xD41D8CD98F00B204ull);
    BOOST_CHECK_EQUAL(id6.low(), 0xE9800998ECF8427Eull);

    std::cout << id6 << std::endl; // for coverage
}

class Thread
{
public:
    void run()
    {
        size_t i = N_UUIDS;

        while (i--)
        {
            const servus::uint128_t uuid = servus::make_UUID();
            {
                // Boost.Test is not thread safe
                std::unique_lock<std::mutex> lock(_mutex);
                BOOST_CHECK(uuid.isUUID());
                BOOST_CHECK(hash.find(uuid) == hash.end());
            }
            hash[uuid] = true;
        }
    }

    TestHash hash;

private:
    static std::mutex _mutex;
};
std::mutex Thread::_mutex;

BOOST_AUTO_TEST_CASE(concurrent)
{
    Thread maps[N_THREADS];
    std::thread threads[N_THREADS];

    const auto startTime = std::chrono::system_clock::now();

    for (size_t i = 0; i < N_THREADS; ++i)
        threads[i] = std::thread(std::bind(&Thread::run, maps[i]));
    for (size_t i = 0; i < N_THREADS; ++i)
        threads[i].join();

    const std::chrono::duration<double> elapsed =
        std::chrono::system_clock::now() - startTime;

    std::cerr << N_UUIDS * N_THREADS / elapsed.count()
              << " UUID generations and hash ops / ms" << std::endl;

    TestHash& first = maps[0].hash;
    for (size_t i = 1; i < N_THREADS; ++i)
    {
        TestHash& current = maps[i].hash;

        for (TestHash::const_iterator j = current.begin(); j != current.end();
             ++j)
        {
            servus::uint128_t uuid(j->first);
            BOOST_CHECK_EQUAL(uuid, j->first);

            std::ostringstream stream;
            stream << uuid;
            uuid = stream.str();
            BOOST_CHECK_EQUAL(uuid, j->first);

            // BOOST_CHECK_EQUAL fails to compile
            BOOST_CHECK(first.find(uuid) == first.end());
            first[uuid] = true;
        }
    }
}

BOOST_AUTO_TEST_CASE(convert_uint128_to_uuid)
{
    const uint64_t low = 1212;
    const uint64_t high = 2314;

    servus::uint128_t test128(high, low);
    BOOST_CHECK_EQUAL(test128.low(), low);
    BOOST_CHECK_EQUAL(test128.high(), high);

    servus::uint128_t testUUID;
    testUUID = test128;
    const servus::uint128_t compare128 = testUUID;
    BOOST_CHECK_EQUAL(compare128, test128);
}

BOOST_AUTO_TEST_CASE(increment)
{
    servus::uint128_t test128(0, 0);
    ++test128;
    BOOST_CHECK_EQUAL(test128.high(), 0);
    BOOST_CHECK_EQUAL(test128.low(), 1);
    --test128;
    BOOST_CHECK_EQUAL(test128.high(), 0);
    BOOST_CHECK_EQUAL(test128.low(), 0);
    test128 = test128 + 1;
    BOOST_CHECK_EQUAL(test128.high(), 0);
    BOOST_CHECK_EQUAL(test128.low(), 1);
    test128 = test128 - 1;
    BOOST_CHECK_EQUAL(test128.high(), 0);
    BOOST_CHECK_EQUAL(test128.low(), 0);

    test128 = servus::uint128_t(0, std::numeric_limits<uint64_t>::max());
    ++test128;
    BOOST_CHECK_EQUAL(test128.high(), 1);
    BOOST_CHECK_EQUAL(test128.low(), 0);
    --test128;
    BOOST_CHECK_EQUAL(test128.high(), 0);
    BOOST_CHECK_EQUAL(test128.low(), std::numeric_limits<uint64_t>::max());
    test128 = test128 + 1;
    BOOST_CHECK_EQUAL(test128.high(), 1);
    BOOST_CHECK_EQUAL(test128.low(), 0);
    test128 = test128 - 1;
    BOOST_CHECK_EQUAL(test128.high(), 0);
    BOOST_CHECK_EQUAL(test128.low(), std::numeric_limits<uint64_t>::max());
}

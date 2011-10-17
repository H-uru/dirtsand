/******************************************************************************
 * This file is part of dirtsand.                                             *
 *                                                                            *
 * dirtsand is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation, either version 3 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * dirtsand is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.          *
 ******************************************************************************/

#ifndef _DS_BITVECTOR_H
#define _DS_BITVECTOR_H

#include "streams.h"

namespace DS
{
    class BitVector
    {
    public:
        BitVector() : m_bits(0), m_words(0) { }

        BitVector(const BitVector& copy) : m_words(copy.m_words)
        {
            m_bits = new uint32_t[m_words];
            memcpy(m_bits, copy.m_bits, m_words);
        }

        BitVector(const BitVector&& move)
            : m_bits(move.m_bits), m_words(move.m_words) { }

        ~BitVector() { delete[] m_bits; }

        bool get(size_t idx) const
        {
            return (m_words < (idx / 32)) ? false
                 : (m_bits[idx / 32] & (1 << (idx % 32))) != 0;
        }

        void set(size_t idx, bool bit);

        BitVector& operator=(const BitVector& copy)
        {
            delete[] m_bits;
            m_words = copy.m_words;
            m_bits = new uint32_t[m_words];
            memcpy(m_bits, copy.m_bits, m_words);
            return *this;
        }

        BitVector& operator=(const BitVector&& move)
        {
            delete[] m_bits;
            m_bits = move.m_bits;
            m_words = move.m_words;
            return *this;
        }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream);

    private:
        uint32_t* m_bits;
        size_t m_words;
    };
}

#endif

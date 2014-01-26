/******************************************************************************
 * This file is part of dirtsand.                                             *
 *                                                                            *
 * dirtsand is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU Affero General Public License as             *
 * published by the Free Software Foundation, either version 3 of the         *
 * License, or (at your option) any later version.                            *
 *                                                                            *
 * dirtsand is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU Affero General Public License for more details.                        *
 *                                                                            *
 * You should have received a copy of the GNU Affero General Public License   *
 * along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.          *
 ******************************************************************************/

#include "BitVector.h"

void DS::BitVector::set(size_t idx, bool bit)
{
    if (m_words < (idx / 32) + 1) {
        size_t moreWords = (idx / 32) + 1;
        uint32_t* moreBits = new uint32_t[moreWords];
        memset(moreBits, 0, moreWords * sizeof(uint32_t));
        memcpy(moreBits, m_bits, m_words * sizeof(uint32_t));
        delete[] m_bits;
        m_bits = moreBits;
        m_words = moreWords;
    }

    if (bit)
        m_bits[idx / 32] |=  (1 << (idx % 32));
    else
        m_bits[idx / 32] &= ~(1 << (idx % 32));
}

void DS::BitVector::read(DS::Stream* stream)
{
    delete[] m_bits;
    m_words = stream->read<uint32_t>();
    m_bits = m_words ? new uint32_t[m_words] : nullptr;
    stream->readBytes(m_bits, m_words * sizeof(uint32_t));
}

void DS::BitVector::write(DS::Stream* stream) const
{
    stream->write<uint32_t>(m_words);
    stream->writeBytes(m_bits, m_words * sizeof(uint32_t));
}

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

#ifndef _DS_MATH_H
#define _DS_MATH_H

namespace DS
{
    class Stream;

    struct Vector3
    {
        float m_X, m_Y, m_Z;

        Vector3() noexcept : m_X(), m_Y(), m_Z() { }

        bool operator==(const Vector3& other) const noexcept
        {
            return m_X == other.m_X && m_Y == other.m_Y && m_Z == other.m_Z;
        }

        bool operator!=(const Vector3& other) const noexcept
        {
            return !operator==(other);
        }
    };

    struct Quaternion
    {
        float m_X, m_Y, m_Z, m_W;

        Quaternion() noexcept : m_X(), m_Y(), m_Z(), m_W() { }

        bool operator==(const Quaternion& other) const noexcept
        {
            return m_X == other.m_X && m_Y == other.m_Y && m_Z == other.m_Z
                && m_W == other.m_W;
        }

        bool operator!=(const Quaternion& other) const noexcept
        {
            return !operator==(other);
        }
    };

    struct Matrix44
    {
        bool m_identity;
        float m_map[4][4];

        Matrix44() noexcept { reset(); }

        bool operator==(const Matrix44& other) const noexcept;
        bool operator!=(const Matrix44& other) const noexcept { return !operator==(other); }

        void reset() noexcept;

        void read(DS::Stream* stream);
        void write(DS::Stream* stream) const;
    };
}

#endif

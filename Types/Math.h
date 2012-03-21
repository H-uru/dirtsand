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
    struct Vector3
    {
        float m_X, m_Y, m_Z;

        bool operator==(const Vector3& other) const
        {
            return m_X == other.m_X && m_Y == other.m_Y && m_Z == other.m_Z;
        }
        bool operator!=(const Vector3& other) const { return !operator==(other); }

    };

    struct Quaternion
    {
        float m_X, m_Y, m_Z, m_W;

        bool operator==(const Quaternion& other) const
        {
            return m_X == other.m_X && m_Y == other.m_Y && m_Z == other.m_Z
                && m_W == other.m_W;
        }
        bool operator!=(const Quaternion& other) const { return !operator==(other); }
    };
}

#endif

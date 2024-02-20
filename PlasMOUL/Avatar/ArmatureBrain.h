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

#ifndef _MOUL_ARMATUREBRAIN_H
#define _MOUL_ARMATUREBRAIN_H

#include "creatable.h"

namespace MOUL
{
    class ArmatureBrain : public Creatable
    {
    public:
        void read(DS::Stream* stream) override;
        void write(DS::Stream* stream) const override;

        virtual bool makeSafeForNet() { return true; }

    protected:
        ArmatureBrain(uint16_t type) : Creatable(type) { }
    };

    class AvBrainHuman : public ArmatureBrain
    {
        FACTORY_CREATABLE(AvBrainHuman)

        bool m_isCustomAvatar;

        void read(DS::Stream* stream) override;
        void write(DS::Stream* stream) const override;

    protected:
        AvBrainHuman(uint16_t type) : ArmatureBrain(type), m_isCustomAvatar() { }
    };

    class AvBrainClimb : public ArmatureBrain
    {
        FACTORY_CREATABLE(AvBrainClimb)

    protected:
        AvBrainClimb(uint16_t type) : ArmatureBrain(type) { }
    };

    class AvBrainCritter : public ArmatureBrain
    {
        FACTORY_CREATABLE(AvBrainCritter)

    protected:
        AvBrainCritter(uint16_t type) : ArmatureBrain(type) { }
    };

    class AvBrainDrive : public ArmatureBrain
    {
        FACTORY_CREATABLE(AvBrainDrive)

    protected:
        AvBrainDrive(uint16_t type) : ArmatureBrain(type) { }
    };

    class AvBrainRideAnimatedPhysical : public ArmatureBrain
    {
        FACTORY_CREATABLE(AvBrainRideAnimatedPhysical)

    protected:
        AvBrainRideAnimatedPhysical(uint16_t type) : ArmatureBrain(type) { }
    };

    class AvBrainSwim : public ArmatureBrain
    {
        FACTORY_CREATABLE(AvBrainSwim)

    protected:
        AvBrainSwim(uint16_t type) : ArmatureBrain(type) { }
    };
}

#endif

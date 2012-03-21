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

#ifndef _MOUL_KIMESSAGE_H
#define _MOUL_KIMESSAGE_H

#include "Message.h"

namespace MOUL
{
    class KIMessage : public Message
    {
        FACTORY_CREATABLE(KIMessage)

        enum Command
        {
            e_ChatMessage, e_EnterChatMode, e_SetChatFadeDelay,
            e_SetTextChatAdminMode, e_DisableKIAndBB, e_EnableKIAndBB,
            e_YesNoDialog, e_AddPlayerDevice, e_RemovePlayerDevice,
            e_UpgradeKILevel, e_DowngradeKILevel, e_RateIt,
            e_SetPrivateChatChannel, e_UnsetPrivateChatChannel, e_StartBookAlert,
            e_MiniBigKIToggle, e_KIPutAway, e_ChatAreaPageUp, e_ChatArePageDown,
            e_ChatAreaGoToBegin, e_ChatAreaGoToEnd, e_KITakePicture,
            e_KICreateJournalNote, e_KIToggleFade, e_KIToggleFadeEnable,
            e_KIChatStatusMsg, e_KILocalChatStatusMsg, e_KIUpSizeFont,
            e_KIDownSizeFont, e_KIOpenYeeshaBook, e_KIOpenKI, e_KIShowCCRHelp,
            e_KICreateMarker, e_KICreateMarkerFolder, e_KILocalChatErrorMsg,
            e_KIPhasedAllOn, e_KIPhasedAllOff, e_KIOKDialog, e_DisableYeeshaBook,
            e_EnableYeeshaBook, e_QuitDialog, e_TempDisableKIAndBB,
            e_TempEnableKIAndBB, e_DisableEntireYeeshaBook,
            e_EnableEntireYeeshaBook, e_KIOKDialogNoQuit, e_GZUpdated,
            e_GZInRange, e_GZOutRange, e_UpgradeKIMarkerLevel, e_KIShowMiniKI,
            e_GZFlashUpdate, e_StartJournalAlert, e_AddJournalBook,
            e_RemoveJournalBook, e_KIOpenJournalBook, e_MGStartCGZGame,
            e_MGStopCGZGame, e_KICreateMarkerNode, e_StartKIAlert,
            e_UpdatePelletScore, e_FriendInviteSent, e_RegisterImager,
            e_NoCommand
        };

        enum Flags
        {
            e_PrivateMsg    = (1<<0),
            e_AdminMsg      = (1<<1),
            e_Dead          = (1<<2),
            e_StatusMsg     = (1<<4),
            e_NeighborMsg   = (1<<5),
            e_ChannelMask   = 0xFF00,
        };

        uint8_t m_command;
        uint32_t m_flags, m_playerId;
        DS::String m_user, m_string;
        float m_delay;
        int32_t m_value;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

        virtual bool makeSafeForNet();

    protected:
        KIMessage(uint16_t type) : Message(type) { }
    };
}

#endif

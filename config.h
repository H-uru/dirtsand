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

#ifndef _DS_CONFIG_H
#define _DS_CONFIG_H

#include <cstdlib>
#include <stdint.h>

typedef unsigned char   chr8_t;
typedef unsigned short  chr16_t;

namespace DS
{
    enum NetResultCode
    {
        e_NetPending = -1,
        e_NetSuccess = 0, e_NetInternalError, e_NetTimeout, e_NetBadServerData,
        e_NetAgeNotFound, e_NetConnectFailed, e_NetDisconnected,
        e_NetFileNotFound, e_NetOldBuildId, e_NetRemoteShutdown,
        e_NetTimeoutOdbc, e_NetAccountAlreadyExists, e_NetPlayerAlreadyExists,
        e_NetAccountNotFound, e_NetPlayerNotFound, e_NetInvalidParameter,
        e_NetNameLookupFailed, e_NetLoggedInElsewhere, e_NetVaultNodeNotFound,
        e_NetMaxPlayersOnAcct, e_NetAuthenticationFailed,
        e_NetStateObjectNotFound, e_NetLoginDenied, e_NetCircularReference,
        e_NetAccountNotActivated, e_NetKeyAlreadyUsed, e_NetKeyNotFound,
        e_NetActivationCodeNotFound, e_NetPlayerNameInvalid,
        e_NetNotSupported, e_NetServiceForbidden, e_NetAuthTokenTooOld,
        e_NetMustUseGameTapClient, e_NetTooManyFailedLogins,
        e_NetGameTapConnectionFailed, e_NetGTTooManyAuthOptions,
        e_NetGTMissingParameter, e_NetGTServerError, e_NetAccountBanned,
        e_NetKickedByCCR, e_NetScoreWrongType, e_NetScoreNotEnoughPoints,
        e_NetScoreAlreadyExists, e_NetScoreNoDataFound,
        e_NetInviteNoMatchingPlayer, e_NetInviteTooManyHoods, e_NetNeedToPay,
        e_NetServerBusy
    };
}

#endif

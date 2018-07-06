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

#include "Types/Uuid.h"
#include <libpq-fe.h>

namespace DS
{
    template <size_t count>
    class PostgresStrings
    {
    public:
        const char* m_values[count];
        ST::string m_strings[count];

        void set(size_t idx, const ST::string& str)
        {
            m_strings[idx] = str;
            _cache(idx);
        }

        void set(size_t idx, uint32_t value)
        {
            m_strings[idx] = ST::string::from_uint(value);
            _cache(idx);
        }

        void set(size_t idx, int value)
        {
            m_strings[idx] = ST::string::from_int(value);
            _cache(idx);
        }

        template <typename... ArgsT>
        void set_all(ArgsT&&... args)
        {
            _set_many(0, std::forward<ArgsT>(args)...);
        }

    private:
        void _set_many(size_t idx) { }

        template <typename ArgN, typename... ArgsT>
        void _set_many(size_t idx, ArgN&& arg, ArgsT&&... args)
        {
            set(idx, std::forward<ArgN>(arg));
            _set_many(idx + 1, std::forward<ArgsT>(args)...);
        }

        void _cache(size_t idx)
        {
            m_values[idx] = m_strings[idx].c_str();
        }
    };

    class PGresultRef
    {
    public:
        constexpr PGresultRef() noexcept : m_result() { }
        constexpr PGresultRef(PGresult* result) noexcept : m_result(result) { }

        void reset(PGresult* result = nullptr) noexcept
        {
            if (m_result && m_result != result)
                PQclear(m_result);
            m_result = result;
        }

        ~PGresultRef() noexcept { reset(); }

        PGresultRef& operator=(PGresult* result) noexcept
        {
            reset(result);
            return *this;
        }

        operator PGresult*() noexcept { return m_result; }
        operator const PGresult*() const noexcept { return m_result; }

        PGresultRef(const PGresultRef&) = delete;
        PGresultRef& operator=(const PGresultRef&) = delete;

        PGresultRef(PGresultRef&& move) noexcept
            : m_result(move.m_result) { move.m_result = nullptr; }

        PGresultRef& operator=(PGresultRef&& move) noexcept
        {
            reset(move.m_result);
            move.m_result = nullptr;
            return *this;
        }

    private:
        PGresult* m_result;
    };

    template <typename... ArgsT>
    PGresultRef PQexecVA(PGconn* conn, const char* command, ArgsT&&... args)
    {
        PostgresStrings<sizeof...(args)> params;
        params.set_all(std::forward<ArgsT>(args)...);
        return PQexecParams(conn, command, sizeof...(args), nullptr,
                            params.m_values, nullptr, nullptr, 0);
    }
}

static inline void check_postgres(PGconn* postgres)
{
    auto status = PQstatus(postgres);
    if (status == CONNECTION_BAD) {
        PQreset(postgres);
        status = PQstatus(postgres);
    }
    if (status != CONNECTION_OK) {
        fprintf(stderr, "WARNING: Failed to reset postgres session.  Status = %d\n",
                static_cast<int>(status));
        fputs("The next postgres transaction is likely to fail...\n", stderr);
    }
}

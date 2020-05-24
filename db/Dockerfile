# This file is part of dirtsand.
#
# dirtsand is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# dirtsand is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.

FROM postgres:12

# Copy DS Schema into the container for auto-init... We concat it so there is no ambiguity about
# the order of database initialization.
COPY *.sql /usr/src/dirtsand/
RUN touch /docker-entrypoint-initdb.d/init.sql && \
    echo "CREATE EXTENSION \"uuid-ossp\";" >> /docker-entrypoint-initdb.d/init.sql && \
    cat /usr/src/dirtsand/dbinit.sql >> /docker-entrypoint-initdb.d/init.sql && \
    cat /usr/src/dirtsand/functions.sql >> /docker-entrypoint-initdb.d/init.sql && \
    rm -r /usr/src/dirtsand

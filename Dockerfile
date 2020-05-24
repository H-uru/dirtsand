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

FROM alpine

# Install build dependencies
RUN apk add build-base cmake git openssl-dev postgresql-dev readline-dev zlib-dev && \
    apk add bash openssl postgresql-client readline zlib

# Build string_theory
WORKDIR /usr/src
RUN git clone --depth 1 https://github.com/zrax/string_theory.git && \
    mkdir -p string_theory/build && cd string_theory/build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DST_BUILD_TESTS=OFF .. && \
    make && make install && \
    cd /usr/src && rm -r string_theory

# Build dirtsand
COPY . /usr/src/dirtsand
RUN mkdir -p dirtsand/build && cd dirtsand/build && \
    cmake -DCMAKE_INSTALL_PREFIX=/opt/dirtsand -DCMAKE_BUILD_TYPE=Release .. && \
    make && make install && \
    cd /usr/src && rm -r dirtsand && \
    mkdir -p /opt/dirtsand/etc

# Hold a copy of the latest age and SDL files in case the user does not populate the mount point
RUN git clone --depth 1 https://github.com/H-uru/Plasma.git && \
    mkdir -p /opt/dirtsand/lib/moul-scripts/SDL && mkdir -p /opt/dirtsand/lib/moul-scripts/dat && \
    cp Plasma/Scripts/SDL/*.sdl /opt/dirtsand/lib/moul-scripts/SDL && \
    cp Plasma/Scripts/dat/*.age /opt/dirtsand/lib/moul-scripts/dat && \
    rm -r Plasma

# Cleanup image
RUN apk del cmake gcc git openssl-dev postgresql-dev readline-dev zlib-dev

# Add user for dirtsand
RUN adduser --disabled-password --no-create-home dirtsand && \
    chown -R dirtsand /opt/dirtsand
USER dirtsand
WORKDIR /opt/dirtsand

EXPOSE 14617/tcp
EXPOSE 8080/tcp
ENTRYPOINT /opt/dirtsand/bin/docker-entrypoint.sh

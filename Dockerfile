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

COPY . /usr/src/dirtsand
WORKDIR /usr/src

ARG PRODUCT_BRANCH_ID=1
ARG PRODUCT_BUILD_ID=918
ARG PRODUCT_BUILD_TYPE=50
ARG PRODUCT_UUID=ea489821-6c35-4bd0-9dae-bb17c585e680
ARG DS_HOOD_USER_NAME=DS
ARG DS_HOOD_INST_NAME=Neighborhood
ARG DS_HOOD_POP_THRESHOLD=20

# Everything coalesced into this single RUN statement to avoid creating WIP images.
RUN \
    apk add build-base cmake git openssl-dev postgresql-dev readline-dev zlib-dev && \
    apk add bash libstdc++ openssl postgresql-client readline zlib && \
    \
    git clone --depth 1 https://github.com/zrax/string_theory.git && \
    cmake -DCMAKE_BUILD_TYPE=Release -DST_BUILD_TESTS=OFF -B string_theory/build -S string_theory && \
    cmake --build string_theory/build --parallel && cmake --build string_theory/build --target install && \
    rm -r string_theory && \
    \
    mkdir -p /opt/dirtsand/db && cp dirtsand/db/*.sql /opt/dirtsand/db && \
    cmake -DCMAKE_INSTALL_PREFIX=/opt/dirtsand -DCMAKE_BUILD_TYPE=Release \
        -DPRODUCT_BRANCH_ID=${PRODUCT_BRANCH_ID} -DPRODUCT_BUILD_ID=${PRODUCT_BUILD_ID} \
        -DPRODUCT_BUILD_TYPE=${PRODUCT_BUILD_TYPE} -DPRODUCT_UUID=${PRODUCT_UUID} \
        -DDS_HOOD_USER_NAME=${DS_HOOD_USER_NAME} -DDS_HOOD_INST_NAME=${DS_HOOD_INST_NAME}} \
        -DDS_HOOD_POP_THRESHOLD=${DS_HOOD_POP_THRESHOLD} \
        -B dirtsand/build -S dirtsand && \
    cmake --build dirtsand/build --parallel && cmake --build dirtsand/build --target install && \
    cd /usr/src && rm -r dirtsand && \
    mkdir -p /opt/dirtsand/etc && \
    \
    git clone --depth 1 https://github.com/H-uru/Plasma.git && \
    mkdir -p /opt/dirtsand/lib/moul-scripts/SDL && mkdir -p /opt/dirtsand/lib/moul-scripts/dat && \
    cp Plasma/Scripts/SDL/*.sdl /opt/dirtsand/lib/moul-scripts/SDL && \
    cp Plasma/Scripts/dat/*.age /opt/dirtsand/lib/moul-scripts/dat && \
    rm -r Plasma && \
    \
    apk del cmake build-base git openssl-dev postgresql-dev readline-dev zlib-dev && \
    \
    adduser --disabled-password --no-create-home dirtsand && \
    chown -R dirtsand /opt/dirtsand

# Add user for dirtsand
USER dirtsand
WORKDIR /opt/dirtsand

EXPOSE 14617/tcp
EXPOSE 8080/tcp
ENTRYPOINT /opt/dirtsand/bin/docker-entrypoint.sh

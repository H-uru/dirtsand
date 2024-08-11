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

ARG BUILD_TYPE=release

FROM alpine:latest AS run_env
RUN \
    apk add bash libstdc++ openssl postgresql-client readline zlib && \
    \
    adduser --disabled-password --no-create-home dirtsand && \
    mkdir -p /opt/dirtsand && chown -R dirtsand /opt/dirtsand

FROM run_env AS build_env_debug
ENV BUILD_TYPE=Debug
ENV CFLAGS=-ggdb
ENV CXXFLAGS=-ggdb

FROM run_env AS build_env_release
ENV BUILD_TYPE=Release

FROM build_env_${BUILD_TYPE} AS build_env
RUN \
    apk add build-base cmake git openssl-dev postgresql-dev readline-dev zlib-dev && \
    \
    mkdir -p /usr/src && cd /usr/src && \
    git clone --depth 1 https://github.com/zrax/string_theory.git && \
    cmake -DCMAKE_BUILD_TYPE=Release -DST_BUILD_TESTS=OFF -B string_theory/build -S string_theory && \
    cmake --build string_theory/build --parallel && cmake --build string_theory/build --target install && \
    \
    git clone --depth 1 https://github.com/H-uru/Plasma.git

FROM build_env AS builder
COPY . /usr/src/dirtsand
WORKDIR /usr/src

ARG PRODUCT_BRANCH_ID=1
ARG PRODUCT_BUILD_ID=918
ARG PRODUCT_BUILD_TYPE=50
ARG PRODUCT_UUID=ea489821-6c35-4bd0-9dae-bb17c585e680
ARG DS_HOOD_USER_NAME=DS
ARG DS_HOOD_INST_NAME=Neighborhood
ARG DS_HOOD_POP_THRESHOLD=20
ARG DS_OU_COMPATIBLE=ON

RUN \
    mkdir -p /opt/dirtsand/db && cp dirtsand/db/*.sql /opt/dirtsand/db && \
    cmake -DCMAKE_INSTALL_PREFIX=/opt/dirtsand -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DPRODUCT_BRANCH_ID=${PRODUCT_BRANCH_ID} -DPRODUCT_BUILD_ID=${PRODUCT_BUILD_ID} \
        -DPRODUCT_BUILD_TYPE=${PRODUCT_BUILD_TYPE} -DPRODUCT_UUID=${PRODUCT_UUID} \
        -DDS_HOOD_USER_NAME=${DS_HOOD_USER_NAME} -DDS_HOOD_INST_NAME=${DS_HOOD_INST_NAME} \
        -DDS_HOOD_POP_THRESHOLD=${DS_HOOD_POP_THRESHOLD} -DDS_OU_COMPATIBLE=${DS_OU_COMPATIBLE} \
        -DENABLE_TESTS=OFF -B dirtsand/build -S dirtsand && \
    cmake --build dirtsand/build --parallel && cmake --build dirtsand/build --target install && \
    mkdir -p /opt/dirtsand/etc && \
    \
    mkdir -p /opt/dirtsand/lib/moul-scripts/SDL && mkdir -p /opt/dirtsand/lib/moul-scripts/dat && \
    cp /usr/src/Plasma/Scripts/SDL/*.sdl /opt/dirtsand/lib/moul-scripts/SDL && \
    cp /usr/src/Plasma/Scripts/dat/*.age /opt/dirtsand/lib/moul-scripts/dat && \
    \
    chown -R dirtsand /opt/dirtsand

FROM run_env AS run_env_debug
RUN apk add gdb musl-dbg
COPY . /usr/src/dirtsand

FROM run_env AS run_env_release

FROM run_env_${BUILD_TYPE} AS production
COPY --from=builder /opt/dirtsand /opt/dirtsand

USER dirtsand
WORKDIR /opt/dirtsand

EXPOSE 14617/tcp
EXPOSE 8080/tcp
ENTRYPOINT /opt/dirtsand/bin/docker-entrypoint.sh

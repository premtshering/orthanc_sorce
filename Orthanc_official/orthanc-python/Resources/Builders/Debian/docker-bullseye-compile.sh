#!/bin/bash

set -ex

if [ "$1" != "Debug" -a "$1" != "Release" ]; then
    echo "Please provide build type: Debug or Release"
    exit -1
fi

if [ -t 1 ]; then
    # TTY is available => use interactive mode
    DOCKER_FLAGS='-i'
fi

ROOT_DIR=`dirname $(readlink -f $0)`/../../..

mkdir -p ${ROOT_DIR}/docker-build-bullseye/

docker pull debian:bullseye-slim

docker run -t ${DOCKER_FLAGS} --rm \
    -v ${ROOT_DIR}:/source:ro \
    -v ${ROOT_DIR}/docker-build-bullseye:/target:rw \
    debian:bullseye-slim \
    bash /source/Resources/Builders/Debian/docker-internal.sh $1 3.9 $(id -u) $(id -g)

ls -lR ${ROOT_DIR}/docker-build-bullseye/

#
# Copyright 2018 Capitar IT Group BV <info@capitar.com>
# Copyright 2018 Staysail Systems, Inc. <info@staysail.tech>
#
# This software is supplied under the terms of the MIT License, a
# copy of which should be located in the distribution where this
# file was obtained (LICENSE.txt).  A copy of the license may also be
# found online at https://opensource.org/licenses/MIT.

cmake_minimum_required (VERSION 2.8.7)

project(nuttx)

find_package(nng CONFIG REQUIRED)

add_executable(reqrep reqrep.c)
add_executable(pubsub pubsub.c)
add_executable(media media.c)
target_link_libraries(reqrep nng::nng)
target_link_libraries(pubsub nng::nng)
target_link_libraries(media nng::nng)

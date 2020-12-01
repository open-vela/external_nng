#
# Copyright 2019 Staysail Systems, Inc. <info@staysail.tech>
# Copyright 2018 Capitar IT Group BV <info@capitar.com>
#
# This software is supplied under the terms of the MIT License, a
# copy of which should be located in the distribution where this
# file was obtained (LICENSE.txt).  A copy of the license may also be
# found online at https://opensource.org/licenses/MIT.
#

#  Bus protocol
option (NNG_NUTTX "Enable NuttX IPC." ON)
mark_as_advanced(NNG_NUTTX)

nng_sources_if(NNG_NUTTX nxipc.c nxparcel.c)
nng_headers_if(NNG_NUTTX nng/nuttx/nxipc.h nng/nuttx/nxparcel.h)

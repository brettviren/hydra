/*  =========================================================================
    hydra_server - Hydra Server (in C)

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: hydra_server.xml, or
     * The code generation script that built this file: zproto_server_c
    ************************************************************************
    Copyright (c) the Contributors as noted in the AUTHORS file.
    This file is part of zbroker, the ZeroMQ broker project.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef HYDRA_SERVER_H_INCLUDED
#define HYDRA_SERVER_H_INCLUDED

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  To work with hydra_server, use the CZMQ zactor API:
//
//  Create new hydra_server instance, passing logging prefix:
//
//      zactor_t *hydra_server = zactor_new (hydra_server, "myname");
//
//  Destroy hydra_server instance
//
//      zactor_destroy (&hydra_server);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (hydra_server, "VERBOSE");
//
//  Bind hydra_server to specified endpoint. TCP endpoints may specify
//  the port number as "*" to acquire an ephemeral port:
//
//      zstr_sendx (hydra_server, "BIND", endpoint, NULL);
//
//  Return assigned port number, specifically when BIND was done using an
//  an ephemeral port:
//
//      zstr_sendx (hydra_server, "PORT", NULL);
//      char *command, *port_str;
//      zstr_recvx (hydra_server, &command, &port_str, NULL);
//      assert (streq (command, "PORT"));
//
//  Specify configuration file to load, overwriting any previous loaded
//  configuration file or options:
//
//      zstr_sendx (hydra_server, "LOAD", filename, NULL);
//
//  Set configuration path value:
//
//      zstr_sendx (hydra_server, "SET", path, value, NULL);
//
//  Save configuration data to config file on disk:
//
//      zstr_sendx (hydra_server, "SAVE", filename, NULL);
//
//  Send zmsg_t instance to hydra_server:
//
//      zactor_send (hydra_server, &msg);
//
//  Receive zmsg_t instance from hydra_server:
//
//      zmsg_t *msg = zactor_recv (hydra_server);
//
//  This is the hydra_server constructor as a zactor_fn:
//
void
    hydra_server (zsock_t *pipe, void *args);

//  Self test of this class
void
    hydra_server_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif

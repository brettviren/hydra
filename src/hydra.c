/*  =========================================================================
    hydra - main Hydra API

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of zbroker, the ZeroMQ broker project.           
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

/*
@header
    The hydra class provides a public API to the Hydra peer to peer service.
    This service runs as a background thread (an actor), and offers a
    blocking synchronous API intended for single-threaded UI code. To use
    the Hydra actor, you configure it, and then start it, and it runs until
    you destroy the actor. The Hydra service logic is: discover new peer,
    sync posts with peer, disconnect, and repeat forever. If multiple new
    peers appear at the same time, Hydra will synch to and from them all
    simultaneously.
@discuss
    Posts and content are held in a single directory tree, by default .hydra
    under the current working directory. You can override the location when
    calling hydra_new ().
@end
*/

#include "hydra_classes.h"

//  Structure of our class

struct _hydra_t {
    zactor_t *actor;            //  Hydra API uses a background actor
};

//  This is the background actor that implements the Hydra scan/sync logic
static void
    s_self_actor (zsock_t *pipe, void *args);
    
//  --------------------------------------------------------------------------
//  Constructor, creates a new Hydra node. Note that until you start the
//  node it is silent and invisible to other nodes on the network. You may
//  specify the working directory, which defaults to .hydra in the current
//  working directory. Creates the working directory if necessary.

hydra_t *
hydra_new (const char *directory)
{
    if (directory == NULL)
        directory = ".hydra";

    //  Create directory if necessary
    if (zsys_file_mode (directory) == -1)
        zsys_dir_create (directory);

    //  Switch to working directory, or die trying
    if (zsys_dir_change (directory)) {
        zsys_error ("hydra: cannot access %s: %s", directory, strerror (errno));
        return NULL;
    }
    //  Check we are the only process currently running here
    if (zsys_run_as ("hydra.lock", NULL, NULL)) {
        zsys_error ("hydra: cannot get directory lock, exiting");
        return NULL;
    }
    hydra_t *self = (hydra_t *) zmalloc (sizeof (hydra_t));
    if (self)
        self->actor = zactor_new (s_self_actor, (void *) directory);
    if (self->actor == NULL)
        hydra_destroy (&self);
    
    return self;
}


//  --------------------------------------------------------------------------
//  Destructor, destroys a Hydra node. When you destroy a node, any posts
//  it is sending or receiving will be discarded.

void
hydra_destroy (hydra_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        hydra_t *self = *self_p;
        zactor_destroy (&self->actor);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Set node nickname; this is saved persistently in the Hydra configuration
//  file.

void
hydra_set_nickname (hydra_t *self, const char *nickname)
{
    assert (self);
    zsock_send (self->actor, "sss", "SET", "NICKNAME", nickname);
}


//  --------------------------------------------------------------------------
//  Return our node nickname, as previously stored in hydra.cfg, or set by
//  the hydra_set_nickname() method. Caller must free returned string using
//  zstr_free ().

const char *
hydra_nickname (hydra_t *self)
{
    assert (self);
    char *nickname;
    zsock_send (self->actor, "ss", "GET", "NICKNAME");
    zsock_recv (self->actor, "s", &nickname);
    return nickname;
}


//  --------------------------------------------------------------------------
//  Set the trace level to animation of main actors; this is helpful to
//  debug the Hydra protocol flow.

void
hydra_set_animate (hydra_t *self)
{
    assert (self);
    zsock_send (self->actor, "ss", "SET", "ANIMATE");
}


//  --------------------------------------------------------------------------
//  Set the trace level to animation of all actors including those used in
//  security and discovery. Use this to collect diagnostic logs.

void
hydra_set_verbose (hydra_t *self)
{
    assert (self);
    zsock_send (self->actor, "ss", "SET", "VERBOSE");
}


//  --------------------------------------------------------------------------
//  By default, Hydra needs a network interface capable of broadcast UDP
//  traffic, e.g. WiFi or LAN interface. To run nodes on a stand-alone PC,
//  set the local IPC option. The node will then do gossip discovery over
//  IPC. Gossip discovery needs at exactly one node to be running in a
//  directory called ".hydra".

void
hydra_set_local_ipc (hydra_t *self)
{
    assert (self);
    zsock_send (self->actor, "sss", "SET", "LOCAL IPC", "1");
}

//  --------------------------------------------------------------------------
//  By default, Hydra uses your hostname via zsys_hostname ().            
//  Use this function to set some other hostname. Useful when using VMs,  
//  containers, etc.                                                      

void
hydra_set_hostname (hydra_t *self, const char *hostname)
{
    assert (self);
    zsock_send (self->actor, "sss", "SET", "HOSTNAME", hostname);
}

//  --------------------------------------------------------------------------
//  Start node. When you start a node it begins discovery and post exchange.
//  Returns 0 if OK, -1 if it wasn't possible to start the node.

int
hydra_start (hydra_t *self)
{
    assert (self);
    zsock_send (self->actor, "s", "START");
    return zsock_wait (self->actor);
}


//  --------------------------------------------------------------------------
//  Return next available post, if any. Does not block. If there are no posts 
//  waiting, returns NULL. The caller can read the post using the hydra_post
//  API, and must destroy the post when done with it.

hydra_post_t *
hydra_fetch (hydra_t *self)
{
    assert (self);
    hydra_post_t *post;
    zsock_send (self->actor, "s", "FETCH");
    zsock_recv (self->actor, "p", &post);
    return post;
}


//  --------------------------------------------------------------------------
//  Store a new post provided as a null-terminated string. Returns post ID for
//  the newly created post, or NULL if it was impossible to store the post.
//  Caller must free post ID when finished with it.

char *
hydra_store_string (hydra_t *self, const char *subject, const char *parent_id,
                    const char *mime_type, const char *content)
{
    char *post_id;
    zsock_send (self->actor, "ssssss", "POST", subject, parent_id, mime_type,
                "string", content);
    zsock_recv (self->actor, "s", &post_id);
    return post_id;
}


//  --------------------------------------------------------------------------
//  Store a new post located in a file somewhere on disk. Returns post ID for
//  the newly created post, or NULL if it was impossible to store the post.
//  Caller must free post ID when finished with it.

char *
hydra_store_file (hydra_t *self, const char *subject, const char *parent_id,
                  const char *mime_type, const char *filename)
{
    char *post_id;
    zsock_send (self->actor, "ssssss", "POST", subject, parent_id, mime_type,
                "file", filename);
    zsock_recv (self->actor, "s", &post_id);
    return post_id;
}


//  --------------------------------------------------------------------------
//  Store a new post provided as a chunk of data. Returns post ID for
//  the newly created post, or NULL if it was impossible to store the post.
//  Caller must free post ID when finished with it.

char *
hydra_store_chunk (hydra_t *self, const char *subject, const char *parent_id,
                   const char *mime_type, zchunk_t *chunk)
{
    char *post_id;
    zsock_send (self->actor, "ssssss", "POST", subject, parent_id, mime_type,
                "chunk", chunk);
    zsock_recv (self->actor, "s", &post_id);
    return post_id;
}


//  --------------------------------------------------------------------------
//  Return the Hydra version for run-time API detection

void
hydra_version (int *major, int *minor, int *patch)
{
    *major = HYDRA_VERSION_MAJOR;
    *minor = HYDRA_VERSION_MINOR;
    *patch = HYDRA_VERSION_PATCH;
}


//  --------------------------------------------------------------------------
//  The self_t structure holds the state for one actor instance

typedef struct {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    zactor_t *server;           //  Hydra server instance
    zyre_t *zyre;               //  Zyre discovery service
    zhashx_t *peers;            //  All peers we are talking to
    zlistx_t *posts;            //  Posts to be delivered to API
    char *directory;            //  Working directory
    bool started;               //  Are we already running?
    bool terminated;            //  Did caller ask us to quit?
    bool local_ipc;             //  Use local IPC discovery
    char *hostname;             //  Custom hostname
    int status;                 //  Last status from any client
    char *reason;               //  Last error reason from any client
} self_t;

static self_t *
s_self_new (zsock_t *pipe, char *directory)
{
    self_t *self = (self_t *) zmalloc (sizeof (self_t));
    self->pipe = pipe;
    self->poller = zpoller_new (self->pipe, NULL);
    self->directory = strdup (directory);
    
    //  We use Zyre for discovery and presence, and the Hydra protocol
    //  for content exchange. We start Zyre and the Hydra server now,
    //  give the caller opportunity to configure then, and then start.
    self->zyre = zyre_new (NULL);
    self->server = zactor_new (hydra_server, NULL);
    self->peers = zhashx_new ();
    zhashx_set_destructor (self->peers, (czmq_destructor *) hydra_client_destroy);
    self->posts = zlistx_new ();
    zlistx_set_destructor (self->posts, (czmq_destructor *) hydra_post_destroy);
    zsock_send (self->server, "ss", "LOAD", "hydra.cfg");
    zsock_send (self->server, "sss", "SET", "server/timeout", "2000");
    return self;
}

static void
s_self_destroy (self_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        self_t *self = *self_p;
        zpoller_destroy (&self->poller);
        zactor_destroy (&self->server);
        zhashx_destroy (&self->peers);
        zlistx_destroy (&self->posts);
        zyre_destroy (&self->zyre);
        zstr_free (&self->reason);
        free (self->directory);
        free (self);
        *self_p = NULL;
    }
}

static void
s_self_set_property (self_t *self, zmsg_t *request)
{
    char *name = zmsg_popstr (request);
    char *value = zmsg_popstr (request);
    if (streq (name, "NICKNAME")) {
        zsock_send (self->server, "sss", "SET", "/hydra/nickname", value);
        zsock_send (self->server, "ss", "SAVE", "hydra.cfg");
    }
    else
    if (streq (name, "ANIMATE")) {
        zstr_send (self->server, "VERBOSE");
        hydra_client_verbose = true;
    }
    else
    if (streq (name, "VERBOSE"))
        zyre_set_verbose (self->zyre);
    else
    if (streq (name, "LOCAL IPC"))
        self->local_ipc = atoi (value);
    else
    if (streq (name, "HOSTNAME")) {
        self->hostname = strdup(value);
    }
    else {
        zsys_error ("hydra: - invalid SET property: %s", name);
        assert (false);
    }
    free (name);
    free (value);
}

static void
s_self_get_property (self_t *self, zmsg_t *request)
{
    char *name = zmsg_popstr (request);
    if (streq (name, "NICKNAME")) {
        //  Get current nickname from server
        char *nickname;
        zsock_send (self->server, "s", "NICKNAME");
        zsock_recv (self->server, "s", &nickname);
        zsock_send (self->pipe, "s", nickname);
        zstr_free (&nickname);
    }
    else
    if (streq (name, "STATUS"))
        zsock_send (self->pipe, "i", self->status);
    else
    if (streq (name, "REASON"))
        zsock_send (self->pipe, "s", self->reason);
    else {
        zsys_error ("hydra: - invalid GET property: %s", name);
        assert (false);
    }
}

static void
s_self_start (self_t *self)
{
    //  Return code for caller
    int rc = 0;
    assert (!self->started);
    
    //  Set up networking; either over TCP (requires active network interface)
    //  or over IPC, using the Zyre gossip API
    char *endpoint;
    if (self->local_ipc) {
        zyre_set_endpoint (self->zyre, "ipc://@/zyre/%s", self->directory);
        //  When we're using gossip, the hub is the node running in .hydra
        if (streq (self->directory, ".hydra"))
            zyre_gossip_bind (self->zyre, "ipc://@/hydra_gossip_hub");
        else
            zyre_gossip_connect (self->zyre, "ipc://@/hydra_gossip_hub");
        endpoint = zsys_sprintf ("ipc://@/hydra/%s", self->directory);
        zsock_send (self->server, "ss", "BIND", endpoint);
    }
    else {
        //  Bind Hydra server to ephemeral port and get that port number
        int port_nbr;
        zsock_send (self->server, "ss", "BIND", "tcp://*:*");
        zsock_send (self->server, "s", "PORT");
        zsock_recv (self->server, "si", NULL, &port_nbr);

        if (self->hostname)
            endpoint = zsys_sprintf ("tcp://%s:%d", self->hostname, port_nbr);
        else {
            //  Get our hostname via a zbeacon instance, and construct endpoint
            zactor_t *beacon = zactor_new (zbeacon, NULL);
            assert (beacon);
            zsock_send (beacon, "si", "CONFIGURE", 31415);
            char *hostname = zstr_recv (beacon);
            endpoint = zsys_sprintf ("tcp://%s:%d", hostname, port_nbr);
            zstr_free (&hostname);
            zactor_destroy (&beacon);
        }
    }
    zsys_info ("hydra: Hydra server started on %s", endpoint);
    zyre_set_header (self->zyre, "X-HYDRA", "%s", endpoint);
    zstr_free (&endpoint);

    if (zyre_start (self->zyre) == 0) {
        zpoller_add (self->poller, zyre_socket (self->zyre));
        self->started = true;
    }
    else {
        zsys_info ("hydra: can't start Zyre discovery service");
        zactor_destroy (&self->server);
        zyre_destroy (&self->zyre);
        rc = -1;
    }
    zsock_signal (self->pipe, rc);
}


static void
s_self_fetch (self_t *self)
{
    zsock_send (self->pipe, "p", zlistx_detach (self->posts, NULL));
}


static void
s_self_post (self_t *self, zmsg_t *request)
{
    zstr_sendm (self->server, "POST");
    zsock_send (self->server, "m", request);
    char *post_id = zstr_recv (self->server);
    if (post_id) {
        zstr_send (self->pipe, post_id);
        zstr_free (&post_id);
    }
}


//  --------------------------------------------------------------------------
//  Handle a command from calling application

static void
s_self_handle_pipe (self_t *self)
{
    //  Get the whole message off the pipe in one go
    zmsg_t *request = zmsg_recv (self->pipe);
    if (!request)
        return;                     //  Interrupted

    char *command = zmsg_popstr (request);
    if (streq (command, "SET"))
        s_self_set_property (self, request);
    else
    if (streq (command, "GET"))
        s_self_get_property (self, request);
    else
    if (streq (command, "START"))
        s_self_start (self);
    else
    if (streq (command, "FETCH"))
        s_self_fetch (self);
    else
    if (streq (command, "POST"))
        s_self_post (self, request);
    else
    if (streq (command, "$TERM"))
        self->terminated = true;
    else {
        zsys_error ("hydra: - invalid command: %s", command);
        assert (false);
    }
    zstr_free (&command);
    zmsg_destroy (&request);
}


//  --------------------------------------------------------------------------
//  Handle a Zyre event; we maintain a list of peers depending on ENTER and
//  EXIT events, and synchronize with active peers.

static void
s_self_handle_zyre (self_t *self)
{
    zyre_event_t *event = zyre_event_new (self->zyre);
    if (streq(zyre_event_type (event), "ENTER")) {
        //  Only connect to Hydra nodes (not other Zyre nodes)
        const char *endpoint = zyre_event_header (event, "X-HYDRA");
        if (endpoint) {
            hydra_client_t *client = hydra_client_new ();
            assert (client);
            if (hydra_client_connect (client, endpoint, 1000) == 0
            && !zhashx_insert (self->peers, zyre_event_peer_uuid (event), client)) {
                zpoller_add (self->poller, hydra_client_msgpipe (client));
                hydra_client_sync (client);
            }
        }
    }
    else
    if (streq(zyre_event_type (event), "EXIT")) {
        hydra_client_t *client =
            (hydra_client_t *) zhashx_lookup (self->peers, zyre_event_peer_uuid (event));
        if (client) 
            zpoller_remove (self->poller, hydra_client_msgpipe (client));
        hydra_client_destroy (&client);
    }
    zyre_event_destroy (&event);
}


//  --------------------------------------------------------------------------
//  Handle a peer event

static void
s_self_handle_peer (self_t *self, zsock_t *msgpipe)
{
    char *command = zstr_recv (msgpipe);
    if (streq (command, "POST")) {
        hydra_post_t *post;
        zsock_recv (msgpipe, "p", &post);
        assert (post);
        zlistx_add_end (self->posts, post);
    }
    else
    if (streq (command, "SUCCESS"))
        zsock_recv (msgpipe, "i", &self->status);
    else
    if (streq (command, "FAILURE")) {
        zstr_free (&self->reason);
        zsock_recv (msgpipe, "is", &self->status, &self->reason);
        zsys_error ("hydra: failed - %s", self->reason);
    }
    zstr_free (&command);
}


//  --------------------------------------------------------------------------
//  Hydra actor

void
s_self_actor (zsock_t *pipe, void *args)
{
    self_t *self = s_self_new (pipe, (char *) args);
    if (self) {
        zsock_signal (pipe, 0);
        while (!self->terminated) {
            zsock_t *which = (zsock_t *) zpoller_wait (self->poller, -1);
            if (which == self->pipe)
                s_self_handle_pipe (self);
            else
            if (which == zyre_socket (self->zyre))
                s_self_handle_zyre (self);
            else
            if (zsock_is (which))
                s_self_handle_peer (self, which);
            
            if (zpoller_terminated (self->poller))
                break;          //  Interrupted
        }
    }
    else
        zsock_signal (pipe, -1);    //  Failed to initialize

    s_self_destroy (&self);
}


//  --------------------------------------------------------------------------
//  Self test of this class

void
hydra_test (bool verbose)
{
    printf (" * hydra: ");

    //  @selftest
    //  Simple create/destroy test
    hydra_t *self = hydra_new (NULL);
    assert (self);
    char *post_id = hydra_store_string (self, "This is a string",
                                        "", "text/plain", "Hello, World");
    assert (post_id);
    zstr_free (&post_id);
    
    hydra_post_t *post = hydra_fetch (self);
    assert (post == NULL);
    hydra_destroy (&self);
    //  @end

    printf ("OK\n");
}

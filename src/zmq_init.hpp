/*
    Copyright (c) 2007-2011 iMatix Corporation
    Copyright (c) 2007-2011 Other contributors as noted in the AUTHORS file

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __ZMQ_ZMQ_INIT_HPP_INCLUDED__
#define __ZMQ_ZMQ_INIT_HPP_INCLUDED__

#include <vector>

#include "../include/zmq.h"

#include "i_inout.hpp"
#include "i_engine.hpp"
#include "own.hpp"
#include "fd.hpp"
#include "stdint.hpp"
#include "blob.hpp"

namespace zmq
{

    //  The class handles initialisation phase of 0MQ wire-level protocol.

    class zmq_init_t : public own_t, public i_inout
    {
    public:

        zmq_init_t (class io_thread_t *io_thread_, class socket_base_t *socket_,
            class session_t *session_, fd_t fd_, const options_t &options_);
        ~zmq_init_t ();

    private:

        //  Peer property IDs.
        enum prop_t
        {
            prop_type = 1,
            prop_identity = 2
        };

        void finalise_initialisation ();
        void dispatch_engine ();

        //  i_inout interface implementation.
        bool read (::zmq_msg_t *msg_);
        bool write (::zmq_msg_t *msg_);
        void flush ();
        void detach ();

        //  Handlers for incoming commands.
        void process_plug ();
        void process_unplug ();

        //  Associated wire-protocol engine.
        i_engine *engine;

        //  Detached transient engine.
        i_engine *ephemeral_engine;

        //  List of messages to send to the peer during the connection
        //  initiation phase.
        typedef std::vector < ::zmq_msg_t> to_send_t;
        to_send_t to_send;

        //  True if peer's identity was already received.
        bool received;

        //  Socket the object belongs to.
        class socket_base_t *socket;

        //  Reference to the session the init object belongs to.
        //  If the associated session is unknown and should be found
        //  depending on peer identity this value is NULL.
        class session_t *session;

        //  Identity of the peer socket.
        blob_t peer_identity;

        //  I/O thread the object is living in. It will be used to plug
        //  the engine into the same I/O thread.
        class io_thread_t *io_thread;

        zmq_init_t (const zmq_init_t&);
        const zmq_init_t &operator = (const zmq_init_t&);
    };

}

#endif

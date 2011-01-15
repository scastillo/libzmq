/*
    Copyright (c) 2007-2010 iMatix Corporation

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

#include <string.h>

#include "../include/zmq.h"

#include "xsub.hpp"
#include "err.hpp"

zmq::xsub_t::xsub_t (class ctx_t *parent_, uint32_t tid_) :
    socket_base_t (parent_, tid_),
    fq (this),
    dist (this),
    has_message (false),
    more (false)
{
    options.type = ZMQ_XSUB;
    options.requires_in = true;
    options.requires_out = true;

    //  When connection fails, session disconnects the associated subscription
    //  pipe. When the engine is reconnected, new pipe is created and all
    //  the subscriptions cached by the socket are re-sent upstream.
    options.disconnect_out = true;

    //  In XSUB and SUB sockets outbound messages are exclusively subscriptions.
    //  Thus, given it does not make sense to send the subscriptions when
    //  the socket is terminating anyway, we set linger period to 0.
    options.linger = 0;

    zmq_msg_init (&message);
}

zmq::xsub_t::~xsub_t ()
{
    zmq_msg_close (&message);
}

void zmq::xsub_t::xattach_pipes (class reader_t *inpipe_,
    class writer_t *outpipe_, const blob_t &peer_identity_)
{
    zmq_assert (inpipe_ && outpipe_);

    //  Send all the cached subscriptions to the new upstream peer.
    subscriptions.apply (send_subscription, outpipe_);
    outpipe_->flush ();

    fq.attach (inpipe_);
    dist.attach (outpipe_);
}

void zmq::xsub_t::process_term (int linger_)
{
    fq.terminate ();
    dist.terminate ();
    socket_base_t::process_term (linger_);
}

int zmq::xsub_t::xsend (zmq_msg_t *msg_, int options_)
{
    size_t size = zmq_msg_size (msg_);
    unsigned char *data = (unsigned char*) zmq_msg_data (msg_);

    if (size >= 1) {

        //  Process a subscription and send it upstream.
        if (*data == 1) {
            subscriptions.add (data + 1, size - 1);
            return dist.send (msg_, options_);
        }

        //  Process an unsubscription. Invalid unsubscription is ignored.
        if (*data == 0) {
            subscriptions.rm (data + 1, size - 1);
            return dist.send (msg_, options_);
        }
    }

    //  Malformed subscriptions are dropped silently.
    int rc = zmq_msg_close (msg_);
    zmq_assert (rc == 0);
    rc = zmq_msg_init (msg_);
    zmq_assert (rc == 0);
    return 0;
}

bool zmq::xsub_t::xhas_out ()
{
    //  Subscription can be added/removed anytime.
    return true;
}

int zmq::xsub_t::xrecv (zmq_msg_t *msg_, int flags_)
{
    //  If there's already a message prepared by a previous call to zmq_poll,
    //  return it straight ahead.
    if (has_message) {
        zmq_msg_move (msg_, &message);
        has_message = false;
        more = msg_->flags & ZMQ_MSG_MORE;
        return 0;
    }

    //  TODO: This can result in infinite loop in the case of continuous
    //  stream of non-matching messages which breaks the non-blocking recv
    //  semantics.
    while (true) {

        //  Get a message using fair queueing algorithm.
        int rc = fq.recv (msg_, flags_);

        //  If there's no message available, return immediately.
        //  The same when error occurs.
        if (rc != 0)
            return -1;

        //  Check whether the message matches at least one subscription.
        //  Non-initial parts of the message are passed 
        if (more || match (msg_)) {
            more = msg_->flags & ZMQ_MSG_MORE;
            return 0;
        }

        //  Message doesn't match. Pop any remaining parts of the message
        //  from the pipe.
        while (msg_->flags & ZMQ_MSG_MORE) {
            rc = fq.recv (msg_, ZMQ_NOBLOCK);
            zmq_assert (rc == 0);
        }
    }
}

bool zmq::xsub_t::xhas_in ()
{
    //  There are subsequent parts of the partly-read message available.
    if (more)
        return true;

    //  If there's already a message prepared by a previous call to zmq_poll,
    //  return straight ahead.
    if (has_message)
        return true;

    //  TODO: This can result in infinite loop in the case of continuous
    //  stream of non-matching messages.
    while (true) {

        //  Get a message using fair queueing algorithm.
        int rc = fq.recv (&message, ZMQ_NOBLOCK);

        //  If there's no message available, return immediately.
        //  The same when error occurs.
        if (rc != 0) {
            zmq_assert (errno == EAGAIN);
            return false;
        }

        //  Check whether the message matches at least one subscription.
        if (match (&message)) {
            has_message = true;
            return true;
        }

        //  Message doesn't match. Pop any remaining parts of the message
        //  from the pipe.
        while (message.flags & ZMQ_MSG_MORE) {
            rc = fq.recv (&message, ZMQ_NOBLOCK);
            zmq_assert (rc == 0);
        }
    }
}

bool zmq::xsub_t::match (zmq_msg_t *msg_)
{
    return subscriptions.check ((unsigned char*) zmq_msg_data (msg_),
        zmq_msg_size (msg_));
}

void zmq::xsub_t::send_subscription (unsigned char *data_, size_t size_,
    void *arg_)
{
    writer_t *outpipe = (writer_t*) arg_;

    //  Create the subsctription message.
    zmq_msg_t msg;
    int rc = zmq_msg_init_size (&msg, size_ + 1);
    zmq_assert (rc == 0);
    unsigned char *data = (unsigned char*) zmq_msg_data (&msg);
    data [0] = 1;
    memcpy (data + 1, data_, size_);

    //  Send it to the pipe.
    bool sent = outpipe->write (&msg);
    zmq_assert (sent);
    zmq_msg_close (&msg);
}

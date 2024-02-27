/* mccp.c - MUD In The Middle telnet-ssl proxy */
/* Created: Wed Feb 21 01:21:51 PM EST 2024 malakai */
/* $Id: mccp.c,v 1.1 2024/02/27 04:39:21 malakai Exp $ */

/* Copyright © 2024 Jeff Jahr <malakai@jeffrika.com>
 *
 * This file is part of MUDitM - MUD in the Middle
 *
 * MUDitM is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * MUDitM is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MUDitM.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <glib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <zlib.h>

#include "debug.h"
#include "proxy.h"
#include "handlers.h"
#include "debug.h"

#include "mccp.h"

/* ---- local #defines ---- */

/* ---- structs and typedefs ---- */

/* ---- local variable declarations ---- */

/* ---- local function declarations ---- */
z_stream *new_zstream(void);


/* ---- code starts here ---- */
void add_mccp_game_patterns(Endpoint *ep) {

	muditm_debug("%s config game mccp2 mode %d",ep->name,ep->mccp_mode);
	
	switch (ep->mccp_mode) {

		case MCCP_IGNORE: {
			/* look for the start of compression, and disable further pattern matching. */
			char mccp_trig[] = { IAC, SB, TELOPT_MCCP, IAC, SE };
			add_pattern(ep,mccp_trig,sizeof(mccp_trig),mccp_ignore);

			char mccp2_trig[] = { IAC, SB, TELOPT_MCCP2, IAC, SE };
			add_pattern(ep,mccp2_trig,sizeof(mccp2_trig),mccp_ignore);

			char mccp3_trig[] = { IAC, SB, TELOPT_MCCP3, IAC, SE };
			add_pattern(ep,mccp3_trig,sizeof(mccp3_trig),mccp_ignore);
			return;
		}

		default:
			/* this is the muditim pre 0.5 behavior- match and respond to game
			 * side messages. */
		case MCCP_DISABLE: {
			/* respond with a "don't" to all of game's offers to do MCCP. */

			char mccp_trig[] = { IAC, WILL, TELOPT_MCCP };
			add_pattern(ep,mccp_trig,sizeof(mccp_trig),respond_dont);

			char mccp2_trig[] = { IAC, WILL, TELOPT_MCCP2 };
			add_pattern(ep,mccp2_trig,sizeof(mccp2_trig),respond_dont);

			char mccp3_trig[] = { IAC, WILL, TELOPT_MCCP3 };
			add_pattern(ep,mccp3_trig,sizeof(mccp3_trig),respond_dont);
			
			return;
		}

		case MCCP_ENABLE: {

			char mccp2_trig[] = { IAC, WILL, TELOPT_MCCP2 };
			add_pattern(ep,mccp2_trig,sizeof(mccp2_trig),respond_do);

			char mccp2_start[] = { IAC, SB, TELOPT_MCCP2, IAC, SE };
			add_pattern(ep,mccp2_start,sizeof(mccp2_start),mccp2_sb_start);

			return;
		}

	}
	return;
}

void add_mccp_client_patterns(Endpoint *ep) {
	
	muditm_debug("%s config client mccp2 mode %d",ep->name,ep->mccp_mode);

	switch (ep->mccp_mode) {

		default:
		case MCCP_IGNORE: {
			/* this is the muditim pre 0.5 behavior- no patterns on client side. */
			/* You know nothing Jon Snow. */
			return;
		}

		case MCCP_DISABLE: {
			/* the client isn't supposed to open up with an unsolicited DO..
			 * but some might. */
			char mccp_trig[] = { IAC, DO, TELOPT_MCCP };
			add_pattern(ep,mccp_trig,sizeof(mccp_trig),respond_wont);

			char mccp2_trig[] = { IAC, DO, TELOPT_MCCP2 };
			add_pattern(ep,mccp2_trig,sizeof(mccp2_trig),respond_wont);

			char mccp3_trig[] = { IAC, DO, TELOPT_MCCP3 };
			add_pattern(ep,mccp3_trig,sizeof(mccp3_trig),respond_wont);
			
			return;
		}

		case MCCP_ENABLE: {

			char mccp2_doseq[] = { IAC, DO, TELOPT_MCCP2 };
			add_pattern(ep,mccp2_doseq,sizeof(mccp2_doseq),mccp2_do);

			char mccp2_dontseq[] = { IAC, DONT, TELOPT_MCCP2 };
			add_pattern(ep,mccp2_dontseq,sizeof(mccp2_dontseq),mccp2_dont);
			
			return;
		}
	}
	return;
}

/* init a new z_stream struct. */
z_stream *new_zstream(void) {

	z_stream *z;

	z = (z_stream *)malloc(sizeof(z_stream));
	z->data_type = Z_ASCII;
	z->zalloc = NULL;
	z->zfree = NULL;
	z->opaque = NULL;

	return(z);
}

void free_zstream(z_stream *z) {
	free(z);
	return;
}

/* set the mccp_mode based on the value from the config file. */
void configure_compression(Endpoint *ep,char *value) {

	if( !strcasecmp(value,"ignore") ) {
		ep->mccp_mode = MCCP_IGNORE;
	} else if( !strcasecmp(value,"disable") ) {
		ep->mccp_mode = MCCP_DISABLE;
	} else if( !strcasecmp(value,"enable") ) {
		ep->mccp_mode = MCCP_ENABLE;
	}

}

/* inject the WILL MCCP2 offer. */
void offer_compression(Endpoint *ep) {
	char mccp2_offer[] = { IAC, WILL, TELOPT_MCCP2 };
	Iobuf *out = ep->iobuf[EP_OUTPUT];

	muditm_debug("Offering WILL MCCP2 to %s",ep->name);
	if(ep->mccp_mode == MCCP_ENABLE) {
		memcpy(tail_iobuf(out),mccp2_offer,sizeof(mccp2_offer));
		push_iobuf(out,sizeof(mccp2_offer));
		flush_endpoint(ep);
	}

}

/* The stream is no longer TELNET, is it ZLIB.  disable TELNET pattern matching. */
int mccp_ignore(Iobuf *iob,size_t match_len,Endpoint *from, Endpoint *to,GKeyFile *gkf) {

	/* just make a note of if. */
	muditm_log("%s has entered mccp compression, matching disabled.",from->name);
	disable_matching(from);
	return(0);
};

/* A side just requested compression.  Set it up. */
int mccp2_do(Iobuf *iob,size_t match_len,Endpoint *from, Endpoint *to,GKeyFile *gkf) {

	Iobuf *out;
	z_stream *z;
	int ret;

	muditm_log("%s has agreed to mccp2 compression.",from->name);
	
	/* Remove that match from the input buffer, we don't want it sent across. */
	pop_iobuf(iob, match_len);

	/* set up the z_stream */
	z = new_zstream();

	/* Scandum's implementation uses deflateInit2() with some options, but
	 * using deflateInit() with the defaults seems to work. */
	if( (ret = deflateInit(z,Z_BEST_COMPRESSION)) != Z_OK ) {
		muditm_log("deflateInit failure code %d?",ret);
		free_zstream(z);
		return(0);
	}

	/* send the "compressed stream begins now" message */
	muditm_debug("Sending start of compression message.");
	out = (from->iobuf[EP_OUTPUT]);
	char mccp2_begin[] = { IAC, SB, TELOPT_MCCP2, IAC, SE };
	memcpy(tail_iobuf(out),mccp2_begin,sizeof(mccp2_begin));
	push_iobuf(out,sizeof(mccp2_begin));
	flush_endpoint(from);

	/* and now really turn it on.*/
	from->mccp[EP_OUTPUT] = z;

	return(1);
};

/* A side just requested NO compression.  disable it.*/
int mccp2_dont(Iobuf *iob,size_t match_len,Endpoint *from, Endpoint *to,GKeyFile *gkf) {

	if(from->mccp[EP_OUTPUT]) {
		muditm_log("%s wants to shut down mccp2 compression.",from->name);
	} else {
		muditm_log("%s refuses mccp2 compression.",from->name);
	}
	
	/* Remove that match from the input buffer, we don't want it sent across. */
	pop_iobuf(iob, match_len);

	/* and now really turn it off.*/
	if(from->mccp[EP_OUTPUT]) {
		free_zstream(from->mccp[EP_OUTPUT]);
		from->mccp[EP_OUTPUT] = NULL;
	}

	return(1);
}

/* A side just saw the start of compression message.  Set it up. */
int mccp2_sb_start(Iobuf *iob,size_t match_len,Endpoint *from, Endpoint *to,GKeyFile *gkf) {

	z_stream *z;
	int ret;

	muditm_log("%s has switched to mccp2 compression.",from->name);
	
	/* Remove that match from the input buffer, we don't want it sent across. */
	pop_iobuf(iob, match_len);

	/* set up the z_stream */
	z = new_zstream();

	if( (ret = inflateInit(z)) != Z_OK ) {
		muditm_log("inflateInit failure code %d?",ret);
		free_zstream(z);
		return(0);
	}

	/* and now really turn it on.*/
	z->next_in = (unsigned char *)head_iobuf(from->ziobuf[EP_INPUT]);
	z->avail_in = 0;
	from->mccp[EP_INPUT] = z;

	return(1);
}

ssize_t read_endpoint_compressed(Endpoint *ep, void *buf, size_t count) {

	ssize_t readtotal,readsize,finalsize, ret;
	char *workspace;
	int reads;
	z_stream *zstr;

	/* if compression is not active, this is just a simple call to
	 * read_endpoint_sock(). */
	if(!(zstr = ep->mccp[EP_INPUT])) {
		return(read_endpoint_sock(ep,buf,count));
	}

	/* Compression is active. This is more involved. */
	/* the initial settings on this z_stream are set up during deflateInit.  If
	 * read_endpoint is called and there are still input bytes left over from
	 * the last read (indicated by zstr->avail being non-zero) the
	 * read_sendpoint_sock is skipped over so that another inflate() can be
	 * processed and placed into the provided space. */
	workspace = head_iobuf(ep->ziobuf[EP_INPUT]);

	/* reset the output to point at the provided space. */
	zstr->next_out = (unsigned char*)buf;
	zstr->avail_out = count;
	zstr->total_out = 0;

	readtotal = 0;
	reads =0;

	/* This do loop keeps processing compressed input from the socket until
	 * some uncompressed data has been produced, or a read error occurs. */
	do {

		/* Need more bytes? */
		if(zstr->avail_in == 0) {

			/* sure do.  read some more from the socket. */
			reads++;
			readsize = read_endpoint_sock(ep,workspace,EP_BUFSIZE);
		
			if(readsize <= 0) {
				/* There was a socket read error, so pass back up.  This sucks
				 * because it breaks the semantics of poll() on a read_endpoint
				 *  socket, but there's nothing for it. */
				muditm_debug("%s inflate socket_read code %d is errno %d %s",
					ep->name,readsize,errno,strerror(errno)
				);
				return(readsize);
			}
			readtotal += readsize;

			zstr->next_in = (unsigned char *)workspace;
			zstr->avail_in = readsize;
			zstr->total_in = 0;
		}

		if( (ret = inflate(zstr,Z_SYNC_FLUSH)) != Z_OK) {
			muditm_log("%s inflate problem? '%s' code %d",ep->name,zstr->msg,ret);
			return(-1);
		}

	} while (zstr->next_out == buf);

	if(zstr->avail_out == 0) {
		muditm_debug("%s out of inflate space.",ep->name,ret);
	}

	finalsize = zstr->total_out;
	zstr->total_out = 0;

	iostat_incr(&(ep->mccpstats),finalsize,0);
	muditm_debug("%s inflate- read %d reads %d total_out %d",ep->name,readtotal,reads,finalsize);
	return(finalsize);

}

ssize_t write_endpoint_compressed(Endpoint *ep, void *buf, size_t count) {

	ssize_t consumed,written,ret;
	int writes;
	
	char *workspace;
	z_stream *zstr;

	/* if compression is not active, this is just a simple call to
	 * read_endpoint_sock(). */
	if(!(zstr = ep->mccp[EP_OUTPUT])) {
		return(write_endpoint_sock(ep,buf,count));
	}

	/* Compression is active, This is more involved. */
	workspace = head_iobuf(ep->ziobuf[EP_OUTPUT]);

	zstr->next_in = (unsigned char *)buf;
	zstr->avail_in = count;
	zstr->total_in = 0;

	consumed = 0;
	written = 0;
	writes = 0;

	do {
		/* this do loop feels like a lot of work, becuase deflate is always
		 * supposed to compress the write into fewer bytes than the input,
		 * right?  Except that it isn't guaranteed to.  Although very unlikely,
		 * it is possible that the deflate algorithm could be handed a block of
		 * input that deflates into more data than is available in EP_BUFSIZE,
		 * which would requires a write to the wire and a second pass through
		 * deflate with the same input. */

		/* reset the workspace window to the start */
		zstr->next_out = (unsigned char*)workspace;
		zstr->avail_out = EP_BUFSIZE;
		zstr->total_out = 0;

		if( (ret = deflate(zstr,Z_SYNC_FLUSH)) != Z_OK) {
			muditm_log("%s deflate problem? '%s' code %d",ep->name,zstr->msg,ret);
			// exit(EXIT_FAILURE);
			return(-1);
		} 

		writes++;

		if( (ret = write_endpoint_sock(ep,workspace,zstr->total_out)) <= 0 ) {
			if(consumed != 0) {
				/* There was a successful partial write before this failure, so
				 * return the byte count that was presumably sent without
				 * error.  It the write_endpoint_sock fails immediately the
				 * next time the code gets here, the actual error code from it
				 * will propigate up. I believe this mimics write(); */
				break;
			} else {
				/* no bytes were put onto the wire, return the underlying error
				 * code. */
				return(ret);
			}
		}
		consumed += zstr->total_in;
		zstr->total_in = 0;
		written += zstr->total_out;

	} while (zstr->avail_out == 0);

	muditm_debug("%s deflate- total offered %d consumed %d written %d writes %d",
		ep->name,count,consumed,written,writes
	);

	iostat_incr(&(ep->mccpstats),0,consumed);
	/* we want to return the number of bytes that were consumed from the
	 * arguments, not the actual amount of bytes put out onto the wire. So
	 * return the total_in bytes consumed. It should be == to count. */
	return(zstr->total_in);
}

/* proxy.h - guts of the telnet proxy */
/* Created: Sat Mar  6 08:06:50 AM EST 2021 malakai */
/* $Id: proxy.h,v 1.8 2024/02/27 04:39:21 malakai Exp $ */

/* Copyright © 2021-2024 Jeff Jahr <malakai@jeffrika.com>
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

#ifndef MUDITM_PROXY_H
#define MUDITM_PROXY_H

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <zlib.h>
#include "iobuf.h"
#include "iostats.h"

/* global #defines */
#define EP_INPUT 0
#define EP_OUTPUT 1
#define EP_MAX 2

#define EP_BUFSIZE (1<<16)

/* structs and typedefs */

struct buffer_data {
	char sob[EP_BUFSIZE]; 
	char *b;
	char *eob;
};

struct endpoint_data {
	char *name;
	int socket;
	SSL *ssl;
	Iobuf *iobuf[EP_MAX];

	int matching_enabled;
	GList *patterns;
	pcre2_code *re;
	pcre2_match_data *match_data;

	int mnes_state;

	int mccp_mode;
	z_stream *mccp[EP_MAX];
	Iobuf *ziobuf[EP_MAX];
	struct iostat_data sockstats;
	struct iostat_data mccpstats;

};

typedef struct endpoint_data Endpoint;

struct flow_data {
	Endpoint *in;
	Endpoint *out;
};


/* exported global variable declarations */

/* exported function declarations */
Endpoint *new_endpoint(char *name);
int ssl_start_endpoint(Endpoint *ep, SSL_CTX *ctx, int connect);
ssize_t write_endpoint(Endpoint *ep, void *buf, size_t count);
ssize_t write_endpoint_sock(Endpoint *ep, void *buf, size_t count);
ssize_t flush_endpoint(Endpoint *ep);
ssize_t read_endpoint(Endpoint *ep, void *buf, size_t count);
ssize_t read_endpoint_sock(Endpoint *ep, void *buf, size_t count);
int close_endpoint(Endpoint *ep);
void free_endpoint(Endpoint *ep);
char *addr_endpoint(Endpoint *ep, char *buf, size_t size);

int muditm_proxy(Endpoint *client, Endpoint *game, GKeyFile *gkf);
size_t stunnel_proxy_header1(Endpoint *ep, char *buf, size_t size);

#endif /* MUDITM_PROXY_H */

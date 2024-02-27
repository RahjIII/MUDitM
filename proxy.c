/* proxy.c - guts of the telnet proxy */
/* Created: Sat Mar  6 08:06:50 AM EST 2021 malakai */
/* $Id: proxy.c,v 1.12 2024/02/27 04:39:21 malakai Exp $ */

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

#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <fcntl.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>

#include "debug.h"
#include "muditm.h"
#include "iobuf.h"

#include "proxy.h"
#include "mccp.h"
#include "handlers.h"

Endpoint *new_endpoint(char *name) {
	Endpoint *ep;
	int e;

	ep = (Endpoint *)malloc(sizeof(Endpoint));

	if(name) {
		ep->name = strdup(name);
	} else {
		ep->name = strdup("unnamed");
	}
	ep->socket = -1;
	ep->ssl = NULL;
	ep->mnes_state = 0;
	ep->matching_enabled = 0;
	ep->patterns = NULL;
	ep->re = NULL;
	ep->match_data = NULL;
	ep->mccp_mode = MCCP_DISABLE;

	/* alloc the iobuf for each available direction */
	for(e=0;e<EP_MAX;e++) {
		ep->iobuf[e] = new_iobuf(EP_BUFSIZE);
	}

	/* NULL the mccp zlib state in each direction. State will be allocated when
	 * compression is negotiated. */
	for(e=0;e<EP_MAX;e++) {
		ep->mccp[e] = NULL;
		ep->ziobuf[e] = new_iobuf(EP_BUFSIZE);
	}
	iostat_init(&(ep->sockstats));
	iostat_init(&(ep->mccpstats));

	return(ep);

}

void free_endpoint(Endpoint *ep) {
	GList *l;
	z_stream *z;
	int e;

	if(!ep) return;
	close_endpoint(ep);

	if(ep->name) free(ep->name);

	for(e=0;e<EP_MAX;e++) {
		if(ep->iobuf[e]) free_iobuf(ep->iobuf[e]);
	}

	if(ep->patterns) {
		for(l=ep->patterns; l ; l=l->next) {
			free_pattern(l->data);
		}
		g_list_free(ep->patterns);
	}
	if(ep->re) pcre2_code_free(ep->re);
	if(ep->match_data) pcre2_match_data_free(ep->match_data);

	/* free up any z_stream buffers. */
	for(e=0;e<EP_MAX;e++) {
		z = ep->mccp[e];
		if(z) { 
			if(e == EP_OUTPUT) {
				deflateEnd(z);
			} else {
				inflateEnd(z);
			}
			free(z);
		}
		if(ep->ziobuf[e]) free_iobuf(ep->ziobuf[e]);
	}

	free(ep);
}

int close_endpoint(Endpoint *ep) {

	int ret;

	if(ep->ssl != NULL) {
		SSL_shutdown(ep->ssl);
		SSL_free(ep->ssl);
		ep->ssl = NULL;
	}
	if(ep->socket >= 0) {
		ret = close(ep->socket);
		ep->socket = -1;
	}
	return(ret);
}


/* prints the source address of the endpoint into buf of given size */
char *addr_endpoint(Endpoint *ep, char *buf, size_t size) {

	struct sockaddr_in6 sa;
	socklen_t sa_size = sizeof(struct sockaddr_in6);

	*buf='\0';

	if(getpeername(ep->socket,(struct sockaddr *)&sa, &sa_size) <0) {
		muditm_log("getpeername: ",strerror(errno));
		return(buf);
	}
	
	inet_ntop(AF_INET6,&((struct sockaddr_in6 *)&sa)->sin6_addr,buf,size);
	if(!strncmp(buf,"::ffff:",7)) {
		/* unmaps the printed address to plain old ipv4. */
		memmove(buf,buf+7,strlen(buf)-6);
	}
	return(buf);
}

/* prints the source address of the endpoint into buf of given size */
size_t stunnel_proxy_header1(Endpoint *ep, char *buf, size_t size) {

	struct sockaddr_in6 src;
	struct sockaddr_in6 dst;
	socklen_t sa_size = sizeof(struct sockaddr_in6);
	char srcaddr[INET6_ADDRSTRLEN];
	char dstaddr[INET6_ADDRSTRLEN];
	int ismapped=0;
	int count;

	*buf='\0';

	/* get the source address */
	if(getpeername(ep->socket,(struct sockaddr *)&src, &sa_size) <0) {
		muditm_log("getpeername: ",strerror(errno));
		return(0);
	}
	
	inet_ntop(AF_INET6,&((struct sockaddr_in6 *)&src)->sin6_addr,srcaddr,sizeof(srcaddr));
	if(!strncmp(srcaddr,"::ffff:",7)) {
		/* unmaps the printed address to plain old ipv4. */
		ismapped=1;
		memmove(srcaddr,srcaddr+7,strlen(srcaddr)-6);
	}
	/* get the destination address */
	if(getsockname(ep->socket,(struct sockaddr *)&dst, &sa_size) <0) {
		muditm_log("getsockname: ",strerror(errno));
		return(0);
	}
	
	inet_ntop(AF_INET6,&((struct sockaddr_in6 *)&dst)->sin6_addr,dstaddr,sizeof(dstaddr));
	if(!strncmp(dstaddr,"::ffff:",7)) {
		/* unmaps the printed address to plain old ipv4. */
		ismapped=1;
		memmove(dstaddr,dstaddr+7,strlen(dstaddr)-6);
	}

	count = snprintf(buf,size,"PROXY %s %s %s %d %d\r\n",
		ismapped?"TCP4":"TCP6",
		srcaddr,
		dstaddr,
		ntohs(src.sin6_port),
		ntohs(dst.sin6_port)
	);

	return(count);
}

ssize_t read_endpoint_sock(Endpoint *ep, void *buf, size_t count) {

	int readsize, err;

	if(ep->ssl) {
		while(1) {
			readsize = SSL_read(ep->ssl,buf,count);
			if(readsize == -1) {
				err = SSL_get_error(ep->ssl,readsize);
				if(err == SSL_ERROR_WANT_READ) {
					continue;
				}
			}
			break;
		}
	} else {
		readsize = read(ep->socket,buf,count);
	}
	iostat_incr(&(ep->sockstats),readsize,0);
	return(readsize);
}

ssize_t read_endpoint(Endpoint *ep, void *buf, size_t count) {

	if(ep->mccp[EP_INPUT]) {
		return(read_endpoint_compressed(ep,buf,count));
	}
	return(read_endpoint_sock(ep,buf,count));

}

ssize_t flush_endpoint(Endpoint *ep) {
	int ret;
	ret = write_endpoint(ep,head_iobuf(ep->iobuf[EP_OUTPUT]),len_iobuf(ep->iobuf[EP_OUTPUT]));
	if(ret > 0) {
		pop_iobuf(ep->iobuf[EP_OUTPUT],ret);
	}
	return(ret);
}

ssize_t write_endpoint_sock(Endpoint *ep, void *buf, size_t count) {

	int writesize, err;
	
	/* if socket is ssl, use SSL_write. */
	if(ep->ssl) {
		while(1) {
			writesize = SSL_write(ep->ssl,buf,count);
			if(writesize == -1) {
				err = SSL_get_error(ep->ssl,writesize);
				if(err == SSL_ERROR_WANT_WRITE) {
					continue;
				}
			}
			break;
		}
	} else {
		/* fixme... ewouldblock, eagain... */
		writesize = write(ep->socket,buf,count);
	}
		
	iostat_incr(&(ep->sockstats),0,writesize);
	return(writesize);
}

ssize_t write_endpoint(Endpoint *ep, void *buf, size_t count) {

	/* if compression active... */
	if(ep->mccp[EP_OUTPUT]) {
		return(write_endpoint_compressed(ep,buf,count));
	}
	return(write_endpoint_sock(ep,buf,count));
}

int ssl_start_endpoint(Endpoint *ep, SSL_CTX *ctx, int connect) {

	int ret;

	if (!ctx) {
		muditm_log("Missing SSL context!");
		muditm_sslerr("Missing SSL context!");
		exit(EXIT_FAILURE);
	}

	ep->ssl = SSL_new(ctx);
	SSL_set_fd(ep->ssl,ep->socket);

	muditm_log("%s SSL start on socket %d",ep->name,ep->socket);
	if(connect) {
		if ( (ret=SSL_connect(ep->ssl)) <= 0) {
			muditm_sslerr("%s SSL_accept",ep->name);
			return(ret);
		} 
		muditm_log("%s SSL connected on socket %d",ep->name,ep->socket);
	} else {
		if ( (ret=SSL_accept(ep->ssl)) <= 0) {
			muditm_sslerr("%s SSL_accept",ep->name);
			return(ret);
		} 
		muditm_log("%s SSL accepted on socket %d",ep->name,ep->socket);
	}

	return(ret);
}


int muditm_proxy(Endpoint *client, Endpoint *game, GKeyFile *gkf) {

	struct pollfd pollster[2];
	struct flow_data flow[2];
	int pollster_count = 2;
	int polltimeout = 1000;
	int ready;
	ssize_t bytes_recv, bytes_sent;
	size_t match_len;
	int ret;
	struct pattern_data *p;
	PCRE2_SIZE *ovector;
	int handled;


	Iobuf *iob;

	/* set up the game side filters */
	add_game_patterns(game);

	/* set up the client side filters */
	add_client_patterns(client);

	/* reset the stats timers to zero. */
	iostat_init(&(client->sockstats));
	iostat_init(&(game->mccpstats));

	/* if mccp is enabled, send WILL MCCP2 to the client side. */
	offer_compression(client);
	
	pollster[0].fd = client->socket;
	pollster[0].events = POLLIN;
	flow[0].in = client;
	flow[0].out = game;

	if(fcntl(client->socket,F_SETFL, O_NONBLOCK) == -1) {
		muditm_log("Couldn't set client side to non-blocking io mode: ",strerror(errno));
		ret=-1;
		goto cleanup;
	}

	pollster[1].fd = game->socket;
	pollster[1].events = POLLIN;
	flow[1].in = game;
	flow[1].out = client;

	if(fcntl(game->socket,F_SETFL, O_NONBLOCK) == -1) {
		muditm_log("Couldn't set server side to non-blocking io mode: ",strerror(errno));
		ret=-1;
		goto cleanup;
	}


	while(1) {

		/* poll for input */
		ready = poll(pollster,pollster_count,polltimeout);

		if(ready == -1) {
			muditm_log("Polling error: %s",strerror(errno));
			ret=-1;
			goto cleanup;
		}

		if(ready == 0) {
			//muditm_log("Idle Tick...");
			continue;
		}

		for(int i=0;i<pollster_count;i++) {
			if(pollster[i].revents & POLLIN) {
				iob = (flow[i].in->iobuf[EP_INPUT]);
				bytes_recv = read_endpoint(flow[i].in,tail_iobuf(iob),avail_iobuf(iob));
				if(bytes_recv == -1) {
					/* If compression is enabled, read_endpoint may need to do
					 * multiple read()'s before it can return data, and only
					 * the first read() is guarenteed to return some bytes. */
					muditm_log("%s errno %d %s",flow[i].in->name, errno, strerror(errno));
					if( (errno == EAGAIN) || 
						(errno == EWOULDBLOCK)
					) {
						muditm_log("%s wasn't really ready.",flow[i].in->name);
						/* so skip processing this one. */
						continue;
					}
					ret=-1;
					goto cleanup;
				}	
				if(bytes_recv == 0) {
					/* He hung up. */
					muditm_log("%s has closed the connection.",flow[i].in->name);
					ret=1;
					goto cleanup;
				}	
				push_iobuf(iob,bytes_recv);

				/* This is the creamy filling in the middle.  (The pcre2
				 * pattern matching loop.)  */
				while(1) {

					/* matching_enabled is a flag to tell if any matching
					 * should be tried at all, and without checking for or
					 * getting rid of any of the configured patterns that might
					 * exist.  Bascially allows the endpoint to simply go
					 * transparent, should that be needed, as is the case when
					 * the stream leaves telnet mode for mccp zlib compression
					 * mode. */
					if( flow[i].in->matching_enabled ) {
						/* run pcre2. */
						ret = pcre2_match( flow[i].in->re,
							(PCRE2_SPTR)head_iobuf(iob), len_iobuf(iob),
							0,
							PCRE2_PARTIAL_HARD,
							flow[i].in->match_data,
							NULL
						);
					} else {
						/* A small lie. Not really an error, matches are just
						 * disabled.*/
						ret = PCRE2_ERROR_NOMATCH;
					}

					if (ret == PCRE2_ERROR_NOMATCH ) {
						/* write the whole buffer */
						bytes_sent = write_endpoint(flow[i].out,head_iobuf(iob),len_iobuf(iob));
						popall_iobuf(iob);
						/* done, all available input is processed! */
						break;
					} else if (ret == PCRE2_ERROR_PARTIAL) {
						/* still needing to add more input. */
						muditm_log("partial match...");
						break;
					} else {
						/* we've got a match to handle. */
						ovector = pcre2_get_ovector_pointer(flow[i].in->match_data);

						match_len = (ovector[1]-ovector[0]);

						/* ship all of the bytes up to, but not including, the match. */
						if(ovector[0]>0) {
							bytes_sent = write_endpoint(flow[i].out,
								head_iobuf(iob), 
								ovector[0]
							);
							pop_iobuf(iob,ovector[0]);
						}

						/* match is now at head_iobuf(iob) */

						/* get a pointer to the pattern that matched. */
						handled = 0;
						if ( (p = g_list_nth_data(flow[i].in->patterns,ret-2))) {
							if(p->action) {
								/* trigger(iobuf_of_match,match_len,fromendpoint,toendpoint) */
								handled = (p->action)(iob,match_len,flow[i].in,flow[i].out,gkf);
							} else {
								muditm_log("null pattern handler?");
							}
						} else {
							muditm_log("Couldn't find the pattern_data that matched?");
						}

						if(!handled) {
							/* the trigger left the input buffer for us to copy over*/
							bytes_sent = write_endpoint(flow[i].out,
								head_iobuf(iob), 
								match_len
							);
							pop_iobuf(iob,match_len);
						}

						if(len_iobuf(iob)>0) {
							continue;
						} else {
							break;
						}
					}	/* end of match to handle */
				}	/* end of pcre2 matching loop */
			}	/* end of polling loop */
		} /* end of pollster loop */
	}
	cleanup:
	return(ret);
}


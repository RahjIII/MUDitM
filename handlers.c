/* handlers.h - matched patten handling code */
/* Created: Wed Mar 17 12:00:30 PM EDT 2021 malakai */
/* $Id: handlers.c,v 1.9 2024/02/27 04:39:21 malakai Exp $*/

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

/* Create a new pattern structure. */
struct pattern_data *new_pattern() {
	struct pattern_data *m;
	m = (struct pattern_data *)malloc(sizeof(struct pattern_data));
	m->pat = NULL;
	m->len = 0;
	m->action = NULL;
	return(m);
}

/* free an existing pattern structure. */
void free_pattern(struct pattern_data *m) {
	if(m->pat) free (m->pat);
	free(m);
}


pcre2_code *compile_patterns(GList *patternlist) {

	GList *l;
	struct pattern_data *m;
	char buf[1<<16]; /* bah should do this dynamically but screw it. */
	char *s,*eos;
	pcre2_code *re;
	int errornumber;
	PCRE2_SIZE erroroffset;

	*buf='\0';
	s=buf;
	eos=s+(sizeof(buf));

	for(l = patternlist; l ; l=l->next) {
		m = l->data;
		if(s != buf) {
			/* concatenate with the pipe char. */
			s+=g_snprintf(s,eos-s,"|");
		}
		/* should check for unescaped pattern chars and parens in the patterns
		 * before doing this, because those will screw things up royally... for
		 * now, we'll try to remember not to aim at our toes when adding
		 * patterns to the list. */
		s+=g_snprintf(s,eos-s,"(%.*s)",(int)m->len,m->pat);
	}

	// muditm_log("pattern list is: '%s'",buf);

	re = pcre2_compile((PCRE2_SPTR)buf,PCRE2_ZERO_TERMINATED,
		0,
		&errornumber, &erroroffset,
		NULL
	);

	if (!re) {	
		PCRE2_UCHAR buffer[256];
		pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
		muditm_log("PCRE2 compilation failed at offset %d: %s\n", (int)erroroffset, buffer);
		exit(EXIT_FAILURE);
	}
	return(re);

}

int snprintf_mnes_pair(char *str, size_t size, char *var, char *val) {
	int s;
	s=g_snprintf(str,size,"%c%c%c%c%c%s%c%s%c%c",
		IAC, SB, TELOPT_NEW_ENVIRON, TELQUAL_IS,
		NEW_ENV_VAR, var,
		NEW_ENV_VALUE, val,
		IAC, SE
	);
	return(s);
}

void add_pattern(Endpoint *ep,char *pat, size_t size, PatternAction *handler) {
	struct pattern_data *p;
	p = new_pattern();
	p->pat = (char *)malloc(size);
	memcpy(p->pat,pat,size);
	p->len = size;
	p->action = handler;
	ep->patterns = g_list_append(ep->patterns,p);
}

/* set up some game->client filters */
void add_game_patterns(Endpoint *ep) {

	char mnes_sendall[] = { IAC, SB, TELOPT_NEW_ENVIRON, TELQUAL_SEND, IAC, SE };
	char mnes_sendallvar[] = { IAC, SB, TELOPT_NEW_ENVIRON, TELQUAL_SEND, NEW_ENV_VAR, IAC, SE };
	char mnes_do_trig[] = { IAC, DO, TELOPT_NEW_ENVIRON };

	/* add the mnes sendall patterns */
	add_pattern(ep,mnes_sendall,sizeof(mnes_sendall),mnes_request);
	add_pattern(ep,mnes_sendallvar,sizeof(mnes_sendallvar),mnes_request);

	/* add the mnes does pattern */
	add_pattern(ep,mnes_do_trig,sizeof(mnes_do_trig),mnes_does);

	/* add patterns for mccp appropriate for the endpoint configuration. */
	/* block some protocols. */
	add_mccp_game_patterns(ep);

	/* add some test triggers */
	/* add_pattern(ep,"DikuMUD",strlen("DikuMUD"),redact_match); */

	/* finally, install and enable them. */
	ep->re = compile_patterns(ep->patterns);
	ep->match_data = pcre2_match_data_create_from_pattern(ep->re, NULL);
	enable_matching(ep);

	return;
}

void enable_matching(Endpoint *ep) {
	ep->matching_enabled = 1;
}

void disable_matching(Endpoint *ep) {
	ep->matching_enabled = 0;
}

/* set up some game->client filters */
void add_client_patterns(Endpoint *ep) {

	struct pattern_data *p;
	
	char mnes_wont[] = { IAC, WONT, TELOPT_NEW_ENVIRON };

	/* add the mnes sendall pattern */
	p = new_pattern();
	p->pat = (char *)malloc(sizeof(mnes_wont));
	memcpy(p->pat,mnes_wont,sizeof(mnes_wont));
	p->len = sizeof(mnes_wont);
	p->action = mnes_client_wont;
	ep->patterns = g_list_append(ep->patterns,p);

	/* add any requested mccp patterns. */
	add_mccp_client_patterns(ep);

	/* finally, install and enable them. */
	ep->re = compile_patterns(ep->patterns);
	ep->match_data = pcre2_match_data_create_from_pattern(ep->re, NULL);
	enable_matching(ep);


	return;
}

/* Pattern Action Handlers - In an action handler, head_iob(iob) is the
 * location of the matched pattern bytes, which is usually the from's input
 * buffer.  match_len is how many bytes the match is.  There may be other input
 * data after that, so be careful what you take off of the input queue!  
 * 
 * The handler can either consume the input from the queue and return 1, or
 * leave the input alone, return 0, and the caller will send the match
 * across the proxy and consume it from the input.  Bad things will occur if
 * your handler returns a 1, but does NOT consume the matched pattern- it'll
 * get matched again, and cause a loop.  You've been warned! */

/* It is very simple to strip the match out of the input stream. */
int remove_match(Iobuf *iob,size_t match_len,Endpoint *from, Endpoint *to,GKeyFile *gkf) {

	/* stripping a match out is easy... just remove the match from the input
	 * buffer, and return 1. */
	pop_iobuf(iob,match_len);
	return(1);
};

/* example showing how to take a match out of the stream and replace it with
 * something else.*/
int redact_match(Iobuf *iob, size_t match_len, Endpoint *from, Endpoint *to, GKeyFile *gkf) {

	Iobuf *out;
	out = (to->iobuf[EP_OUTPUT]);

	/* Remove the input from the buffer, we aren't going to use it. */
	pop_iobuf(iob, match_len);

	/* queue the 'something else' to the output side of the proxy */
	push_iobuf(out, g_snprintf(tail_iobuf(out), avail_iobuf(out), "REDACTED"));

	/* send the output buffer. */
	flush_endpoint(to);

	/* mischief managed. */
	return (1);
}


/* server has asked for the list of all environment variables.  Inject our special ones! */
int mnes_request(Iobuf *iob, size_t match_len, Endpoint *from, Endpoint *to, GKeyFile *gkf) {
	
	gchar **ipreportlist;
	gsize ipreportcount;
	char addrtxt[INET6_ADDRSTRLEN];
	Iobuf *out;

	muditm_log("%s requested mnes new-environ info",from->name);
	
	ipreportlist = g_key_file_get_string_list (gkf,"muditm","newenv_ipaddress",&ipreportcount,NULL);

	/* this output is going back to who we got it from, not through the proxy to the other side */
	out = (from->iobuf[EP_OUTPUT]);

	/* queue up the proxy name */
	push_iobuf(out, 
		snprintf_mnes_pair(tail_iobuf(out),avail_iobuf(out),
			"PROXY_NAME",muditm_proxy_name
		)
	);

	/* queue all of the host report vars */
	if(ipreportlist) {
		addr_endpoint(to,addrtxt,sizeof(addrtxt));
		for(int i=0; i<ipreportcount; i++) {
			push_iobuf(out, 
				snprintf_mnes_pair(tail_iobuf(out),avail_iobuf(out),
					ipreportlist[i],addrtxt
				)
			);
			muditm_log("Sent %s '%s' to %s",
				ipreportlist[i],addrtxt,
				from->name
			);
		}
		g_strfreev (ipreportlist);
	}

	/* and write it out. */
	flush_endpoint(from);

	/* Do we need to filter out this server request? */
	if(to->mnes_state == WONT) {
		pop_iobuf(iob, match_len);
		return(1);
	}

	/* ...or let the caller forward it on. */
	return(0);

}

/* the client won't do new-env, but we will.  Take note of that, and lie about it. */
int mnes_client_wont(Iobuf *iob, size_t match_len, Endpoint *from, Endpoint *to, GKeyFile *gkf) {

	Iobuf *out;
	char mnes_will[] = { IAC, WILL, TELOPT_NEW_ENVIRON };

	muditm_log("%s wont mnes.",from->name);

	out = (to->iobuf[EP_OUTPUT]);

	/* note that the client side wont be doing mnes. */
	from->mnes_state = WONT;

	/* Remove that match from the input buffer, we don't want it send across. */
	pop_iobuf(iob, match_len);

	/* we're going to lie to the server about it instead. */
	memcpy(tail_iobuf(out),mnes_will,sizeof(mnes_will));
	push_iobuf(out,sizeof(mnes_will));
	flush_endpoint(to);

	/* mischief managed. */
	return (1);
}

int mnes_does(Iobuf *iob,size_t match_len,Endpoint *from, Endpoint *to,GKeyFile *gkf) {

	/* just make a note of if. */
	muditm_log("%s does new-environ.",from->name);
	from->mnes_state = DO;
	return(0);
};

/* whatever the sender said he will, we say X on behalf of the other side. */
int respond_X(Iobuf *iob, size_t match_len, Endpoint *from, Endpoint *to, GKeyFile *gkf, char X) {
	
	Iobuf *out;
	char proto;

	proto = *(head_iobuf(iob)+2);

	/* this output is going back to who we got it from, not through the proxy to the other side */
	out = (from->iobuf[EP_OUTPUT]);

	/* queue up the proxy name */
	push_iobuf(out, 
		g_snprintf(tail_iobuf(out),avail_iobuf(out),"%c%c%c",
			IAC, X, proto
		)
	);
	/* and write it out. */
	flush_endpoint(from);

	/* no need to pass that on to the other side. */
	pop_iobuf(iob, match_len);
	return(1);
}


/* whatever the sender said he will, we say dont. */
int respond_dont(Iobuf *iob, size_t match_len, Endpoint *from, Endpoint *to, GKeyFile *gkf) {
	
	char proto;

	proto = *(head_iobuf(iob)+2);

	muditm_debug("send IAC DONT %d to %s",proto,from->name);

	return(respond_X(iob,match_len,from,to,gkf,DONT));
}

/* whatever the sender said he will, we say dont. */
int respond_do(Iobuf *iob, size_t match_len, Endpoint *from, Endpoint *to, GKeyFile *gkf) {
	
	char proto;

	proto = *(head_iobuf(iob)+2);

	muditm_debug("send IAC DO %d to %s",proto,from->name);

	return(respond_X(iob,match_len,from,to,gkf,DO));
}

/* whatever the sender said he will, we say dont. */
int respond_wont(Iobuf *iob, size_t match_len, Endpoint *from, Endpoint *to, GKeyFile *gkf) {
	
	char proto;

	proto = *(head_iobuf(iob)+2);

	muditm_debug("send IAC WONT %d to %s",proto,from->name);

	return(respond_X(iob,match_len,from,to,gkf,DONT));
}

/* handlers.h - matched patten handling code */
/* Created: Wed Mar 17 12:00:30 PM EDT 2021 malakai */
/* $Id: handlers.h,v 1.3 2024/02/27 04:39:21 malakai Exp $*/

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

#ifndef MUDITM_HANDLERS_H
#define MUDITM_HANDLERS_H

/* global #defines */
#define TELOPT_MCCP2 86
#define TELOPT_MCCP3 87

/* structs and typedefs */
typedef int PatternAction(Iobuf *iob, size_t match_len,
	Endpoint *from, 
	Endpoint *to, 
	GKeyFile *gkf 
);

struct pattern_data {
	char *pat;
	size_t len;
	PatternAction *action;
};


/* exported global variable declarations */

/* exported function declarations */
struct pattern_data *new_pattern();
void free_pattern(struct pattern_data *m);
pcre2_code *compile_patterns(GList *patternlist);
void add_game_patterns(Endpoint *ep);
void add_client_patterns(Endpoint *ep);
void add_pattern(Endpoint *ep,char *pat, size_t size, PatternAction *handler);
void enable_matching(Endpoint *ep);
void disable_matching(Endpoint *ep);

PatternAction redact_match;
PatternAction remove_match;
PatternAction mnes_request;
PatternAction mnes_client_wont;
PatternAction mnes_does;
PatternAction respond_dont;
PatternAction respond_do;
PatternAction respond_wont;

int snprintf_mnes_pair(char *str, size_t size, char *var, char *val);
#endif /* MUDITM_HANDLERS_H */

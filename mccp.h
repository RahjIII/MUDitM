/* mccp.h - MUD In The Middle telnet-SSL proxy */
/* Created: Wed Feb 21 01:14:31 PM EST 2024 malakai */
/* $Id: mccp.h,v 1.1 2024/02/27 04:39:21 malakai Exp $ */

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

#ifndef MUDITM_MCCP_H
#define MUDITM_MCCP_H

#include "handlers.h"

/* global #defines */
#define TELOPT_MCCP  85
#define TELOPT_MCCP2 86
#define TELOPT_MCCP3 87

/* structs and typedefs */
typedef enum {
	MCCP_IGNORE,
	MCCP_DISABLE,
	MCCP_ENABLE,
	MCCP_MAX
} mccp_mode_t;

/* exported global variable declarations */

/* exported function declarations */
void configure_compression(Endpoint *ep,char *value);
void offer_compression(Endpoint *ep);
void add_mccp_game_patterns(Endpoint *ep);
void add_mccp_client_patterns(Endpoint *ep);
ssize_t write_endpoint_compressed(Endpoint *ep, void *buf, size_t count);
ssize_t read_endpoint_compressed(Endpoint *ep, void *buf, size_t count);

PatternAction mccp_ignore;
PatternAction mccp2_do;
PatternAction mccp2_dont;
PatternAction mccp2_sb_start;

#endif /* MUDITM_MCCP_H */

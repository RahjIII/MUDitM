/* iostats.h - per connection io statistics */
/* Created: Fri Feb 16 11:16:41 AM EST 2024 malakai */
/* $Id: iostats.h,v 1.1 2024/02/27 04:39:21 malakai Exp $ */

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

#ifndef LO_IOSTATS_H
#define LO_IOSTATS_H

/* global #defines */

/* structs and typedefs */

struct iostat_counter {
	struct timeval ts;
	long int in;
	long int out;
};

struct iostat_data {
	struct iostat_counter lifetime;
	struct iostat_counter checkpoint;
	struct iostat_counter rate;
};

/* exported global variable declarations */

/* exported function declarations */
struct iostat_data *iostat_new(void);
void iostat_free(struct iostat_data *ios);
void iostat_init(struct iostat_data *ios);
void iostat_incr(struct iostat_data *ios,int in, int out);
int iostat_printraw(char *buf, size_t len, struct iostat_data *ios);
int iostat_printhuman(char *buf, size_t len, struct iostat_data *ios);
int iostat_printhrate(char *buf, size_t len, struct iostat_data *ios);
void iostat_checkpoint(struct iostat_data *ios,double weight);

#endif /* LO_IOSTATS_H */

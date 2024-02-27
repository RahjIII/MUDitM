/* iobuf.h - quick and dirty contiguous buffer managment */
/* Created: Tue Mar  9 09:52:37 AM EST 2021 malakai */
/* $Id: iobuf.h,v 1.4 2024/02/27 04:39:21 malakai Exp $ */

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

#ifndef MUDITM_IOBUF_H
#define MUDITM_IOBUF_H

/* global #defines */
#define IP_BUFSIZE (1<<16)

/* structs and typedefs */
struct iobuf_data {
	char *head;
	size_t length;
	char *tail;
};

typedef struct iobuf_data Iobuf;

/* exported global variable declarations */

/* exported function declarations */
Iobuf *new_iobuf(size_t length);
void free_iobuf(Iobuf *iob);
char *tail_iobuf(Iobuf *iob);
char *head_iobuf(Iobuf *iob);
char *push_iobuf(Iobuf *iob,size_t len);
char *pop_iobuf(Iobuf *iob,size_t len);
char *popall_iobuf(Iobuf *iob);
size_t avail_iobuf(Iobuf *iob);
size_t len_iobuf(Iobuf *iob);


#endif /* MUDITM_IOBUF_H */

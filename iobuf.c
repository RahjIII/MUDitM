/* iobuf.c - quick and dirty contiguous buffer managment */
/* Created: Tue Mar  9 09:52:37 AM EST 2021 malakai */
/* $Id: iobuf.c,v 1.4 2024/02/27 04:39:21 malakai Exp $ */

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

#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "iobuf.h"

/* global #defines */

/* local structs and typedefs */

/* local global variable declarations */

/* local function declarations */

/* allocate and init a new iob. */
Iobuf *new_iobuf(size_t length) {
	Iobuf *iob;

	iob = (Iobuf *)malloc(sizeof(Iobuf));
	iob->head = (char *)malloc(sizeof(char) * length);
	iob->length = length;
	iob->tail = iob->head;
	return(iob);
}

/* deallocate and free an existing iob. */
void free_iobuf(Iobuf *iob) {
	if(iob) {
		if(iob->head) free(iob->head);
		free(iob);
	}
}

char *tail_iobuf(Iobuf *iob) {
	return(iob->tail);
}

char *head_iobuf(Iobuf *iob) {
	return(iob->head);
}


char *push_iobuf(Iobuf *iob,size_t len) {
	iob->tail += len;
	if( (iob->tail - iob->head) > iob->length) {
		/* if you push beyond the end of the iobuf, that's on you. The tail won't advance.*/
		iob->tail = iob->head + iob->length;
	}
	return(iob->tail);
}

char *pop_iobuf(Iobuf *iob,size_t len) {

	/* simple case, pop everything. */
	if(len >= (iob->tail - iob->head)) {
		iob->tail = iob->head;
		return(iob->tail);
	}
	
	memmove(iob->head,iob->head+len,(iob->tail - (iob->head+len)));
	iob->tail = iob->head + (iob->tail - (iob->head + len));

	return(iob->tail);
}

/* pop iobuf back to the begining */
char *popall_iobuf(Iobuf *iob) {
	iob->tail = iob->head;
	return(iob->tail);
}

size_t avail_iobuf(Iobuf *iob) {
	int available;
	available = iob->length - (iob->tail - iob->head);
	if(available < 0) {
		return(0);
	}
	return(available);
}

size_t len_iobuf(Iobuf *iob) {
	return(iob->tail - iob->head);
}


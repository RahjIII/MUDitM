/* debug.c - Debugging code for muditm */
/* Created: Wed Mar  3 11:09:27 PM EST 2021 malakai */
/* $Id: debug.c,v 1.6 2024/02/27 04:39:21 malakai Exp $*/

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

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <openssl/err.h>

#include "debug.h"

unsigned int global_debug_flag;

FILE *muditm_logfile;

void muditm_log_init(char *pathname) {
	FILE *out;

	muditm_logfile = stderr;
	if(pathname && *pathname) {
		if(! (out=fopen(pathname,"a"))) {
			muditm_log("Can't open log file %s: %s",pathname,strerror(errno));
			exit(EXIT_FAILURE);
		}
		muditm_logfile = out;
	}
}
	
void muditm_log(char *str, ...)
{
	va_list ap;
	long ct;
	char *tmstr;
	char vbuf[LOG_BUF_LEN];
	vbuf[0] = '\0';

	va_start(ap, str);
	vsnprintf(vbuf, sizeof(vbuf) - 1, str, ap);
	va_end(ap);

	ct = time(0);
	tmstr = asctime(localtime(&ct));
	*(tmstr + strlen(tmstr) - 1) = '\0';
	fprintf(muditm_logfile, "%s [%d] %s\n", tmstr, getpid(), vbuf);
	fflush(muditm_logfile);
}

int muditm_ssl_err_cb(const char *str, size_t len, void *u) {
	muditm_log("%s: %.*s",(char *)u,len-1,str);
	return(0);
}

void muditm_sslerr(char *str, ...)
{
	va_list ap;
	char vbuf[LOG_BUF_LEN];

	va_start(ap, str);
	vsnprintf(vbuf, sizeof(vbuf) - 1, str, ap);
	va_end(ap);

	ERR_print_errors_cb(muditm_ssl_err_cb,vbuf);
}

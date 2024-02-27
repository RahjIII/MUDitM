/* debug.h - Debugging code for muditm */
/* Created: Wed Mar  3 11:09:27 PM EST 2021 malakai */
/* $Id: debug.h,v 1.5 2024/02/27 04:39:21 malakai Exp $*/

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

#ifndef MUDITM_DEBUG_H
#define MUDITM_DEBUG_H

/* global #defines */
#define LOG_BUF_LEN 8192

#define muditm_debug(args...) { if(global_debug_flag) { muditm_log(args); } }

/* structs and typedefs */

/* exported global variable declarations */
extern unsigned int global_debug_flag;

/* exported function declarations */
void muditm_log_init(char *pathname);
void muditm_log(char *str, ...);
void muditm_sslerr(char *str, ...);

#endif /* MUDITM_DEBUG_H */

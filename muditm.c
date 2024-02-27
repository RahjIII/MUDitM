/* muditm.c - MUD In The Middle telnet-ssl proxy */
/* Created: Wed Mar  3 09:26:25 PM EST 2021 malakai */
/* $Id: muditm.c,v 1.14 2024/02/27 04:39:21 malakai Exp $ */

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
#include <getopt.h>
#include <glib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "debug.h"
#include "proxy.h"
#include "mccp.h"

#include "muditm.h"

#define CONFIG_FILE "/etc/muditm.conf"

void configure_context(SSL_CTX * ctx,char *cert, char *key, char *chain);
void log_endpoint_stats(Endpoint *ep);

char *muditm_proxy_name;

int new_mommie(int port) {
	
	int s; 
	int on = 1;
	struct sockaddr_in6 addr;
	struct linger ld;

	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(port);
	addr.sin6_flowinfo = 0;
	addr.sin6_addr = in6addr_any;
	addr.sin6_scope_id = 0;

	ld.l_onoff = 0;
	ld.l_linger = 1;

	s = socket(AF_INET6, SOCK_STREAM, 0);
	
	if(s<0) {
		muditm_log("Socket error: %s",strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on)) < 0) {
		muditm_log("Failed to set REUSEADDR: %s",strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (setsockopt (s, SOL_SOCKET, SO_LINGER,  &ld, sizeof(ld)) < 0) {
		muditm_log("Failed to set LINGER: %s",strerror(errno));
		exit(EXIT_FAILURE);
	}

	if(bind(s,(struct sockaddr*)&addr,sizeof(addr)) < 0) {
		muditm_log("The ties that bind are far too loose: %s",strerror(errno));
		exit(EXIT_FAILURE);
	}

	if(listen(s,1) < 0) {
		muditm_log("I said 'Now you listen to me...' and he said: %s",strerror(errno));
		exit(EXIT_FAILURE);
	}
	muditm_log("Listening on port %d.",port);


	return(s);
}


void configure_context(SSL_CTX * ctx,char *cert, char *key, char *chain)
{
	SSL_CTX_set_ecdh_auto(ctx, 1);

	/* Set the key and cert */
	if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0) {
		muditm_sslerr("using cert file");
		exit(EXIT_FAILURE);
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0) {
		muditm_sslerr("using private key");
		exit(EXIT_FAILURE);
	}

	if(*chain) {
		if (SSL_CTX_use_certificate_chain_file(ctx, chain) <= 0) {
			muditm_sslerr("using chain cert");
			exit(EXIT_FAILURE);
		}
	}

	if (SSL_CTX_set_default_verify_paths(ctx) <= 0) {
		muditm_sslerr("default paths");
		exit(EXIT_FAILURE);
	}

}

int game_connect(char *host, char *service) {
	
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int server_sock;
	int ret;

	/* get the possible addresses from the host and service port */
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;		/* doesn't matter, I can take IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;	/* tcp please */
	hints.ai_flags = 0;	
	hints.ai_protocol = 0;

	ret = getaddrinfo(host,service,&hints,&result);
	if(ret != 0) {
		muditm_log("Dingos ate my game server! %s",gai_strerror(ret));
		return(-1);
	}

	/* loop through the list until one of them works. */
	for(rp = result; rp != NULL; rp=rp->ai_next) {
		server_sock = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
		if(server_sock == -1) continue;
		if(connect(server_sock,rp->ai_addr,rp->ai_addrlen) == 0) {
			/* connected! */
			break;
		}
		close(server_sock);
	}
	freeaddrinfo(result);

	if( rp == NULL ) {
		muditm_log("Couldn't connect to game server, %s",strerror(errno));
		return(-1);
	}

	return(server_sock);
}

/* keyfile parsing simplified. */
char *get_conf_string(GKeyFile * gkf, gchar * group, gchar * key, gchar * def) {
	gchar *gs = NULL;

	if ((gs = g_key_file_get_string(gkf, group, key, NULL))) {
		return (gs);
	} else {
		return (strdup(def));
	}

}

/* keyfile parsing simplified. */
int get_conf_int(GKeyFile * gkf, gchar * group, gchar * key, int def) {
	gint gs;
	GError *error = NULL;

	gs = g_key_file_get_integer(gkf, group, key, &error);
	if (error == NULL) {
		return (gs);
	} else {
		return (def);
	}

}

/* keyfile parsing simplified. */
int get_conf_boolean(GKeyFile * gkf, gchar * group, gchar * key, int def) {
	gint gs;
	GError *error = NULL;

	gs = g_key_file_get_boolean(gkf, group, key, &error);
	if (error == NULL) {
		return (gs);
	} else {
		return (def);
	}

}

/* Does what it says on the tin. */
void zombie_killer(int s) {
	int saved_errno = errno;
	while(waitpid(-1,NULL,WNOHANG) > 0);
	errno = saved_errno;
}

int demonize(int mother_sock, int forking) {

	socklen_t addrlen;
	struct sockaddr_in6 addr;
	char addrstr[INET6_ADDRSTRLEN];
	int client_sock;
	struct sigaction sa;

	muditm_log("Accepting Client Connections.");

	if(forking) {
		sa.sa_handler = zombie_killer;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
		if(sigaction(SIGCHLD,&sa,NULL) == -1) {
			muditm_log("Failed to set the zombie_killer sigaction: %s",strerror(errno));
			return(-1);
		}
	}

	while(1) {

		addrlen = sizeof(addr);

		client_sock = accept(mother_sock,(struct sockaddr*)&addr,&addrlen);
		if(client_sock <0) {
			muditm_log("I can't accept that from the likes of you! %s",strerror(errno));
			return(-1);
		}

		if(forking && fork()) {
			close(client_sock);
			client_sock = -1;
			continue;
		} else {
			close(mother_sock);
			mother_sock = -1;
			break;
		}
	}

	inet_ntop(addr.sin6_family,&addr.sin6_addr,addrstr,sizeof(addrstr));
	muditm_log("Connect from %s",addrstr);
	return(client_sock);

}

char *get_proxy_name(void) {
	char buf[8192];
	sprintf(buf,"%s-%d.%d",
		MUDITM_SHORTNAME,
		MUDITM_MAJOR_VER,
		MUDITM_MINOR_VER
	);
	return(strdup(buf));
}

int main(int argc, char **argv)
{

	/* local variables. */
	char *configfilename = NULL;
	GKeyFile *gkf;
	int debug = 0;
	int mother_sock;
	SSL_CTX *ctx = NULL;
	Endpoint *client;
	Endpoint *game;
	int count;
	Iobuf *iob;

	/* configuration setable variables */
	int listening_port = 4143;  /* PARAM */
	char *client_security;
	char *client_compression;
	char *game_host;
	char *game_service;
	char *game_security;
	char *game_compression;
	char *cert_file;
	char *key_file;
	char *chain_file;
	char *log_file;
	int demon = 1;

	muditm_proxy_name = get_proxy_name();

	while(1) {
		char *short_options = "hc:dv";
		static struct option long_options[] = {
			{"help", no_argument,0,'h'},
			{"config",required_argument,0,'c'},
			{"debug", no_argument,0,'d'},
			{"version", no_argument,0,'v'},
			{NULL,no_argument,NULL,0}
		};
		int option_index = 0;
		int opt = getopt_long(argc,argv,short_options,long_options,&option_index);
		if(opt == -1) break;
		switch (opt) {
			case 'c':
				configfilename = optarg;
				break;
			case 'd':
				debug = 1;
				global_debug_flag = 1;
				break;
			case 'v':
				fprintf(stdout,"%s\n",muditm_proxy_name);
				exit(EXIT_SUCCESS);
			case 'h':
			default:
				fprintf(stdout,"Usage: %s [options]\n",argv[0]);
				fprintf(stdout,"\t-c / --config   : specify location of config file\n");
				fprintf(stdout,"\t-d / --debug    : run in debug mode\n");
				fprintf(stdout,"\t-h / --help     : this message\n");
				exit(EXIT_SUCCESS);
		}
	}

	if(configfilename == NULL) {
		configfilename = CONFIG_FILE;
	}
	gkf = g_key_file_new();

	if(!g_key_file_load_from_file(gkf,configfilename,G_KEY_FILE_NONE,NULL)){
		fprintf(stderr,"Couldn't read config file %s\n",CONFIG_FILE);
		exit(EXIT_FAILURE);
	}

	listening_port = get_conf_int(gkf,"muditm","listen",4143);
	demon = get_conf_boolean(gkf,"muditm","demon",1);
	client_security = get_conf_string(gkf,"client","security","none");
	game_security = get_conf_string(gkf,"game","security","none");
	game_host = get_conf_string(gkf,"game","host","::");
	game_service = get_conf_string(gkf,"game","service","4000");
	cert_file = get_conf_string(gkf,"ssl","cert","cert.pem");
	key_file = get_conf_string(gkf,"ssl","key","key.pem");
	chain_file = get_conf_string(gkf,"ssl","chain","");
	log_file = g_key_file_get_string(gkf, "muditm", "log-file", NULL);
	client_compression = get_conf_string(gkf,"client","compression","enable");
	game_compression = get_conf_string(gkf,"game","compression","enable");

	if(debug) {
		demon = 0;
	}

	muditm_log_init(log_file);

	muditm_log("Starting %s", muditm_proxy_name);

	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();
	/* start listening for the client end */
	mother_sock = new_mommie(listening_port);

	client = new_endpoint("Client");

	if( (client->socket = demonize(mother_sock,demon)) == -1) {
		goto cleanup_client;
	}

	/* load ssl data after the fork, so that each new client connect will read
	 * the keys again, in case they have been updated. */
	if( (!strcasecmp(client_security,"SSL")) ||
		(!strcasecmp(game_security,"SSL"))
	) {
		ctx = SSL_CTX_new(TLS_method());
		configure_context(ctx,cert_file,key_file,chain_file);
	}

	if(!strcasecmp(client_security,"SSL")) {
		if( ssl_start_endpoint(client, ctx,0) <= 0) {
			goto cleanup_client;
		} 
	}

	configure_compression(client,client_compression);

	/* open up the game end. */
	game = new_endpoint("Game");


	if ( (game->socket = game_connect(game_host,game_service)) == -1) {
		char reply[] = "Couldn't connect to server!\r\n";
		write_endpoint(client,reply,strlen(reply));
		goto cleanup_game;
	}
	
	if(!strcasecmp(game_security,"SSL")) {
		configure_context(ctx,cert_file,key_file,chain_file);
		if( ssl_start_endpoint(game, ctx,1) <= 0) {
			char reply[] = "Couldn't ssl to to server!\r\n";
			write_endpoint(client,reply,strlen(reply));
			goto cleanup_game;
		}
	}

	/* perhaps send the PROXY header. */
	if(get_conf_boolean(gkf,"muditm","stunnelproxy",0)) {
		iob = game->iobuf[EP_OUTPUT];
		count = stunnel_proxy_header1(client,tail_iobuf(iob),avail_iobuf(iob));
		muditm_log("Sent %.*s to %s",count-2,tail_iobuf(iob),game->name);
		push_iobuf(iob,count);
		flush_endpoint(game);
	}
	configure_compression(game,game_compression);

	/* start proxying */
	if (muditm_proxy(client,game,gkf) == -1) {
		muditm_log("Proxy ended abnormaly.");
	}

	/* LOG THE iostats here. */
	log_endpoint_stats(client);
	log_endpoint_stats(game);

	/*cleanup_game: */
	cleanup_game:
	free_endpoint(game);

	cleanup_client:
	free_endpoint(client);

	if(ctx) SSL_CTX_free(ctx);
	EVP_cleanup();

	free(muditm_proxy_name);

	muditm_log("Shutdown complete.");
}

void log_endpoint_stats(Endpoint *ep) {

	char buf[8192];
	char *s,*eos;

	s=buf;
	eos=s+sizeof(buf);

	s=buf;
	s += g_snprintf(s,eos-s,"%s sock ",ep->name);
	s += iostat_printhuman(s,eos-s, &(ep->sockstats));
	muditm_log("%s",buf);

	/*  show the raw mccp bytes in debug mode. */
	s=buf;
	s += g_snprintf(s,eos-s,"%s mccp ",ep->name);
	s += iostat_printhuman(s,eos-s, &(ep->mccpstats));
	muditm_debug("%s",buf);

	/* if there was compression, show it. */
	if( ep->mccpstats.lifetime.in >0 || ep->mccpstats.lifetime.out >0 ) {
		s=buf;
		s += g_snprintf(s,eos-s,"%s compression ratio ",ep->name);

		if( ep->mccpstats.lifetime.in > 0 ) {
			s += g_snprintf(s,eos-s,"%3.2f%% in ",
				(100.0 * (1.0 - ((double)ep->sockstats.lifetime.in / ep->mccpstats.lifetime.in)))
			);
		}
				
		if( ep->mccpstats.lifetime.out > 0) {
			s += g_snprintf(s,eos-s,"%3.2f%% out",
				(100.0 * (1.0 - ((double)ep->sockstats.lifetime.out / ep->mccpstats.lifetime.out)))
			);
		}
		muditm_log("%s",buf);
	}
}

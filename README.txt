                        MUDitM - MUD in the Middle Proxy

                ******* This is the MUDitM-1.0 release! *******
                        Mon Feb 26 10:36:47 PM EST 2024

MUDitM is a telnet proxy that I originally wrote on a whim over a long weekend
in 2021.  My intent was to try to provide an IPv6 and telnet-ssl front end to
standard IPv4/telnet Multi-User Dungeon type games that have not implemented
those networking features yet, whatever the reason.  MUDitM-0.2 had another
weekend's worth of polish on it, and MUDitM has served as the production SSL
frontend for the Last Outpost MUD since then.

MUDitM's implementation differs from other secure proxy implementations in that
it tries to report the proxied client address up to the game server via the
NEW-ENVIRON / MNES standard.  (See https://tintin.mudhalla.net/protocols/mnes/
for docs on MNES.)  I went for that method because my MUD, The Last Outpost,
already understands it.

Version 0.3 changed the scnprintf() calls for glib's compatible g_snprintf()
implementation, removing the need for an included scnprintf implementation.  It
also fixed a bug with non-ssl encrypted setups.  Version 0.4 had some minor
bugfixes around logging.

Version 1.0 adds the MCCP2 compression protocol on both the client and server
sides of the proxy, adding zlib compression for MUD's that haven't added it
yet.

MUDitM uses PCRE2 as its back end pattern matching engine, it allows
configuration of multiple ip address reporting variables, and I added the
stunnel PROXY announcement as an option for games that already support that.


Compilation, Installation
-------------------------

It'll compile and run under Debian 12 and compatible system.  You'll need to
install gcc, gnumake, ctags, libpcre2-dev, libglib2.0-dev, libssl-dev, openssl,
and zlib1g-dev.

See INSTALL file for the barest of documentation.  As of version 0.2, there is
an install option in the makefile.

The muditm.conf example configuration file also documents the various
configuration options in comments.

Installing it on Windows or Mac?  Le'me know how that goes.

What I've learned from this project:
------------------------------------

    1) At least for my mud, throwing an SSL socket directly into the game
    server itself using openssl wouldn't have been as involved as I thought it
    might be.  This proxy came together pretty quickly.

    2) There is a another ssl proxy project out there called `stunnel`
    (https://www.stunnel.org/) that I had looked at briefly, but did not try
    out because I didn't realize that it had its own way of sending the remote
    networking address info through the proxy.  It does, it is very
    straightforward, and acception that style of reporting would be pretty
    trivial to add to a mud server that doesn't already do NEW-ENVIRON. So, I
    added that PROXY reporting method to MUDitM for the sake of compatibility.

    3) PCRE2 looks kind of daunting from the documentation.  Its not.

    4) MCCP2 compression with zlib1g doesn't look daunting at all from the
    documentation.  It is.  Or at least, it is kind of a pain to get it right.
    Unlike the SSL library that provides a wrapper call on top of a regular
    socket for layering the protocol, zlib provides a sliding window buffer
    stream interface.  It took me a few iterations to get it working correctly
    under a variety of network situations.  I feel like adding IPv6 and SSL
    into an existing C/C++ MUD isn't that hard, but adding MCCP is.  If you are
    trying to add MCCP directly into your game, pay attention to the endpoint
    read/write commands in mccp.c.  They took me almost a day to get to where
    they are.

Bugs, Limitations, Todos
------------------------

In compression enable mode, MCCP and MCCP3 requests are intercepted and denied
by MUDitM, and are not transparently passed through.  Only MCCP2 is allowed.

In compression ignore mode, MUDitM uses a pattern to detect the start of MCCP2.
Once detected, ALL pcre2 patterns on both sides of the proxy are disabled.
NEW-ENVIRON/MNES will not be detected after compression starts while in ignore
mode.

If a client and server negotiatie MCCP or MCCP3 while in ignore mode, MUDitM
won't be able to detect the start of the compressed stream, and is likely to
hang during pattern matching on those binary streams.  Use of ignore mode is
discouraged for games that might agree to perform any MCCP compression.

Compression enabled and disabled can be set independently on either the client
or game sides of the proxy.  Compression ignore, if used at all, should be
enabled on BOTH sides of the proxy, or mayhem and lack of connectivity may
ensue.

You've got to be careful with adding patterns via pcre2.  Don't include sub
match expressions, or you are going to screw up my 'what just matched'
algorithm.  (see also: "ret-2" buried somewhere in proxy.c.)

I did a crummy job of handling write(). They aren't queued properly at all for
partial or zero writes.  Of course, with over two years of runtime front-ending
my game, it hasn't been an issue in production, so perhaps not so crummy after
all.

The IPADDRESS injection from MUDitM happens as soon as the server makes a
request for the full environment set.  If the client is also going to export
IPADDRESS, MUDitM does nothing to prevent that, and the client's export will
update the value seen on the game.  This is both good and bad- it is good,
because it lets a chain of proxies forward the first IPADDRESS along
unmolested.  It's bad in that it lets the client control the value of
IPADDRESS.  If you want to have access to the address that MUDitM is connected to
on the other side, export it using a different, less likely to be overwritten
variable name.  This is an area that could use improvement.

Contributing, Bug Reporting, Support
------------------------------------

You can contact the author (see the AUTHORS file) with your questions, bug
reports, or patches! The most up to date .tar.gz version of MUDitM can be found
at:

https://last-outpost.com/LO/pubcode

There is no "sourceforge" or "github" or "gitlab" or "slack" or "google code"
or "public svn" or other "open source repository" for this project. 

If you found MUDitM on one of those kind of repos, you can safely assume that
is a fork, and it has nothing to do with me, the original author.  I probably
won't be looking at the fork, so don't get mad when I don't respond to things
that have been posted to wherever that is.

Thanks
------

Thank you to the Multi User Dungeon #coding discord group for your help and
encouragement!



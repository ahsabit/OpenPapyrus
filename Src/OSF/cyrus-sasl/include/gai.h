/*
 * Mar  8, 2000 by Hajimu UMEMOTO <ume@mahoroba.org>
 *
 * This module is besed on ssh-1.2.27-IPv6-1.5 written by
 * KIKUCHI Takahiro <kick@kyoto.wide.ad.jp>
 */
// Copyright (c) 1998-2016 Carnegie Mellon University.  All rights reserved.
// 
/*
 * fake library for ssh
 *
 * This file is included in getaddrinfo.c and getnameinfo.c.
 * See getaddrinfo.c and getnameinfo.c.
 */

#ifndef _GAI_H_
#define _GAI_H_

#ifndef NI_MAXHOST
#define	NI_MAXHOST	1025
#endif
#ifndef NI_MAXSERV
#define	NI_MAXSERV	32
#endif

/* for old netdb.h */
#ifndef EAI_NODATA
#define EAI_NODATA	1
#define EAI_MEMORY	2
#define EAI_FAMILY	5	/* ai_family not supported */
#define EAI_SERVICE	9	/* servname not supported for ai_socktype */
#endif

/* dummy value for old netdb.h */
#ifndef AI_PASSIVE
#define AI_PASSIVE	1
#define AI_CANONNAME	2
struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_GETNAMEINFO
int	getnameinfo(const struct sockaddr *, socklen_t, char *,
		    size_t, char *, size_t, int);
#endif

#ifndef HAVE_GETADDRINFO
int	getaddrinfo(const char *, const char *,
		    const struct addrinfo *, struct addrinfo **);
void	freeaddrinfo(struct addrinfo *);
char	*gai_strerror(int);
#endif

#ifdef __cplusplus
}
#endif

#endif

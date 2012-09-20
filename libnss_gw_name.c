/*  
 *  Copyright © 2010,2012 Joachim Breitner
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  This file incorporates some code from the nss-mdns module,
 *  © 2004 Lennart Poettering.
 */


// #include <netlink/route/rtnl.h>
#include <netlink/socket.h>
#include <netlink/route/route.h>
#include <nss.h>
#include <netdb.h>
#include <errno.h>
#include <sys/socket.h>

typedef struct {
    uint32_t address;
} ipv4_address_t;


#define ALIGN(idx) do { \
  if (idx % sizeof(void*)) \
    idx += (sizeof(void*) - idx % sizeof(void*)); /* Align on 32 bit boundary */ \
} while(0)


static struct nl_addr *
find_gateway() {
	struct nl_cache* route_cache;
	struct nl_sock *sock;
	struct nl_object *obj;
	struct nl_addr *gw = NULL;
	int err;

	// Allocate a new netlink socket
	sock = nl_socket_alloc();

	err = nl_connect(sock, NETLINK_ROUTE);
	if (err) {
		nl_socket_free(sock);
		return NULL;
	}

	if (rtnl_route_alloc_cache(sock, AF_INET, 0, &route_cache)) {
		nl_close(sock);
		nl_socket_free(sock);
		return NULL;
	}

	for (obj = nl_cache_get_first(route_cache); obj; obj = nl_cache_get_next(obj)) {
		struct rtnl_route *route = (struct rtnl_route *)obj;

		// Ignore non ipv4 routes
		if (rtnl_route_get_family(route) != AF_INET) continue;

		// Find a default route
		if (nl_addr_get_prefixlen(rtnl_route_get_dst(route)) != 0) continue;

		// Assert a next hop
		if (rtnl_route_get_nnexthops(route) < 1) continue;
		
		// Found a gateway
		struct nl_addr *gw_ = rtnl_route_nh_get_gateway(rtnl_route_nexthop_n(route, 0));
		if (!gw_) continue;

		// Clone the address, as this one will be freed with the route cache (will it?)
		gw = nl_addr_clone(gw_);
		if (!gw) continue;

		break;

		//char buf[100];
		//printf("Addr: %s\n", nl_addr2str(dst,buf,100));
	}
	
	// Free the cache
	nl_cache_free(route_cache);

	// Close the socket first to release kernel memory
	nl_close(sock);

	// Finally destroy the netlink handle
	nl_socket_free(sock);

	return gw;
}

enum nss_status _nss_gw_name_gethostbyname_r (
	const char *name,
	struct hostent *result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop) {

	size_t idx, astart;
	struct nl_addr *gw;

	if (!strcmp(name,"gateway.localhost")) {
		// Look up gatway
		gw = find_gateway();
		if (!gw) {
			*errnop = EAGAIN;
			*h_errnop = NO_RECOVERY;
			return NSS_STATUS_TRYAGAIN;
		}

		if (buflen <
			sizeof(char*)+    /* alias names */
			strlen(name)+1+   /* main name */
			sizeof(ipv4_address_t)+ /* address */
			sizeof(char*)+ /* null address pointer */
			8 /* Alignment */
			)  {   /* official name */

			nl_addr_put(gw);

			*errnop = ERANGE;
			*h_errnop = NO_RECOVERY;
			return NSS_STATUS_TRYAGAIN;
		}
	

		/* Alias names */
		*((char**) buffer) = NULL;
		result->h_aliases = (char**) buffer;
		idx = sizeof(char*);

		/* Official name */
		strcpy(buffer+idx, name); 
		result->h_name = buffer+idx;
		idx += strlen(name)+1;
		ALIGN(idx);

		result->h_addrtype = AF_INET;
		result->h_length = sizeof(ipv4_address_t);

		astart = idx;
		memcpy(buffer+astart, nl_addr_get_binary_addr(gw), sizeof(ipv4_address_t));
		idx += sizeof(ipv4_address_t);
		
		result->h_addr_list = (char**)(buffer + idx);
		result->h_addr_list[0] = buffer + astart;
		result->h_addr_list[1] = NULL;

		nl_addr_put(gw);
		return NSS_STATUS_SUCCESS;
	}{
		*errnop = EINVAL;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_UNAVAIL;
	}
}

enum nss_status _nss_gw_name_gethostbyname2_r(
	const char *name,
	int af,
	struct hostent * result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop) {

	if (af != AF_INET) {
		*errnop = EAGAIN;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_TRYAGAIN;
	} else {
		return _nss_gw_name_gethostbyname_r(name, result, buffer, buflen, errnop, h_errnop);
	}
}


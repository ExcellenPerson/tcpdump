/*
 * Copyright (c) 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* \summary: Protocol Independent Multicast (PIM) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ip.h"
#include "ip6.h"
#include "ipproto.h"

#define PIMV1_TYPE_QUERY           0
#define PIMV1_TYPE_REGISTER        1
#define PIMV1_TYPE_REGISTER_STOP   2
#define PIMV1_TYPE_JOIN_PRUNE      3
#define PIMV1_TYPE_RP_REACHABILITY 4
#define PIMV1_TYPE_ASSERT          5
#define PIMV1_TYPE_GRAFT           6
#define PIMV1_TYPE_GRAFT_ACK       7

static const struct tok pimv1_type_str[] = {
	{ PIMV1_TYPE_QUERY,           "Query"         },
	{ PIMV1_TYPE_REGISTER,        "Register"      },
	{ PIMV1_TYPE_REGISTER_STOP,   "Register-Stop" },
	{ PIMV1_TYPE_JOIN_PRUNE,      "Join/Prune"    },
	{ PIMV1_TYPE_RP_REACHABILITY, "RP-reachable"  },
	{ PIMV1_TYPE_ASSERT,          "Assert"        },
	{ PIMV1_TYPE_GRAFT,           "Graft"         },
	{ PIMV1_TYPE_GRAFT_ACK,       "Graft-ACK"     },
	{ 0, NULL }
};

#define PIMV2_TYPE_HELLO         0
#define PIMV2_TYPE_REGISTER      1
#define PIMV2_TYPE_REGISTER_STOP 2
#define PIMV2_TYPE_JOIN_PRUNE    3
#define PIMV2_TYPE_BOOTSTRAP     4
#define PIMV2_TYPE_ASSERT        5
#define PIMV2_TYPE_GRAFT         6
#define PIMV2_TYPE_GRAFT_ACK     7
#define PIMV2_TYPE_CANDIDATE_RP  8
#define PIMV2_TYPE_PRUNE_REFRESH 9
#define PIMV2_TYPE_DF_ELECTION   10
#define PIMV2_TYPE_ECMP_REDIRECT 11

static const struct tok pimv2_type_values[] = {
    { PIMV2_TYPE_HELLO,         "Hello" },
    { PIMV2_TYPE_REGISTER,      "Register" },
    { PIMV2_TYPE_REGISTER_STOP, "Register Stop" },
    { PIMV2_TYPE_JOIN_PRUNE,    "Join / Prune" },
    { PIMV2_TYPE_BOOTSTRAP,     "Bootstrap" },
    { PIMV2_TYPE_ASSERT,        "Assert" },
    { PIMV2_TYPE_GRAFT,         "Graft" },
    { PIMV2_TYPE_GRAFT_ACK,     "Graft Acknowledgement" },
    { PIMV2_TYPE_CANDIDATE_RP,  "Candidate RP Advertisement" },
    { PIMV2_TYPE_PRUNE_REFRESH, "Prune Refresh" },
    { PIMV2_TYPE_DF_ELECTION,   "DF Election" },
    { PIMV2_TYPE_ECMP_REDIRECT, "ECMP Redirect" },
    { 0, NULL}
};

#define PIMV2_HELLO_OPTION_HOLDTIME             1
#define PIMV2_HELLO_OPTION_LANPRUNEDELAY        2
#define PIMV2_HELLO_OPTION_DR_PRIORITY_OLD     18
#define PIMV2_HELLO_OPTION_DR_PRIORITY         19
#define PIMV2_HELLO_OPTION_GENID               20
#define PIMV2_HELLO_OPTION_REFRESH_CAP         21
#define PIMV2_HELLO_OPTION_BIDIR_CAP           22
#define PIMV2_HELLO_OPTION_ADDRESS_LIST        24
#define PIMV2_HELLO_OPTION_ADDRESS_LIST_OLD 65001

static const struct tok pimv2_hello_option_values[] = {
    { PIMV2_HELLO_OPTION_HOLDTIME,         "Hold Time" },
    { PIMV2_HELLO_OPTION_LANPRUNEDELAY,    "LAN Prune Delay" },
    { PIMV2_HELLO_OPTION_DR_PRIORITY_OLD,  "DR Priority (Old)" },
    { PIMV2_HELLO_OPTION_DR_PRIORITY,      "DR Priority" },
    { PIMV2_HELLO_OPTION_GENID,            "Generation ID" },
    { PIMV2_HELLO_OPTION_REFRESH_CAP,      "State Refresh Capability" },
    { PIMV2_HELLO_OPTION_BIDIR_CAP,        "Bi-Directional Capability" },
    { PIMV2_HELLO_OPTION_ADDRESS_LIST,     "Address List" },
    { PIMV2_HELLO_OPTION_ADDRESS_LIST_OLD, "Address List (Old)" },
    { 0, NULL}
};

#define PIMV2_REGISTER_FLAG_LEN      4
#define PIMV2_REGISTER_FLAG_BORDER 0x80000000
#define PIMV2_REGISTER_FLAG_NULL   0x40000000

static const struct tok pimv2_register_flag_values[] = {
    { PIMV2_REGISTER_FLAG_BORDER, "Border" },
    { PIMV2_REGISTER_FLAG_NULL, "Null" },
    { 0, NULL}
};

/*
 * XXX: We consider a case where IPv6 is not ready yet for portability,
 * but PIM dependent defintions should be independent of IPv6...
 */

struct pim {
	nd_uint8_t	pim_typever;
			/* upper 4bit: PIM version number; 2 for PIMv2 */
			/* lower 4bit: the PIM message type, currently they are:
			 * Hello, Register, Register-Stop, Join/Prune,
			 * Bootstrap, Assert, Graft (PIM-DM only),
			 * Graft-Ack (PIM-DM only), C-RP-Adv
			 */
#define PIM_VER(x)	(((x) & 0xf0) >> 4)
#define PIM_TYPE(x)	((x) & 0x0f)
	nd_uint8_t	pim_rsv;	/* Reserved in v1, address length in v2 */
	nd_uint16_t	pim_cksum;	/* IP style check sum */
};

static void pimv2_print(netdissect_options *, const u_char *bp, u_int len, const u_char *);

static void
pimv1_join_prune_print(netdissect_options *ndo,
                       const u_char *bp, u_int len)
{
	u_int ngroups, njoin, nprune;
	u_int njp;

	/* If it's a single group and a single source, use 1-line output. */
	if (ND_TTEST_LEN(bp, 30) && EXTRACT_U_1(bp + 11) == 1 &&
	    ((njoin = EXTRACT_BE_U_2(bp + 20)) + EXTRACT_BE_U_2(bp + 22)) == 1) {
		u_int hold;

		ND_PRINT(" RPF %s ", ipaddr_string(ndo, bp));
		hold = EXTRACT_BE_U_2(bp + 6);
		if (hold != 180) {
			ND_PRINT("Hold ");
			unsigned_relts_print(ndo, hold);
		}
		ND_PRINT("%s (%s/%u, %s", njoin ? "Join" : "Prune",
		ipaddr_string(ndo, bp + 26), EXTRACT_U_1(bp + 25) & 0x3f,
		ipaddr_string(ndo, bp + 12));
		if (EXTRACT_BE_U_4(bp + 16) != 0xffffffff)
			ND_PRINT("/%s", ipaddr_string(ndo, bp + 16));
		ND_PRINT(") %s%s %s",
		    (EXTRACT_U_1(bp + 24) & 0x01) ? "Sparse" : "Dense",
		    (EXTRACT_U_1(bp + 25) & 0x80) ? " WC" : "",
		    (EXTRACT_U_1(bp + 25) & 0x40) ? "RP" : "SPT");
		return;
	}

	if (len < sizeof(nd_ipv4))
		goto trunc;
	ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
	if (ndo->ndo_vflag > 1)
		ND_PRINT("\n");
	ND_PRINT(" Upstream Nbr: %s", ipaddr_string(ndo, bp));
	bp += 4;
	len -= 4;
	if (len < 4)
		goto trunc;
	ND_TCHECK_2(bp + 2);
	if (ndo->ndo_vflag > 1)
		ND_PRINT("\n");
	ND_PRINT(" Hold time: ");
	unsigned_relts_print(ndo, EXTRACT_BE_U_2(bp + 2));
	if (ndo->ndo_vflag < 2)
		return;
	bp += 4;
	len -= 4;

	if (len < 4)
		goto trunc;
	ND_TCHECK_4(bp);
	ngroups = EXTRACT_U_1(bp + 3);
	bp += 4;
	len -= 4;
	while (ngroups != 0) {
		/*
		 * XXX - does the address have length "addrlen" and the
		 * mask length "maddrlen"?
		 */
		if (len < 4)
			goto trunc;
		ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
		ND_PRINT("\n\tGroup: %s", ipaddr_string(ndo, bp));
		bp += 4;
		len -= 4;
		if (len < 4)
			goto trunc;
		ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
		if (EXTRACT_BE_U_4(bp) != 0xffffffff)
			ND_PRINT("/%s", ipaddr_string(ndo, bp));
		bp += 4;
		len -= 4;
		if (len < 4)
			goto trunc;
		ND_TCHECK_4(bp);
		njoin = EXTRACT_BE_U_2(bp);
		nprune = EXTRACT_BE_U_2(bp + 2);
		ND_PRINT(" joined: %u pruned: %u", njoin, nprune);
		bp += 4;
		len -= 4;
		for (njp = 0; njp < (njoin + nprune); njp++) {
			const char *type;

			if (njp < njoin)
				type = "Join ";
			else
				type = "Prune";
			if (len < 6)
				goto trunc;
			ND_TCHECK_6(bp);
			ND_PRINT("\n\t%s %s%s%s%s/%u", type,
			    (EXTRACT_U_1(bp) & 0x01) ? "Sparse " : "Dense ",
			    (EXTRACT_U_1(bp + 1) & 0x80) ? "WC " : "",
			    (EXTRACT_U_1(bp + 1) & 0x40) ? "RP " : "SPT ",
			    ipaddr_string(ndo, bp + 2),
			    EXTRACT_U_1(bp + 1) & 0x3f);
			bp += 6;
			len -= 6;
		}
		ngroups--;
	}
	return;
trunc:
	ND_PRINT("[|pim]");
	return;
}

void
pimv1_print(netdissect_options *ndo,
            const u_char *bp, u_int len)
{
	u_char type;

	ND_TCHECK_1(bp + 1);
	type = EXTRACT_U_1(bp + 1);

	ND_PRINT(" %s", tok2str(pimv1_type_str, "[type %u]", type));
	switch (type) {
	case PIMV1_TYPE_QUERY:
		if (ND_TTEST_1(bp + 8)) {
			switch (EXTRACT_U_1(bp + 8) >> 4) {
			case 0:
				ND_PRINT(" Dense-mode");
				break;
			case 1:
				ND_PRINT(" Sparse-mode");
				break;
			case 2:
				ND_PRINT(" Sparse-Dense-mode");
				break;
			default:
				ND_PRINT(" mode-%u", EXTRACT_U_1(bp + 8) >> 4);
				break;
			}
		}
		if (ndo->ndo_vflag) {
			ND_TCHECK_2(bp + 10);
			ND_PRINT(" (Hold-time ");
			unsigned_relts_print(ndo, EXTRACT_BE_U_2(bp + 10));
			ND_PRINT(")");
		}
		break;

	case PIMV1_TYPE_REGISTER:
		ND_TCHECK_LEN(bp + 8, 20);			/* ip header */
		ND_PRINT(" for %s > %s", ipaddr_string(ndo, bp + 20),
			  ipaddr_string(ndo, bp + 24));
		break;
	case PIMV1_TYPE_REGISTER_STOP:
		ND_TCHECK_LEN(bp + 12, sizeof(nd_ipv4));
		ND_PRINT(" for %s > %s", ipaddr_string(ndo, bp + 8),
			  ipaddr_string(ndo, bp + 12));
		break;
	case PIMV1_TYPE_RP_REACHABILITY:
		if (ndo->ndo_vflag) {
			ND_TCHECK_2(bp + 22);
			ND_PRINT(" group %s", ipaddr_string(ndo, bp + 8));
			if (EXTRACT_BE_U_4(bp + 12) != 0xffffffff)
				ND_PRINT("/%s", ipaddr_string(ndo, bp + 12));
			ND_PRINT(" RP %s hold ", ipaddr_string(ndo, bp + 16));
			unsigned_relts_print(ndo, EXTRACT_BE_U_2(bp + 22));
		}
		break;
	case PIMV1_TYPE_ASSERT:
		ND_TCHECK_LEN(bp + 16, sizeof(nd_ipv4));
		ND_PRINT(" for %s > %s", ipaddr_string(ndo, bp + 16),
			  ipaddr_string(ndo, bp + 8));
		if (EXTRACT_BE_U_4(bp + 12) != 0xffffffff)
			ND_PRINT("/%s", ipaddr_string(ndo, bp + 12));
		ND_TCHECK_4(bp + 24);
		ND_PRINT(" %s pref %u metric %u",
		    (EXTRACT_U_1(bp + 20) & 0x80) ? "RP-tree" : "SPT",
		    EXTRACT_BE_U_4(bp + 20) & 0x7fffffff,
		    EXTRACT_BE_U_4(bp + 24));
		break;
	case PIMV1_TYPE_JOIN_PRUNE:
	case PIMV1_TYPE_GRAFT:
	case PIMV1_TYPE_GRAFT_ACK:
		if (ndo->ndo_vflag) {
			if (len < 8)
				goto trunc;
			pimv1_join_prune_print(ndo, bp + 8, len - 8);
		}
		break;
	}
	ND_TCHECK_1(bp + 4);
	if ((EXTRACT_U_1(bp + 4) >> 4) != 1)
		ND_PRINT(" [v%u]", EXTRACT_U_1(bp + 4) >> 4);
	return;

trunc:
	ND_PRINT("[|pim]");
	return;
}

/*
 * auto-RP is a cisco protocol, documented at
 * ftp://ftpeng.cisco.com/ipmulticast/specs/pim-autorp-spec01.txt
 *
 * This implements version 1+, dated Sept 9, 1998.
 */
void
cisco_autorp_print(netdissect_options *ndo,
                   const u_char *bp, u_int len)
{
	u_int type;
	u_int numrps;
	u_int hold;

	if (len < 8)
		goto trunc;
	ND_TCHECK_1(bp);
	ND_PRINT(" auto-rp ");
	type = EXTRACT_U_1(bp);
	switch (type) {
	case 0x11:
		ND_PRINT("candidate-advert");
		break;
	case 0x12:
		ND_PRINT("mapping");
		break;
	default:
		ND_PRINT("type-0x%02x", type);
		break;
	}

	ND_TCHECK_1(bp + 1);
	numrps = EXTRACT_U_1(bp + 1);

	ND_TCHECK_2(bp + 2);
	ND_PRINT(" Hold ");
	hold = EXTRACT_BE_U_2(bp + 2);
	if (hold)
		unsigned_relts_print(ndo, EXTRACT_BE_U_2(bp + 2));
	else
		ND_PRINT("FOREVER");

	/* Next 4 bytes are reserved. */

	bp += 8; len -= 8;

	/*XXX skip unless -v? */

	/*
	 * Rest of packet:
	 * numrps entries of the form:
	 * 32 bits: RP
	 * 6 bits: reserved
	 * 2 bits: PIM version supported, bit 0 is "supports v1", 1 is "v2".
	 * 8 bits: # of entries for this RP
	 * each entry: 7 bits: reserved, 1 bit: negative,
	 *	       8 bits: mask 32 bits: source
	 * lather, rinse, repeat.
	 */
	while (numrps != 0) {
		u_int nentries;
		char s;

		if (len < 4)
			goto trunc;
		ND_TCHECK_4(bp);
		ND_PRINT(" RP %s", ipaddr_string(ndo, bp));
		bp += 4;
		len -= 4;
		if (len < 1)
			goto trunc;
		ND_TCHECK_1(bp);
		switch (EXTRACT_U_1(bp) & 0x3) {
		case 0: ND_PRINT(" PIMv?");
			break;
		case 1:	ND_PRINT(" PIMv1");
			break;
		case 2:	ND_PRINT(" PIMv2");
			break;
		case 3:	ND_PRINT(" PIMv1+2");
			break;
		}
		if (EXTRACT_U_1(bp) & 0xfc)
			ND_PRINT(" [rsvd=0x%02x]", EXTRACT_U_1(bp) & 0xfc);
		bp += 1;
		len -= 1;
		if (len < 1)
			goto trunc;
		ND_TCHECK_1(bp);
		nentries = EXTRACT_U_1(bp);
		bp += 1;
		len -= 1;
		s = ' ';
		while (nentries != 0) {
			if (len < 6)
				goto trunc;
			ND_TCHECK_6(bp);
			ND_PRINT("%c%s%s/%u", s, EXTRACT_U_1(bp) & 1 ? "!" : "",
			          ipaddr_string(ndo, bp + 2), EXTRACT_U_1(bp + 1));
			if (EXTRACT_U_1(bp) & 0x02) {
				ND_PRINT(" bidir");
			}
			if (EXTRACT_U_1(bp) & 0xfc) {
				ND_PRINT("[rsvd=0x%02x]", EXTRACT_U_1(bp) & 0xfc);
			}
			s = ',';
			bp += 6; len -= 6;
			nentries--;
		}
		numrps--;
	}
	return;

trunc:
	ND_PRINT("[|autorp]");
	return;
}

void
pim_print(netdissect_options *ndo,
          const u_char *bp, u_int len, const u_char *bp2)
{
	const struct pim *pim = (const struct pim *)bp;
	uint8_t pim_typever;

#ifdef notyet			/* currently we see only version and type */
	ND_TCHECK_1(pim->pim_rsv);
#endif

	ND_TCHECK_1(pim->pim_typever);
	pim_typever = EXTRACT_U_1(pim->pim_typever);
	switch (PIM_VER(pim_typever)) {
	case 2:
		if (!ndo->ndo_vflag) {
			ND_PRINT("PIMv%u, %s, length %u",
			          PIM_VER(pim_typever),
			          tok2str(pimv2_type_values,"Unknown Type",PIM_TYPE(pim_typever)),
			          len);
			return;
		} else {
			ND_PRINT("PIMv%u, length %u\n\t%s",
			          PIM_VER(pim_typever),
			          len,
			          tok2str(pimv2_type_values,"Unknown Type",PIM_TYPE(pim_typever)));
			pimv2_print(ndo, bp, len, bp2);
		}
		break;
	default:
		ND_PRINT("PIMv%u, length %u",
		          PIM_VER(pim_typever),
		          len);
		break;
	}
	return;

trunc:
	ND_PRINT("[|pim]");
	return;
}

/*
 * PIMv2 uses encoded address representations.
 *
 * The last PIM-SM I-D before RFC2117 was published specified the
 * following representation for unicast addresses.  However, RFC2117
 * specified no encoding for unicast addresses with the unicast
 * address length specified in the header.  Therefore, we have to
 * guess which encoding is being used (Cisco's PIMv2 implementation
 * uses the non-RFC encoding).  RFC2117 turns a previously "Reserved"
 * field into a 'unicast-address-length-in-bytes' field.  We guess
 * that it's the draft encoding if this reserved field is zero.
 *
 * RFC2362 goes back to the encoded format, and calls the addr length
 * field "reserved" again.
 *
 * The first byte is the address family, from:
 *
 *    0    Reserved
 *    1    IP (IP version 4)
 *    2    IP6 (IP version 6)
 *    3    NSAP
 *    4    HDLC (8-bit multidrop)
 *    5    BBN 1822
 *    6    802 (includes all 802 media plus Ethernet "canonical format")
 *    7    E.163
 *    8    E.164 (SMDS, Frame Relay, ATM)
 *    9    F.69 (Telex)
 *   10    X.121 (X.25, Frame Relay)
 *   11    IPX
 *   12    Appletalk
 *   13    Decnet IV
 *   14    Banyan Vines
 *   15    E.164 with NSAP format subaddress
 *
 * In addition, the second byte is an "Encoding".  0 is the default
 * encoding for the address family, and no other encodings are currently
 * specified.
 *
 */

enum pimv2_addrtype {
	pimv2_unicast, pimv2_group, pimv2_source
};

/*  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Addr Family   | Encoding Type |     Unicast Address           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+++++++
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Addr Family   | Encoding Type |   Reserved    |  Mask Len     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                Group multicast Address                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Addr Family   | Encoding Type | Rsrvd   |S|W|R|  Mask Len     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                        Source Address                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
static int
pimv2_addr_print(netdissect_options *ndo,
                 const u_char *bp, u_int len, enum pimv2_addrtype at,
                 u_int addr_len, int silent)
{
	u_int af;
	int hdrlen;

	if (addr_len == 0) {
		if (len < 2)
			goto trunc;
		ND_TCHECK_1(bp + 1);
		switch (EXTRACT_U_1(bp)) {
		case 1:
			af = AF_INET;
			addr_len = (u_int)sizeof(nd_ipv4);
			break;
		case 2:
			af = AF_INET6;
			addr_len = (u_int)sizeof(nd_ipv6);
			break;
		default:
			return -1;
		}
		if (EXTRACT_U_1(bp + 1) != 0)
			return -1;
		hdrlen = 2;
	} else {
		switch (addr_len) {
		case sizeof(nd_ipv4):
			af = AF_INET;
			break;
		case sizeof(nd_ipv6):
			af = AF_INET6;
			break;
		default:
			return -1;
			break;
		}
		hdrlen = 0;
	}

	bp += hdrlen;
	len -= hdrlen;
	switch (at) {
	case pimv2_unicast:
		if (len < addr_len)
			goto trunc;
		ND_TCHECK_LEN(bp, addr_len);
		if (af == AF_INET) {
			if (!silent)
				ND_PRINT("%s", ipaddr_string(ndo, bp));
		}
		else if (af == AF_INET6) {
			if (!silent)
				ND_PRINT("%s", ip6addr_string(ndo, bp));
		}
		return hdrlen + addr_len;
	case pimv2_group:
	case pimv2_source:
		if (len < addr_len + 2)
			goto trunc;
		ND_TCHECK_LEN(bp, addr_len + 2);
		if (af == AF_INET) {
			if (!silent) {
				ND_PRINT("%s", ipaddr_string(ndo, bp + 2));
				if (EXTRACT_U_1(bp + 1) != 32)
					ND_PRINT("/%u", EXTRACT_U_1(bp + 1));
			}
		}
		else if (af == AF_INET6) {
			if (!silent) {
				ND_PRINT("%s", ip6addr_string(ndo, bp + 2));
				if (EXTRACT_U_1(bp + 1) != 128)
					ND_PRINT("/%u", EXTRACT_U_1(bp + 1));
			}
		}
		if (EXTRACT_U_1(bp) && !silent) {
			if (at == pimv2_group) {
				ND_PRINT("(0x%02x)", EXTRACT_U_1(bp));
			} else {
				ND_PRINT("(%s%s%s",
					EXTRACT_U_1(bp) & 0x04 ? "S" : "",
					EXTRACT_U_1(bp) & 0x02 ? "W" : "",
					EXTRACT_U_1(bp) & 0x01 ? "R" : "");
				if (EXTRACT_U_1(bp) & 0xf8) {
					ND_PRINT("+0x%02x", EXTRACT_U_1(bp) & 0xf8);
				}
				ND_PRINT(")");
			}
		}
		return hdrlen + 2 + addr_len;
	default:
		return -1;
	}
trunc:
	return -1;
}

enum checksum_status {
	CORRECT,
	INCORRECT,
	UNVERIFIED
};

static enum checksum_status
pimv2_check_checksum(netdissect_options *ndo, const u_char *bp,
		     const u_char *bp2, u_int len)
{
	const struct ip *ip;
	u_int cksum;

	if (!ND_TTEST_LEN(bp, len)) {
		/* We don't have all the data. */
		return (UNVERIFIED);
	}
	ip = (const struct ip *)bp2;
	if (IP_V(ip) == 4) {
		struct cksum_vec vec[1];

		vec[0].ptr = bp;
		vec[0].len = len;
		cksum = in_cksum(vec, 1);
		return (cksum ? INCORRECT : CORRECT);
	} else if (IP_V(ip) == 6) {
		const struct ip6_hdr *ip6;

		ip6 = (const struct ip6_hdr *)bp2;
		cksum = nextproto6_cksum(ndo, ip6, bp, len, len, IPPROTO_PIM);
		return (cksum ? INCORRECT : CORRECT);
	} else {
		return (UNVERIFIED);
	}
}

static void
pimv2_print(netdissect_options *ndo,
            const u_char *bp, u_int len, const u_char *bp2)
{
	const struct pim *pim = (const struct pim *)bp;
	int advance;
	enum checksum_status cksum_status;
	u_int pim_typever;
	u_int pimv2_addr_len;

	if (len < 2)
		goto trunc;
	ND_TCHECK_1(pim->pim_rsv);
	pim_typever = EXTRACT_U_1(pim->pim_typever);
	pimv2_addr_len = EXTRACT_U_1(pim->pim_rsv);
	if (pimv2_addr_len != 0)
		ND_PRINT(", RFC2117-encoding");

	if (len < 4)
		goto trunc;
	ND_TCHECK_2(pim->pim_cksum);
	ND_PRINT(", cksum 0x%04x ", EXTRACT_BE_U_2(pim->pim_cksum));
	if (EXTRACT_BE_U_2(pim->pim_cksum) == 0) {
		ND_PRINT("(unverified)");
	} else {
		if (PIM_TYPE(pim_typever) == PIMV2_TYPE_REGISTER) {
			/*
			 * The checksum only covers the packet header,
			 * not the encapsulated packet.
			 */
			cksum_status = pimv2_check_checksum(ndo, bp, bp2, 8);
			if (cksum_status == INCORRECT) {
				/*
				 * To quote RFC 4601, "For interoperability
				 * reasons, a message carrying a checksum
				 * calculated over the entire PIM Register
				 * message should also be accepted."
				 */
				cksum_status = pimv2_check_checksum(ndo, bp, bp2, len);
			}
		} else {
			/*
			 * The checksum covers the entire packet.
			 */
			cksum_status = pimv2_check_checksum(ndo, bp, bp2, len);
		}
		switch (cksum_status) {

		case CORRECT:
			ND_PRINT("(correct)");
			break;

		case INCORRECT:
			ND_PRINT("(incorrect)");
			break;

		case UNVERIFIED:
			ND_PRINT("(unverified)");
			break;
		}
	}
	bp += 4;
	len -= 4;

	switch (PIM_TYPE(pim_typever)) {
	case PIMV2_TYPE_HELLO:
	    {
		uint16_t otype, olen;
		while (len > 0) {
			if (len < 4)
				goto trunc;
			ND_TCHECK_4(bp);
			otype = EXTRACT_BE_U_2(bp);
			olen = EXTRACT_BE_U_2(bp + 2);
			ND_PRINT("\n\t  %s Option (%u), length %u, Value: ",
			          tok2str(pimv2_hello_option_values, "Unknown", otype),
			          otype,
			          olen);
			bp += 4;
			len -= 4;

			if (len < olen)
				goto trunc;
			ND_TCHECK_LEN(bp, olen);
			switch (otype) {
			case PIMV2_HELLO_OPTION_HOLDTIME:
				if (olen != 2) {
					ND_PRINT("ERROR: Option Length != 2 Bytes (%u)", olen);
				} else {
					unsigned_relts_print(ndo,
							     EXTRACT_BE_U_2(bp));
				}
				break;

			case PIMV2_HELLO_OPTION_LANPRUNEDELAY:
				if (olen != 4) {
					ND_PRINT("ERROR: Option Length != 4 Bytes (%u)", olen);
				} else {
					char t_bit;
					uint16_t lan_delay, override_interval;
					lan_delay = EXTRACT_BE_U_2(bp);
					override_interval = EXTRACT_BE_U_2(bp + 2);
					t_bit = (lan_delay & 0x8000)? 1 : 0;
					lan_delay &= ~0x8000;
					ND_PRINT("\n\t    T-bit=%u, LAN delay %ums, Override interval %ums",
					t_bit, lan_delay, override_interval);
				}
				break;

			case PIMV2_HELLO_OPTION_DR_PRIORITY_OLD:
			case PIMV2_HELLO_OPTION_DR_PRIORITY:
				switch (olen) {
				case 0:
					ND_PRINT("Bi-Directional Capability (Old)");
					break;
				case 4:
					ND_PRINT("%u", EXTRACT_BE_U_4(bp));
					break;
				default:
					ND_PRINT("ERROR: Option Length != 4 Bytes (%u)", olen);
					break;
				}
				break;

			case PIMV2_HELLO_OPTION_GENID:
				if (olen != 4) {
					ND_PRINT("ERROR: Option Length != 4 Bytes (%u)", olen);
				} else {
					ND_PRINT("0x%08x", EXTRACT_BE_U_4(bp));
				}
				break;

			case PIMV2_HELLO_OPTION_REFRESH_CAP:
				if (olen != 4) {
					ND_PRINT("ERROR: Option Length != 4 Bytes (%u)", olen);
				} else {
					ND_PRINT("v%u", EXTRACT_U_1(bp));
					if (EXTRACT_U_1(bp + 1) != 0) {
						ND_PRINT(", interval ");
						unsigned_relts_print(ndo,
								     EXTRACT_U_1(bp + 1));
					}
					if (EXTRACT_BE_U_2(bp + 2) != 0) {
						ND_PRINT(" ?0x%04x?", EXTRACT_BE_U_2(bp + 2));
					}
				}
				break;

			case  PIMV2_HELLO_OPTION_BIDIR_CAP:
				break;

			case PIMV2_HELLO_OPTION_ADDRESS_LIST_OLD:
			case PIMV2_HELLO_OPTION_ADDRESS_LIST:
				if (ndo->ndo_vflag > 1) {
					const u_char *ptr = bp;
					u_int plen = len;
					while (ptr < (bp+olen)) {
						ND_PRINT("\n\t    ");
						advance = pimv2_addr_print(ndo, ptr, plen, pimv2_unicast, pimv2_addr_len, 0);
						if (advance < 0)
							goto trunc;
						ptr += advance;
						plen -= advance;
					}
				}
				break;
			default:
				if (ndo->ndo_vflag <= 1)
					print_unknown_data(ndo, bp, "\n\t    ", olen);
				break;
			}
			/* do we want to see an additionally hexdump ? */
			if (ndo->ndo_vflag> 1)
				print_unknown_data(ndo, bp, "\n\t    ", olen);
			bp += olen;
			len -= olen;
		}
		break;
	    }

	case PIMV2_TYPE_REGISTER:
	{
		const struct ip *ip;

		if (len < 4)
			goto trunc;
		ND_TCHECK_LEN(bp, PIMV2_REGISTER_FLAG_LEN);

		ND_PRINT(", Flags [ %s ]\n\t",
		          tok2str(pimv2_register_flag_values,
		          "none",
		          EXTRACT_BE_U_4(bp)));

		bp += 4; len -= 4;
		/* encapsulated multicast packet */
		if (len == 0)
			goto trunc;
		ip = (const struct ip *)bp;
		ND_TCHECK_1(ip->ip_vhl);
		switch (IP_V(ip)) {
                case 0: /* Null header */
			ND_TCHECK_4(ip->ip_dst);
			ND_PRINT("IP-Null-header %s > %s",
			          ipaddr_string(ndo, ip->ip_src),
			          ipaddr_string(ndo, ip->ip_dst));
			break;

		case 4:	/* IPv4 */
			ip_print(ndo, bp, len);
			break;

		case 6:	/* IPv6 */
			ip6_print(ndo, bp, len);
			break;

		default:
			ND_PRINT("IP ver %u", IP_V(ip));
			break;
		}
		break;
	}

	case PIMV2_TYPE_REGISTER_STOP:
		ND_PRINT(" group=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance; len -= advance;
		ND_PRINT(" source=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance; len -= advance;
		break;

	case PIMV2_TYPE_JOIN_PRUNE:
	case PIMV2_TYPE_GRAFT:
	case PIMV2_TYPE_GRAFT_ACK:


        /*
         * 0                   1                   2                   3
         *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |PIM Ver| Type  | Addr length   |           Checksum            |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |             Unicast-Upstream Neighbor Address                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |  Reserved     | Num groups    |          Holdtime             |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |            Encoded-Multicast Group Address-1                  |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |   Number of Joined  Sources   |   Number of Pruned Sources    |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |               Encoded-Joined Source Address-1                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |                             .                                 |
         *  |                             .                                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |               Encoded-Joined Source Address-n                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |               Encoded-Pruned Source Address-1                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |                             .                                 |
         *  |                             .                                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |               Encoded-Pruned Source Address-n                 |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |                           .                                   |
         *  |                           .                                   |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |                Encoded-Multicast Group Address-n              |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */

	    {
		uint8_t ngroup;
		uint16_t holdtime;
		uint16_t njoin;
		uint16_t nprune;
		u_int i, j;

		if (PIM_TYPE(pim_typever) != 7) {	/*not for Graft-ACK*/
			ND_PRINT(", upstream-neighbor: ");
			if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
				goto trunc;
			bp += advance; len -= advance;
		}
		if (len < 4)
			goto trunc;
		ND_TCHECK_4(bp);
		ngroup = EXTRACT_U_1(bp + 1);
		holdtime = EXTRACT_BE_U_2(bp + 2);
		ND_PRINT("\n\t  %u group(s)", ngroup);
		if (PIM_TYPE(pim_typever) != 7) {	/*not for Graft-ACK*/
			ND_PRINT(", holdtime: ");
			if (holdtime == 0xffff)
				ND_PRINT("infinite");
			else
				unsigned_relts_print(ndo, holdtime);
		}
		bp += 4; len -= 4;
		for (i = 0; i < ngroup; i++) {
			ND_PRINT("\n\t    group #%u: ", i+1);
			if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
				goto trunc;
			bp += advance; len -= advance;
			if (len < 4)
				goto trunc;
			ND_TCHECK_4(bp);
			njoin = EXTRACT_BE_U_2(bp);
			nprune = EXTRACT_BE_U_2(bp + 2);
			ND_PRINT(", joined sources: %u, pruned sources: %u", njoin, nprune);
			bp += 4; len -= 4;
			for (j = 0; j < njoin; j++) {
				ND_PRINT("\n\t      joined source #%u: ", j+1);
				if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_source, pimv2_addr_len, 0)) < 0)
					goto trunc;
				bp += advance; len -= advance;
			}
			for (j = 0; j < nprune; j++) {
				ND_PRINT("\n\t      pruned source #%u: ", j+1);
				if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_source, pimv2_addr_len, 0)) < 0)
					goto trunc;
				bp += advance; len -= advance;
			}
		}
		break;
	    }

	case PIMV2_TYPE_BOOTSTRAP:
	{
		u_int i, j, frpcnt;

		/* Fragment Tag, Hash Mask len, and BSR-priority */
		if (len < 2)
			goto trunc;
		ND_TCHECK_2(bp);
		ND_PRINT(" tag=%x", EXTRACT_BE_U_2(bp));
		bp += 2;
		len -= 2;
		if (len < 1)
			goto trunc;
		ND_TCHECK_1(bp);
		ND_PRINT(" hashmlen=%u", EXTRACT_U_1(bp));
		if (len < 2)
			goto trunc;
		ND_TCHECK_1(bp + 2);
		ND_PRINT(" BSRprio=%u", EXTRACT_U_1(bp + 1));
		bp += 2;
		len -= 2;

		/* Encoded-Unicast-BSR-Address */
		ND_PRINT(" BSR=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;

		for (i = 0; len > 0; i++) {
			/* Encoded-Group Address */
			ND_PRINT(" (group%u: ", i);
			if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
				goto trunc;
			bp += advance;
			len -= advance;

			/* RP-Count, Frag RP-Cnt, and rsvd */
			if (len < 1)
				goto trunc;
			ND_TCHECK_1(bp);
			ND_PRINT(" RPcnt=%u", EXTRACT_U_1(bp));
			if (len < 2)
				goto trunc;
			ND_TCHECK_1(bp + 1);
			frpcnt = EXTRACT_U_1(bp + 1);
			ND_PRINT(" FRPcnt=%u", frpcnt);
			if (len < 4)
				goto trunc;
			bp += 4;
			len -= 4;

			for (j = 0; j < frpcnt && len > 0; j++) {
				/* each RP info */
				ND_PRINT(" RP%u=", j);
				if ((advance = pimv2_addr_print(ndo, bp, len,
								pimv2_unicast,
								pimv2_addr_len,
								0)) < 0)
					goto trunc;
				bp += advance;
				len -= advance;

				if (len < 2)
					goto trunc;
				ND_TCHECK_2(bp);
				ND_PRINT(",holdtime=");
				unsigned_relts_print(ndo,
						     EXTRACT_BE_U_2(bp));
				if (len < 3)
					goto trunc;
				ND_TCHECK_1(bp + 2);
				ND_PRINT(",prio=%u", EXTRACT_U_1(bp + 2));
				if (len < 4)
					goto trunc;
				bp += 4;
				len -= 4;
			}
			ND_PRINT(")");
		}
		break;
	}
	case PIMV2_TYPE_ASSERT:
		ND_PRINT(" group=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance; len -= advance;
		ND_PRINT(" src=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance; len -= advance;
		if (len < 8)
			goto trunc;
		ND_TCHECK_8(bp);
		if (EXTRACT_U_1(bp) & 0x80)
			ND_PRINT(" RPT");
		ND_PRINT(" pref=%u", EXTRACT_BE_U_4(bp) & 0x7fffffff);
		ND_PRINT(" metric=%u", EXTRACT_BE_U_4(bp + 4));
		break;

	case PIMV2_TYPE_CANDIDATE_RP:
	{
		u_int i, pfxcnt;

		/* Prefix-Cnt, Priority, and Holdtime */
		if (len < 1)
			goto trunc;
		ND_TCHECK_1(bp);
		ND_PRINT(" prefix-cnt=%u", EXTRACT_U_1(bp));
		pfxcnt = EXTRACT_U_1(bp);
		if (len < 2)
			goto trunc;
		ND_TCHECK_1(bp + 1);
		ND_PRINT(" prio=%u", EXTRACT_U_1(bp + 1));
		if (len < 4)
			goto trunc;
		ND_TCHECK_2(bp + 2);
		ND_PRINT(" holdtime=");
		unsigned_relts_print(ndo, EXTRACT_BE_U_2(bp + 2));
		bp += 4;
		len -= 4;

		/* Encoded-Unicast-RP-Address */
		ND_PRINT(" RP=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;

		/* Encoded-Group Addresses */
		for (i = 0; i < pfxcnt && len > 0; i++) {
			ND_PRINT(" Group%u=", i);
			if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
				goto trunc;
			bp += advance;
			len -= advance;
		}
		break;
	}

	case PIMV2_TYPE_PRUNE_REFRESH:
		ND_PRINT(" src=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;
		ND_PRINT(" grp=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_group, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;
		ND_PRINT(" forwarder=");
		if ((advance = pimv2_addr_print(ndo, bp, len, pimv2_unicast, pimv2_addr_len, 0)) < 0)
			goto trunc;
		bp += advance;
		len -= advance;
		if (len < 2)
			goto trunc;
		ND_TCHECK_2(bp);
		ND_PRINT(" TUNR ");
		unsigned_relts_print(ndo, EXTRACT_BE_U_2(bp));
		break;


	 default:
		ND_PRINT(" [type %u]", PIM_TYPE(pim_typever));
		break;
	}

	return;

trunc:
	ND_PRINT("[|pim]");
}

/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 8
 * End:
 */

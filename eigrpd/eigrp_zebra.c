/*
 * Zebra connect library for OSPFd
 * Copyright (C) 1997, 98, 99, 2000 Kunihiro Ishiguro, Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <zebra.h>

#include "thread.h"
#include "command.h"
#include "network.h"
#include "prefix.h"
#include "routemap.h"
#include "table.h"
#include "stream.h"
#include "memory.h"
#include "zclient.h"
#include "filter.h"
#include "plist.h"
#include "log.h"

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_network.h"

static int eigrp_interface_add (int , struct zclient *, zebra_size_t);
static int eigrp_interface_delete (int , struct zclient *,
                                   zebra_size_t );
static int
eigrp_interface_address_add (int, struct zclient *,
                            zebra_size_t);

/* Zebra structure to hold current status. */
struct zclient *zclient = NULL;

/* For registering threads. */
extern struct thread_master *master;
struct in_addr router_id_zebra;

/* Router-id update message from zebra. */
static int
eigrp_router_id_update_zebra (int command, struct zclient *zclient,
                             zebra_size_t length)
{
  struct eigrp *eigrp;
  struct prefix router_id;
  zebra_router_id_update_read(zclient->ibuf,&router_id);

  router_id_zebra = router_id.u.prefix4;

  eigrp = eigrp_lookup ();

  if (eigrp != NULL)
    eigrp_router_id_update (eigrp);

  return 0;
}


void
eigrp_zebra_init (void)
{
  zclient = zclient_new ();

  zclient_init (zclient, ZEBRA_ROUTE_EIGRP);
  zclient->router_id_update = eigrp_router_id_update_zebra; /* Not implemented */
  zclient->interface_add = eigrp_interface_add;/* Not implemented */
  zclient->interface_delete = eigrp_interface_delete;/* Not implemented */
//  zclient->interface_up = eigrp_interface_state_up;/* Not implemented */
//  zclient->interface_down = eigrp_interface_state_down;/* Not implemented */
  zclient->interface_address_add = eigrp_interface_address_add;/* Not implemented */
//  zclient->interface_address_delete = ospf_interface_address_delete;/* Not implemented */
//  zclient->ipv4_route_add = eigrp_zebra_read_ipv4;/* Not implemented */
//  zclient->ipv4_route_delete = eigrp_zebra_read_ipv4;/* Not implemented */
}

/* Inteface addition message from zebra. */
static int
eigrp_interface_add (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface *ifp;

  ifp = zebra_interface_add_read (zclient->ibuf);

  assert (ifp->info);

  if (!EIGRP_IF_PARAM_CONFIGURED (IF_DEF_PARAMS (ifp), type))
    {
      SET_IF_PARAM (IF_DEF_PARAMS (ifp), type);
      IF_DEF_PARAMS (ifp)->type = eigrp_default_iftype(ifp);
    }

  eigrp_if_update (NULL, ifp);

  return 0;
}

static int
eigrp_interface_delete (int command, struct zclient *zclient,
                       zebra_size_t length)
{
  struct interface *ifp;
  struct stream *s;
  struct route_node *rn;

  s = zclient->ibuf;
  /* zebra_interface_state_read() updates interface structure in iflist */
  ifp = zebra_interface_state_read (s);

  if (ifp == NULL)
    return 0;

  if (if_is_up (ifp))
    zlog_warn ("Zebra: got delete of %s, but interface is still up",
               ifp->name);

//  if (IS_DEBUG_OSPF (zebra, ZEBRA_INTERFACE))
//    zlog_debug
//      ("Zebra: interface delete %s index %d flags %llx metric %d mtu %d",
//       ifp->name, ifp->ifindex, (unsigned long long)ifp->flags, ifp->metric, ifp->mtu);

  for (rn = route_top (IF_OIFS (ifp)); rn; rn = route_next (rn))
    if (rn->info)
      eigrp_if_free ((struct eigrp_interface *) rn->info);

  ifp->ifindex = IFINDEX_INTERNAL;
  return 0;
}

static int
eigrp_interface_address_add (int command, struct zclient *zclient,
                            zebra_size_t length)
{
  struct connected *c;

  c = zebra_interface_address_read (command, zclient->ibuf);

  if (c == NULL)
    return 0;

//  if (IS_DEBUG_OSPF (zebra, ZEBRA_INTERFACE))
//    {
//      char buf[128];
//      prefix2str(c->address, buf, sizeof(buf));
//      zlog_debug("Zebra: interface %s address add %s", c->ifp->name, buf);
//    }

  eigrp_if_update (NULL, c->ifp);

  return 0;
}
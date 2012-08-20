#! /usr/bin/env python

import sys
import re

# Input Files
system_map_file   = sys.argv[1]
interconnect_file = sys.argv[2]
in_routes_file    = sys.argv[3]

# Output Files
topomap_file    = 'topomap.txt'
out_routes_file = 'routes.txt'

# Global Data Structures
nid2xyz  = {}  # Maps NID          -> (x,y,z) coordinate
nid2node = {}  # Maps NID          -> Node_CNAME   (e.g., c0-0c0s0n0)
nid2gem  = {}  # Maps NID          -> Gemini_CNAME (e.g., c0-0c0s0g0)
nid2vid  = {}  # Maps NID          -> Vertex ID
vid2nid  = {}  # Maps Vertex ID    -> NID
node2nid = {}  # Maps Node_CNAME   -> NID
gem2nids = {}  # Maps Gemini_CNAME -> the 2 NIDs attached to it
gem2xyz  = {}  # Maps Gemini_CNAME -> (x,y,z) coordinate
gem2vid  = {}  # Maps Gemini_CNAME -> Vertex ID
gid2gem  = {}  # Maps Gemini_ID    -> Gemini_CNAME
gid2vid  = {}  # Maps Gemini_ID    -> Vertex ID
xyz2gem  = {}  # Maps (x,y,z)      -> Gemini_CNAME
vid2name = []  # Maps Vertex ID    -> CNAME
vid2adj  = []  # Maps Vertex ID    -> Adjacency List
               #                      (the other VIDs the VID is connected to)
x_max    = -1  # Maximum coordinate in x dimension
y_max    = -1  # Maximum coordinate in y dimension
z_max    = -1  # Maximum coordinate in z dimension
non_mins = []  # Non minimal routes. Each entry is a string.

def xyz_str(x, y, z):
    """Takes x, y, and z integer coordinates and returns (x,y,z) string"""
    return '(' + x + ',' + y + ',' + z + ')'

def vid_is_host(vid):
    """Returns true if the vertex ID passed in represents a Host"""
    return (vid < len(nid2node))

def vid_is_gemini(vid):
    """Returns true if the vertex ID passed in represents a Gemini"""
    return (vid >= len(nid2node))

def calc_edge_weight(links):
    """Calculates and returns the edge weight to use for a given link"""
    wt = 0  # wt stores the total link bandwidth in Gbytes/s
    for link_type,count in sorted(links.iteritems()):
        bw = 0  # bw stores the link bandwidth of the current link_type
        if link_type == 'host':
            # The host link is a 16 bit interface running at 5.2 gbits/s.
            # Peak per direction bandwidth is 10.4 GBytes/s.
            # Effective BW after overhead is probably somewhere
            # around 8 GBytes/s, but use peak anyway.
            bw = 10.4
        elif link_type == 'mezzanine':
            # Per Cray, mezzanine links on all Cray XE and XK systems run at
            # 6.25 gbits/s. Each of the links is 3-bits wide.
            bw = 6.25 * 3 * count / 8
        elif link_type == 'backplane':
            # Per Cray, backplane links on all Cray XE and XK systems run at
            # 5.0 gbits/s. Each of the links is 3-bits wide.
            bw = 5.0 * 3 * count / 8
        elif re.search('cable', link_type) != None: 
            # Per Cray, cable links on all Cray XE and XK systems run at
            # 3.125 gbits/s. Each of the links is 3-bits wide.
            bw = 3.125 * 3 * count / 8
        else:
            raise NameError('unknown link type')
        wt += bw
    # Return the calculated edge weight (link GBytes/s)
    # Scale by 10000 to convert to integer... bad hack
    return int(wt * 10000)

def calc_edge_string(links):
    """Generates a string representing the edge's link type"""
    # Generates a string like: 'link_type=count, link_type=count, ...'
    string = ''
    nlinks = len(links)
    for link_type,count in sorted(links.iteritems()):
        string += link_type + '=' + str(count)
        nlinks -= 1
        if nlinks != 0:
            string += ','
    return string

def is_minimal(ros):
    """Returns true if the input route order string is a minimal route"""

    # First, verify that we only move in each direction once and in order
    if not re.match('^[X|x]*[Y|y]*[Z|z]*$', ros):
        return False

    # Next, verify that the route is minimal
    x_hops = y_hops = z_hops = 0

    x_run = re.findall('[X|x]+', ros)
    if len(x_run) == 1:
        x_hops = len(x_run[0])
    elif len(x_run) > 1:
        raise NameError('x_run list has more than 1 entry')

    if x_hops > (x_max+1)/2:
        return False

    y_run = re.findall('[Y|y]+', ros)
    if len(y_run) == 1:
        y_hops = len(y_run[0])
    elif len(y_run) > 1:
        raise NameError('y_run list has more than 1 entry')

    if y_hops > (y_max+1)/2:
        return False

    z_run = re.findall('[Z|z]+', ros)
    if len(z_run) == 1:
        z_hops = len(z_run[0])
    elif len(z_run) > 1:
        raise NameError('z_run list has more than 1 entry')

    if z_hops > (z_max+1)/2:
        return False

    return True


# Step 1: Read the System Map input file.
#         This file has one entry per node (host), and defines
#         which gemini each node is connected to and its (x,y,z)
#         coordinate.
smf = open(system_map_file, 'r')
smf.readline()  # First line is column headers
smf.readline()  # Second line is header
for line in smf:
    fields = re.split('\s+', line)

    nid  = int(fields[0])
    node = fields[2]
    gem  = fields[3]
    xyz  = xyz_str(fields[4], fields[5], fields[6])
    
    nid2xyz[nid]   = xyz
    nid2node[nid]  = node
    nid2gem[nid]   = gem
    node2nid[node] = nid
    gem2xyz[gem]   = xyz
    xyz2gem[xyz]   = gem

    if gem not in gem2nids:
        gem2nids[gem] = [nid]
    else:
        gem2nids[gem].append(nid)

    # Assign each gemini the same ID as the even NID attached to it
    if nid % 2 == 0:
        gid2gem[nid] = gem

    # Keep track of maximum x, y, and z coordinates
    if fields[4] > x_max:
        x_max = int(fields[4])
    if fields[5] > y_max:
        y_max = int(fields[5])
    if fields[6] > z_max:
        z_max = int(fields[6])


# Step 2: Assign a Vertex ID (VID) to each host and gemini.
#         VID space will be constructed to list all hosts first,
#         followed by all geminis.
for nid, name in sorted(nid2node.iteritems()):
    vid2name.append(name)
    vid2adj.append({})
    vid = len(vid2name) - 1
    nid2vid[nid] = vid
    vid2nid[vid] = nid
for gid, name in sorted(gid2gem.iteritems()):
    vid2name.append(name)
    vid2adj.append({})
    vid = len(vid2name) - 1
    gid2vid[gid] = vid
    gem2vid[name] = vid


# Step 3: Read the Interconnects input file.
#         This file specifies the links between the geminis,
#         as well as the type of each link.
icf = open(interconnect_file, 'r')
for line in icf:
    fields    = re.split('\s+', line)
    sfields   = re.split('\[|\]', fields[0])
    dfields   = re.split('\[|\]', fields[3])
    src_gem   = (sfields[0])[:-3]
    src_xyz   = sfields[1]
    dst_gem   = (dfields[0])[:-3]
    dst_xyz   = dfields[1]
    link_type = fields[5]

    # Sanity check.
    # Make sure src and dst gemini xyz coordinates match what we already saw.
    if (gem2xyz[src_gem] != src_xyz) or (gem2xyz[dst_gem] != dst_xyz):
        raise NameError('(x,y,z) coordinates do not match')

    src_vid = gem2vid[src_gem]
    dst_vid = gem2vid[dst_gem]

    # Add the edge
    edges = vid2adj[src_vid]
    if dst_vid not in edges:
        edges[dst_vid] = {}
    if link_type not in edges[dst_vid]:
        (edges[dst_vid])[link_type] = 1
    else:
        (edges[dst_vid])[link_type] += 1


# Step 4: Add in gemini <-> host edges
#         Each gemini chip contains two network interfaces,
#         each connected to a different host.
for vid in range(len(vid2adj)):
    edges = vid2adj[vid]
    if vid_is_gemini(vid):
        gem = vid2name[vid]
        for host_nid in gem2nids[gem]:
            host_vid = nid2vid[host_nid]
            if host_vid not in edges:
                edges[host_vid] = {}
            (edges[host_vid])['host'] = 1
    elif vid_is_host(vid):
        gem_cname = nid2gem[vid]
        gem_vid   = gem2vid[gem_cname]
        if gem_vid not in edges:
            edges[gem_vid] = {}
        (edges[gem_vid])['host'] = 1


# Step 5: Output the libtopomap System Topology input file.
#         The first part of this file lists each vertex ID
#         and its human readable name. The second part of the
#         file lists the adjacency list for each vertex ID,
#         with edge weights.
tf = open(topomap_file, 'w')
print >>tf, 'num: ' + str(len(vid2name)) + 'w'
for vid in range(len(vid2name)):
    print >>tf, vid, vid2name[vid]
for vid in range(len(vid2adj)):
    print >>tf, vid,
    for edge,links in sorted(vid2adj[vid].iteritems()):
         wt = calc_edge_weight(links)
         print >>tf, str(edge) + '(' + str(wt) + ')',
    print >>tf, ''
tf.close()


# Step 6: Output the frst portion of the libtopomap routing input file.
#         The first portion of the file contains an entry for each
#         host vertex ID (i.e., non-Gemini) along with the host's
#         x, y, z coordinate in the Gemini network.
orf = open(out_routes_file, 'w')
print >>orf, 'type: COMP3DTORUSXYZ'
for vid in range(len(nid2node)):
    nid    = vid2nid[vid]
    xyz    = nid2xyz[nid]
    fields = re.split('\(|\)|\,', xyz)
    print >>orf, vid, fields[1], fields[2], fields[3]


# Step 7: Parse the routes file, looking for exceptions to minimal routing.
irf = open(in_routes_file, 'r')
num_routes = 0
for line in irf:
    num_routes += 1

    # Print out a progress indication...
    # This script can take a long time for big systems.
    if num_routes % 100000 == 0:
        expected_routes = len(gem2vid) * len(gem2vid)
        print 'Processed', num_routes, 'lines from routes file,', expected_routes, 'expected'

    # Build the route order string from source Gemini to destination Gemini
    hops = re.findall('\([0-9]+,[0-9]+,[0-9]+\)', line)
    ros  = ''  # starts out empty, will tack on one character at a time
    prev = hops[0]
    for curr in hops[1: ]:
        prev_fields = re.split('\(|\)|\,', prev)
        curr_fields = re.split('\(|\)|\,', curr)

        x_delta    = int(curr_fields[1]) - int(prev_fields[1])
        y_delta    = int(curr_fields[2]) - int(prev_fields[2])
        z_delta    = int(curr_fields[3]) - int(prev_fields[3])

        if x_delta != 0 and y_delta == 0 and z_delta == 0:
            if x_delta == 1:
                ros += 'X'
            elif x_delta == -1:
                ros += 'x'
            elif x_delta > 1:
                ros += 'x'
            elif x_delta < -1:
                ros += 'X'
        elif x_delta == 0 and y_delta != 0 and z_delta == 0:
            if y_delta == 1:
                ros += 'Y'
            elif y_delta == -1:
                ros += 'y'
            elif y_delta > 1:
                ros += 'y'
            elif y_delta < -1:
                ros += 'Y'
        elif x_delta == 0 and y_delta == 0 and z_delta != 0:
            if z_delta == 1:
                ros += 'Z'
            elif z_delta == -1:
                ros += 'z'
            elif z_delta > 1:
                ros += 'z'
            elif z_delta < -1:
                ros += 'Z'
        else:
            print prev, curr, x_delta, y_delta, z_delta
            raise NameError('Bogus route detected.')

        prev = curr

    if is_minimal(ros) == False:
        print 'Found non-minimal route:', ros

        # Store the non-minimal route, will be output in next step
        src_xyz     = hops[0];
        dst_xyz     = hops[-1];
        src_gem     = xyz2gem[src_xyz]
        dst_gem     = xyz2gem[dst_xyz]
        src_gem_vid = gem2vid[src_gem]
        dst_gem_vid = gem2vid[dst_gem]
        src_nids    = gem2nids[src_gem]
        dst_nids    = gem2nids[dst_gem]
        for src_nid in src_nids:
            for dst_nid in dst_nids:
                src_vid = nid2vid[src_nid]
                dst_vid = nid2vid[dst_nid]
                route = str(src_vid) + ' ' + str(dst_vid)
                if src_vid != dst_vid:
                    for hop_xyz in hops:
                        hop_gem     = xyz2gem[hop_xyz]
                        hop_gem_vid = gem2vid[hop_gem]
                        route = route + ' ' + str(hop_gem_vid)
                route = route + ' ' + str(dst_vid)
                non_mins.append(route)


# Sanity check:
# The number of routes in the logical routes file must equal num_geminis^2
if num_routes != len(gem2vid) * len(gem2vid):
    raise NameError('Wrong number of lines in logical-routes input file.')


# Step 8: Output the second portion of the libtopomap routing input file.
#         The second portion contains just the routes that are non-minimal.
#         All other routes are assumed to be minimal and are computed
#         algorithmically by libtopomap.
print >>orf, 'num:', len(non_mins)
for route in non_mins:
    print >>orf, route


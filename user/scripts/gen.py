#! /usr/bin/env python

# Usage: gen.py system-map.txt interconnect.txt logical-routes.txt
# Outputs: topomap.txt and routes.txt in libtopomap format

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


# Step 2: Assign a Vertex ID (VID) to each host and gemini.
#         VID space will be constructed to list all hosts first,
#         followed by all geminis.
for nid, name in sorted(nid2node.iteritems()):
    vid2name.append(name)
    vid2adj.append({})
    vid = len(vid2name) - 1
    nid2vid[nid] = vid
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


# Step 6: Read the Routes input file, and output routes file at same time.
#         The Routes input file specifies the route from each source gemini
#         to each destination gemini. Libtopomap wants the routes from each
#         source node to each destination node, so the output routes file
#         will be four times larger than the input routes file.
irf = open(in_routes_file, 'r')
orf = open(out_routes_file, 'w')
for line in irf:
    hops = re.findall('\([0-9]+,[0-9]+,[0-9]+\)', line)
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
            print >>orf, src_vid, dst_vid,
            if src_vid != dst_vid:
                for hop_xyz in hops:
                    hop_gem     = xyz2gem[hop_xyz]
                    hop_gem_vid = gem2vid[hop_gem]
                    print >>orf, hop_gem_vid,
            print >>orf, dst_vid


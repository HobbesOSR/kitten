#! /usr/bin/env python

# Usage: check.py topomap.txt routes.txt system-map.txt
# Outputs: various stats to stdout
#
# Uncomment last few lines to output topomap.txt format
# file with the number of routes used by each edge as
# the edge weights.

import sys
import re


# Input Files
topomap_file    = sys.argv[1]
routes_file     = sys.argv[2]
system_map_file = sys.argv[3]


# Global Data Structures
gemini_vids      = {}    # Hash table containing Gemini vertex IDs, val=Gemini_CNAME
node_vids        = {}    # Hash table containing node vertex IDs, val=Host_CNAME
gem2xyz          = {}    # Maps Gemini_CNAME -> (x,y,z) coordinate
neighbors_hist   = {}    # Histogram: key=num_neighbors, val=vertex_count
linkspeed_hist   = {}    # Histogram: key=edge_weight,   val=edge_count
xlinkspeed_hist  = {}    # Histogram: key=edge_weight,   val=edge_count  (only Gemini-to-Gemini X links)
ylinkspeed_hist  = {}    # Histogram: key=edge_weight,   val=edge_count  (only Gemini-to-Gemini Y links)
zlinkspeed_hist  = {}    # Histogram: key=edge_weight,   val=edge_count  (only Gemini-to-Gemini Z links)
xyz_hist         = {}    # Histogram: key=xyz_delta,     val=num_edges
hopcount_hist    = {}    # Histogram: key=hop_count,     val=route_count
route_order_hist = {}    # Histogram: key=route_order,   val=route_count (example order: xyz, yxyz, ...)
route_count_hist = {}    # Histogram: key=route_count,   val=edge_count
machine_graph    = []
is_weighted      = False
num_vertices     = 0
num_edges        = 0
num_routes       = 0
num_nodes        = 0
num_geminis      = 0
num_n2g_edges    = 0     # Node to Gemini Edges
num_g2n_edges    = 0     # Gemini to Node Edges
num_g2g_edges    = 0     # Gemini to Gemini Edges


def xyz_str(x, y, z):
    """Takes x, y, and z integer coordinates and returns (x,y,z) string"""
    return '(' + x + ',' + y + ',' + z + ')'


tmf = open(topomap_file, 'r')
rf  = open(routes_file, 'r')
smf = open(system_map_file, 'r')


# Parse the system map file to get the x,y,z coordinate of each Gemini
smf.readline()  # First line is column headers
smf.readline()  # Second line is header
for line in smf:
    fields = re.split('\s+', line)
    gem  = fields[3]
    xyz  = xyz_str(fields[4], fields[5], fields[6])
    gem2xyz[gem] = xyz


# Parse the topomap file to get the graph representation of the machine.
# Vertices are hosts or Geminis.
# Edges connect hosts to Geminis and Geminis to Geminis.
#
# First line has format:
#
#    num: 1234w
#
# Where 1234 is the number of vertices defined in the file
# and the w signifies that edge weights are included.  If
# there are no edge weights, the w is omitted.
line = (tmf.readline()).rstrip('\r\n')
if line[-1] == 'w':
    num_vertices = int(line[5:-1])
    is_weighted = True
else:
    num_vertices = int(line[5:])
    is_weighted = False


# Next set of num_vertices lines defines a human readable name for each vertex.
# This is just a sanity check.
expected_sum = 0
actual_sum   = 0

for i in range(num_vertices):
    expected_sum = expected_sum + i

for i in range(num_vertices):
    line = tmf.readline()
    fields = re.split('\s+', line)
    id = int(fields[0])
    actual_sum += id
    if fields[1][-2:-1] == 'n':
        num_nodes += 1
        node_vids[id] = fields[1]
    elif fields[1][-2:-1] == 'g':
        num_geminis += 1
        gemini_vids[id] = fields[1]

if actual_sum != expected_sum:
    raise NameError('Vertex List Parse: Actual ID sum not equal to expected ID sum.')

if num_vertices != num_nodes + num_geminis:
    raise NameError('Wrong number of nodes or geminis detected.')


# Next set of num_vertices lines defines the adjacency list for each vertex.
# Keep a histogram relating the number of neighbors to the number of nodes.
# Keep a histogram relating link speed to the number of links of that speed.
# Keep a histogram relating Gemini-to-Gemini edge (x,y,z) delta to edge count.
actual_sum = 0
num_edges  = 0
for i in range(num_vertices):
    line = tmf.readline().rstrip('\r\n ')
    fields = re.split('\s+', line)
    src_vid = int(fields[0])
    actual_sum += src_vid
    num_neighbors = len(fields) - 1
    num_edges += num_neighbors

    # Update the neighbor histogram
    if num_neighbors in neighbors_hist:
        neighbors_hist[num_neighbors] += 1
    else:
        neighbors_hist[num_neighbors] = 1

    # Update machine graph
    # Each source vertex (src_vid) has its own hash table with:
    #    key = destination vertex ID (dst_vid)
    #    val = vector of values:
    #             index 0 = edge weight
    #             index 1 = number of routes using the edge
    machine_graph.append({})

    # Parse the edge list
    for field in fields[1:]:
        fields2 = re.split('\(|\)', field)
        dst_vid = int(fields2[0])
        linkspeed = int(fields2[1])

        # Store the linkspeed in the machine graph
        # See comment above for info on machine_graph data structure
        machine_graph[src_vid][dst_vid] = []
        machine_graph[src_vid][dst_vid].append(0) # index 0 = linkspeed
        machine_graph[src_vid][dst_vid].append(0) # index 1 = route count using edge
        machine_graph[src_vid][dst_vid][0] = linkspeed

        # Update the linkspeed histogram
        if linkspeed in linkspeed_hist:
            linkspeed_hist[linkspeed] += 1
        else:
            linkspeed_hist[linkspeed] = 1

        # Update things relating to vertex type and x,y,z coordinates
        if (src_vid in node_vids) and (dst_vid in gemini_vids): 
            num_n2g_edges += 1
        elif (src_vid in gemini_vids) and (dst_vid in node_vids):
            num_g2n_edges += 1
        elif (src_vid in gemini_vids) and (dst_vid in gemini_vids):
            num_g2g_edges += 1
            src_xyz    = gem2xyz[gemini_vids[src_vid]]
            dst_xyz    = gem2xyz[gemini_vids[dst_vid]]
            src_fields = re.split('\(|\)|\,', src_xyz)
            dst_fields = re.split('\(|\)|\,', dst_xyz)
            x_delta    = int(dst_fields[1]) - int(src_fields[1])
            y_delta    = int(dst_fields[2]) - int(src_fields[2])
            z_delta    = int(dst_fields[3]) - int(src_fields[3])
            xyz_delta  = xyz_str(str(x_delta), str(y_delta), str(z_delta))

            # Update the xyz delta histogram
            if xyz_delta in xyz_hist:
                xyz_hist[xyz_delta] += 1
            else:
                xyz_hist[xyz_delta] = 1

            # Update linkspeed histograms for x, y, and z dimensions
            if x_delta != 0 and y_delta == 0 and z_delta == 0:
                if linkspeed in xlinkspeed_hist:
                    xlinkspeed_hist[linkspeed] += 1
                else:
                    xlinkspeed_hist[linkspeed] = 1
            elif x_delta == 0 and y_delta != 0 and z_delta == 0:
                if linkspeed in ylinkspeed_hist:
                    ylinkspeed_hist[linkspeed] += 1
                else:
                    ylinkspeed_hist[linkspeed] = 1
            elif x_delta == 0 and y_delta == 0 and z_delta != 0:
                if linkspeed in zlinkspeed_hist:
                    zlinkspeed_hist[linkspeed] += 1
                else:
                    zlinkspeed_hist[linkspeed] = 1

        else:
            raise NameError('Unknown edge type encountered')


if actual_sum != expected_sum:
    raise NameError('Adjacency List Parse: Actual ID sum not equal to expected ID sum.')

if (num_n2g_edges + num_g2n_edges + num_g2g_edges) != num_edges:
    raise NameError('Adjacency List Parse: missing edges detected.')


# Should be at the end of the topomap file.  If not, that is bad.
extra_lines = 0
for line in tmf:
    extra_lines += 1
if extra_lines != 0:
    raise NameError('Extra lines at end of file.')


# Parse the routes file.
# Keep a histogram relating the route length in hops to the number of routes of that length.
num_routes = 0
for line in rf:
    num_routes += 1
    if num_routes % 10000 == 0:
        print 'Processed', num_routes, 'lines from routes file,', num_nodes*num_nodes, 'expected'
    line = line.rstrip('\r\n ')
    fields = re.split('\s+', line)

    # Sanity check: last vertex ID in the route list should be the target
    if fields[1] != fields[-1]:
        raise NameError('Bogus route detected.')

    # Update the hopcount histogram
    hops = len(fields) - 4
    if hops in hopcount_hist:
        hopcount_hist[hops] += 1
    else:
        hopcount_hist[hops] = 1

    # Traverse the hops along the route
    if hops > 0:
        prev_hop = -1
        ros = ''
        for hop in fields[2:-1]:
            if prev_hop != -1:
                # Update the machine graph
                src_vid = int(prev_hop)
                dst_vid = int(hop)
                machine_graph[src_vid][dst_vid][1] += 1

                # Update the route order histogram
                prev = gem2xyz[gemini_vids[prev_hop]]
                curr = gem2xyz[gemini_vids[int(hop)]]
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
                    raise NameError('Bogus route detected.')
            prev_hop = int(hop)

        # Compact the route string
        short_ros = ''
        prev_char = 'a'
        for char in ros[:]:
            if char != prev_char:
                short_ros += char
            prev_char = char

        # Update route order histogram
        if short_ros in route_order_hist:
            route_order_hist[short_ros] += 1
        else:
            route_order_hist[short_ros] = 1

# Sanity check: the number of lines in routes.txt should equal num_nodes^2
if num_routes != num_nodes * num_nodes:
    raise NameError('Incorrect number of routes in routes file.')


# Build the route count histogram
for src_vid in range(len(machine_graph)):
    for dst_vid, vector in sorted(machine_graph[src_vid].iteritems()):
        routes_using_edge = vector[1]
        if routes_using_edge in route_count_hist:
            route_count_hist[routes_using_edge] += 1
        else:
            route_count_hist[routes_using_edge] = 1


# Print out some general stats
print ''
print 'Overall Stats'
print '-------------'
print 'Number of Nodes:                  ', num_nodes
print 'Number of Geminis:                ', num_geminis
print ''
print 'Number of Node-to-Gemini Edges:   ', num_n2g_edges
print 'Number of Gemini-to-Node Edges:   ', num_g2n_edges
print 'Number of Gemini-to-Gemini Edges: ', num_g2g_edges
print ''
print 'Number of Vertices:               ', num_vertices
print 'Number of Edges:                  ', num_edges
print 'Number of Routes:                 ', num_routes
print ''
print ''


# Print the neighbor histogram to the screen
total = 0
print 'Neighbor Histogram (key=num_neighbors, val=vertex_count)'
print '--------------------------------------------------------'
for num_neighbors,count in sorted(neighbors_hist.iteritems()):
    print num_neighbors, count
    total += count

if total != num_vertices:
    raise NameError('Wrong number of edges in neighbor histogram.')


print ''
print ''


# Print the linkspeed histograms to the screen
total = 0
print 'Link Speed Histogram (key=edge_weight, val=edge_count)'
print '------------------------------------------------------'
for weight,count in sorted(linkspeed_hist.iteritems()):
    print weight, count
    total += count
if total != num_edges:
    raise NameError('Wrong number of edges in linkspeed histogram.')
print ''
print ''

print 'X Dimension Link Speed Histogram (key=edge_weight, val=edge_count)'
print '------------------------------------------------------------------'
for weight,count in sorted(xlinkspeed_hist.iteritems()):
    print weight, count
print ''
print ''

print 'Y Dimension Link Speed Histogram (key=edge_weight, val=edge_count)'
print '------------------------------------------------------------------'
for weight,count in sorted(ylinkspeed_hist.iteritems()):
    print weight, count
print ''
print ''

print 'Z Dimension Link Speed Histogram (key=edge_weight, val=edge_count)'
print '------------------------------------------------------------------'
for weight,count in sorted(zlinkspeed_hist.iteritems()):
    print weight, count
print ''
print ''


# Print the xyz delta histogram to the screen
total = 0
print 'Gemini-to-Gemini Edge (x,y,z) Delta Histogram'
print '      (key=xyz_delta, val=edge_count)'
print '---------------------------------------------'
for xyz_delta,count in sorted(xyz_hist.iteritems()):
    print xyz_delta, count
    total += count

if total != num_g2g_edges:
    raise NameError('Wrong number of edges in xyz histogram.')


print ''
print ''


# Print the hopcount histogram to the screen
total = 0
print 'Route Hop Count Histogram (key=hop_count, val=route_count)'
print 'Note: Only Gemini-to-Gemini hops are considered hops.'
print '      Host-to-Gemini hops are not counted.'
print '      -1 means src and dst are the same host.'
print '       0 means src and dst are connected to same Gemini.'
print '-----------------------------------------------------------'
for hops,count in sorted(hopcount_hist.iteritems()):
    print hops, count
    total += count

if total != num_routes:
    raise NameError('Wrong number of routes in hopcount histogram.')


print ''
print ''


# Print the route order histogram to the screen
total = 0
print 'Route Order Histogram (key=route_order, val=route_count)'
print 'Note: Capital letters represent positive direction,'
print '      Lower-case letters represent negative direction.'
print '--------------------------------------------------------'
for order,count in sorted(route_order_hist.iteritems()):
    print order, count
    total += count

if total != num_geminis * (num_geminis - 1) * 4:
    raise NameError('Wrong number of routes in route order histogram.')


print ''
print ''


# Print the route count histogram to the screen
print 'Route Count Histogram (key=routes_using_edge, val=edge_count)'
print 'Note: This only considers Gemini-to-Gemini edges.'
print '      Host-to-Gemini and Gemini-to-Host links have edge_count=0.'
print '----------------------------------------------------------------'
for route_count, edge_count in sorted(route_count_hist.iteritems()):
    print route_count, edge_count

print ''


# Output a file with edge weights representing the number of static
# routes that use the edge.  Similar format to topomap.txt format,
# without header stuff.
#f = open("route-dist.txt", 'w')
#for src_vid in range(len(machine_graph)):
#    print >>f, src_vid, 
#    for dst_vid,vector in sorted(machine_graph[src_vid].iteritems()):
#        print >>f, str(dst_vid) + '(' + str(vector[1]) + ')',
#    print >>f, ''


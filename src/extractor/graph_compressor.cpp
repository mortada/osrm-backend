#include "extractor/graph_compressor.hpp"

#include "extractor/compressed_edge_container.hpp"
#include "extractor/restriction_map.hpp"
#include "util/dynamic_graph.hpp"
#include "util/node_based_graph.hpp"
#include "util/percent.hpp"

#include "util/simple_logger.hpp"

namespace osrm
{
namespace extractor
{

GraphCompressor::GraphCompressor(SpeedProfileProperties speed_profile)
    : speed_profile(std::move(speed_profile))
{
}

void GraphCompressor::Compress(const std::unordered_set<NodeID> &barrier_nodes,
                               const std::unordered_set<NodeID> &traffic_lights,
                               RestrictionMap &restriction_map,
                               util::NodeBasedDynamicGraph &graph,
                               CompressedEdgeContainer &geometry_compressor)
{
    const unsigned original_number_of_nodes = graph.GetNumberOfNodes();
    const unsigned original_number_of_edges = graph.GetNumberOfEdges();

    util::Percent progress(original_number_of_nodes);

    for (const NodeID node_v : util::irange(0u, original_number_of_nodes))
    {
        progress.printStatus(node_v);

        // only contract degree 2 vertices
        if (2 != graph.GetOutDegree(node_v))
        {
            continue;
        }

        // don't contract barrier node
        if (barrier_nodes.end() != barrier_nodes.find(node_v))
        {
            continue;
        }

        // check if v is a via node for a turn restriction, i.e. a 'directed' barrier node
        if (restriction_map.IsViaNode(node_v))
        {
            continue;
        }

        //    reverse_e2   forward_e2
        // u <---------- v -----------> w
        //    ----------> <-----------
        //    forward_e1   reverse_e1
        //
        // Will be compressed to:
        //
        //    reverse_e1
        // u <---------- w
        //    ---------->
        //    forward_e1
        //
        // If the edges are compatible.

        const bool reverse_edge_order = graph.GetEdgeData(graph.BeginEdges(node_v)).reversed;
        const EdgeID forward_e2 = graph.BeginEdges(node_v) + reverse_edge_order;
        BOOST_ASSERT(SPECIAL_EDGEID != forward_e2);
        BOOST_ASSERT(forward_e2 >= graph.BeginEdges(node_v) && forward_e2 < graph.EndEdges(node_v));
        const EdgeID reverse_e2 = graph.BeginEdges(node_v) + 1 - reverse_edge_order;
        BOOST_ASSERT(SPECIAL_EDGEID != reverse_e2);
        BOOST_ASSERT(reverse_e2 >= graph.BeginEdges(node_v) && reverse_e2 < graph.EndEdges(node_v));

        const EdgeData &fwd_edge_data2 = graph.GetEdgeData(forward_e2);
        const EdgeData &rev_edge_data2 = graph.GetEdgeData(reverse_e2);

        const NodeID node_w = graph.GetTarget(forward_e2);
        BOOST_ASSERT(SPECIAL_NODEID != node_w);
        BOOST_ASSERT(node_v != node_w);
        const NodeID node_u = graph.GetTarget(reverse_e2);
        BOOST_ASSERT(SPECIAL_NODEID != node_u);
        BOOST_ASSERT(node_u != node_v);

        const EdgeID forward_e1 = graph.FindEdge(node_u, node_v);
        BOOST_ASSERT(SPECIAL_EDGEID != forward_e1);
        BOOST_ASSERT(node_v == graph.GetTarget(forward_e1));
        const EdgeID reverse_e1 = graph.FindEdge(node_w, node_v);
        BOOST_ASSERT(SPECIAL_EDGEID != reverse_e1);
        BOOST_ASSERT(node_v == graph.GetTarget(reverse_e1));

        const EdgeData &fwd_edge_data1 = graph.GetEdgeData(forward_e1);
        const EdgeData &rev_edge_data1 = graph.GetEdgeData(reverse_e1);

        if (graph.FindEdgeInEitherDirection(node_u, node_w) != SPECIAL_EDGEID)
        {
            continue;
        }

        // this case can happen if two ways with different names overlap
        if (fwd_edge_data1.name_id != rev_edge_data1.name_id ||
            fwd_edge_data2.name_id != rev_edge_data2.name_id)
        {
            continue;
        }

        if (fwd_edge_data1.IsCompatibleTo(fwd_edge_data2) &&
            rev_edge_data1.IsCompatibleTo(rev_edge_data2))
        {
            BOOST_ASSERT(graph.GetEdgeData(forward_e1).name_id ==
                         graph.GetEdgeData(reverse_e1).name_id);
            BOOST_ASSERT(graph.GetEdgeData(forward_e2).name_id ==
                         graph.GetEdgeData(reverse_e2).name_id);

            // Get distances before graph is modified
            const int forward_weight1 = graph.GetEdgeData(forward_e1).distance;
            const int forward_weight2 = graph.GetEdgeData(forward_e2).distance;

            BOOST_ASSERT(0 != forward_weight1);
            BOOST_ASSERT(0 != forward_weight2);

            const int reverse_weight1 = graph.GetEdgeData(reverse_e1).distance;
            const int reverse_weight2 = graph.GetEdgeData(reverse_e2).distance;

            BOOST_ASSERT(0 != reverse_weight1);
            BOOST_ASSERT(0 != reverse_weight2);

            const bool has_node_penalty = traffic_lights.find(node_v) != traffic_lights.end();

            // add weight of e2's to e1
            graph.GetEdgeData(forward_e1).distance += fwd_edge_data2.distance;
            graph.GetEdgeData(reverse_e1).distance += rev_edge_data2.distance;
            if (has_node_penalty)
            {
                graph.GetEdgeData(forward_e1).distance += speed_profile.traffic_signal_penalty;
                graph.GetEdgeData(reverse_e1).distance += speed_profile.traffic_signal_penalty;
            }

            // extend e1's to targets of e2's
            graph.SetTarget(forward_e1, node_w);
            graph.SetTarget(reverse_e1, node_u);

            // remove e2's (if bidir, otherwise only one)
            graph.DeleteEdge(node_v, forward_e2);
            graph.DeleteEdge(node_v, reverse_e2);

            // update any involved turn restrictions
            restriction_map.FixupStartingTurnRestriction(node_u, node_v, node_w);
            restriction_map.FixupArrivingTurnRestriction(node_u, node_v, node_w, graph);

            restriction_map.FixupStartingTurnRestriction(node_w, node_v, node_u);
            restriction_map.FixupArrivingTurnRestriction(node_w, node_v, node_u, graph);

            // store compressed geometry in container
            geometry_compressor.CompressEdge(
                forward_e1, forward_e2, node_v, node_w,
                forward_weight1 + (has_node_penalty ? speed_profile.traffic_signal_penalty : 0),
                forward_weight2);
            geometry_compressor.CompressEdge(
                reverse_e1, reverse_e2, node_v, node_u, reverse_weight1,
                reverse_weight2 + (has_node_penalty ? speed_profile.traffic_signal_penalty : 0));
        }
    }

    PrintStatistics(original_number_of_nodes, original_number_of_edges, graph);
}

void GraphCompressor::PrintStatistics(unsigned original_number_of_nodes,
                                      unsigned original_number_of_edges,
                                      const util::NodeBasedDynamicGraph &graph) const
{

    unsigned new_node_count = 0;
    unsigned new_edge_count = 0;

    for (const auto i : util::irange(0u, graph.GetNumberOfNodes()))
    {
        if (graph.GetOutDegree(i) > 0)
        {
            ++new_node_count;
            new_edge_count += (graph.EndEdges(i) - graph.BeginEdges(i));
        }
    }
    util::SimpleLogger().Write() << "Node compression ratio: "
                                 << new_node_count / (double)original_number_of_nodes;
    util::SimpleLogger().Write() << "Edge compression ratio: "
                                 << new_edge_count / (double)original_number_of_edges;
}
}
}

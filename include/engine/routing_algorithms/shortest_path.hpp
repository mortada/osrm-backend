#ifndef SHORTEST_PATH_HPP
#define SHORTEST_PATH_HPP

#include "util/typedefs.hpp"

#include "engine/routing_algorithms/routing_base.hpp"

#include "engine/search_engine_data.hpp"
#include "util/integer_range.hpp"

#include <boost/assert.hpp>

namespace osrm
{
namespace engine
{
namespace routing_algorithms
{

template <class DataFacadeT>
class ShortestPathRouting final
    : public BasicRoutingInterface<DataFacadeT, ShortestPathRouting<DataFacadeT>>
{
    using super = BasicRoutingInterface<DataFacadeT, ShortestPathRouting<DataFacadeT>>;
    using QueryHeap = SearchEngineData::QueryHeap;
    SearchEngineData &engine_working_data;
    const static constexpr bool FORWARD_DIRECTION = true;
    const static constexpr bool REVERSE_DIRECTION = false;
    const static constexpr bool DO_NOT_FORCE_LOOP = false;

  public:
    ShortestPathRouting(DataFacadeT *facade, SearchEngineData &engine_working_data)
        : super(facade), engine_working_data(engine_working_data)
    {
    }

    ~ShortestPathRouting() {}

    inline bool
    forceLoop(bool forward, const PhantomNode &source_phantom, const PhantomNode &target_phantom) const
    {
        if (forward)
            return source_phantom.forward_node_id == target_phantom.forward_node_id &&
                   source_phantom.GetForwardWeightPlusOffset() >
                       target_phantom.GetForwardWeightPlusOffset();
        else
            return source_phantom.reverse_node_id == target_phantom.reverse_node_id &&
                   source_phantom.GetReverseWeightPlusOffset() >
                       target_phantom.GetReverseWeightPlusOffset();
    };

    // allows a uturn at the target_phantom
    // searches source forward/reverse -> target forward/reverse
    void SearchWithUTurn(QueryHeap &forward_heap,
                         QueryHeap &reverse_heap,
                         const bool search_from_forward_node,
                         const bool search_from_reverse_node,
                         const bool search_to_forward_node,
                         const bool search_to_reverse_node,
                         const PhantomNode &source_phantom,
                         const PhantomNode &target_phantom,
                         const int total_distance_to_forward,
                         const int total_distance_to_reverse,
                         int &new_total_distance,
                         std::vector<NodeID> &leg_packed_path) const
    {
        forward_heap.Clear();
        reverse_heap.Clear();
        if (search_from_forward_node)
        {
            forward_heap.Insert(source_phantom.forward_node_id,
                                total_distance_to_forward -
                                    source_phantom.GetForwardWeightPlusOffset(),
                                source_phantom.forward_node_id);
        }
        if (search_from_reverse_node)
        {
            forward_heap.Insert(source_phantom.reverse_node_id,
                                total_distance_to_reverse -
                                    source_phantom.GetReverseWeightPlusOffset(),
                                source_phantom.reverse_node_id);
        }
        if (search_to_forward_node)
        {
            reverse_heap.Insert(target_phantom.forward_node_id,
                                target_phantom.GetForwardWeightPlusOffset(),
                                target_phantom.forward_node_id);
        }
        if (search_to_reverse_node)
        {
            reverse_heap.Insert(target_phantom.reverse_node_id,
                                target_phantom.GetReverseWeightPlusOffset(),
                                target_phantom.reverse_node_id);
        }

        BOOST_ASSERT(forward_heap.Size() > 0);
        BOOST_ASSERT(reverse_heap.Size() > 0);
        super::Search(forward_heap, reverse_heap, new_total_distance, leg_packed_path,
                      forceLoop(FORWARD_DIRECTION, source_phantom, target_phantom),
                      forceLoop(REVERSE_DIRECTION, source_phantom, target_phantom));
    }

    // searches shortest path between:
    // source forward/reverse -> target forward
    // source forward/reverse -> target reverse
    void Search(QueryHeap &forward_heap,
                QueryHeap &reverse_heap,
                const bool search_from_forward_node,
                const bool search_from_reverse_node,
                const bool search_to_forward_node,
                const bool search_to_reverse_node,
                const PhantomNode &source_phantom,
                const PhantomNode &target_phantom,
                const int total_distance_to_forward,
                const int total_distance_to_reverse,
                int &new_total_distance_to_forward,
                int &new_total_distance_to_reverse,
                std::vector<NodeID> &leg_packed_path_forward,
                std::vector<NodeID> &leg_packed_path_reverse) const
    {
        if (search_to_forward_node)
        {
            forward_heap.Clear();
            reverse_heap.Clear();
            reverse_heap.Insert(target_phantom.forward_node_id,
                                target_phantom.GetForwardWeightPlusOffset(),
                                target_phantom.forward_node_id);

            if (search_from_forward_node)
            {
                forward_heap.Insert(source_phantom.forward_node_id,
                                    total_distance_to_forward -
                                        source_phantom.GetForwardWeightPlusOffset(),
                                    source_phantom.forward_node_id);
            }
            if (search_from_reverse_node)
            {
                forward_heap.Insert(source_phantom.reverse_node_id,
                                    total_distance_to_reverse -
                                        source_phantom.GetReverseWeightPlusOffset(),
                                    source_phantom.reverse_node_id);
            }
            BOOST_ASSERT(forward_heap.Size() > 0);
            BOOST_ASSERT(reverse_heap.Size() > 0);
            super::Search(
                forward_heap, reverse_heap, new_total_distance_to_forward, leg_packed_path_forward,
                forceLoop(FORWARD_DIRECTION, source_phantom, target_phantom), DO_NOT_FORCE_LOOP);
        }

        if (search_to_reverse_node)
        {
            forward_heap.Clear();
            reverse_heap.Clear();
            reverse_heap.Insert(target_phantom.reverse_node_id,
                                target_phantom.GetReverseWeightPlusOffset(),
                                target_phantom.reverse_node_id);
            if (search_from_forward_node)
            {
                forward_heap.Insert(source_phantom.forward_node_id,
                                    total_distance_to_forward -
                                        source_phantom.GetForwardWeightPlusOffset(),
                                    source_phantom.forward_node_id);
            }
            if (search_from_reverse_node)
            {
                forward_heap.Insert(source_phantom.reverse_node_id,
                                    total_distance_to_reverse -
                                        source_phantom.GetReverseWeightPlusOffset(),
                                    source_phantom.reverse_node_id);
            }
            BOOST_ASSERT(forward_heap.Size() > 0);
            BOOST_ASSERT(reverse_heap.Size() > 0);
            super::Search(forward_heap, reverse_heap, new_total_distance_to_reverse,
                          leg_packed_path_reverse, DO_NOT_FORCE_LOOP,
                          forceLoop(REVERSE_DIRECTION, source_phantom, target_phantom));
        }
    }

    void UnpackLegs(const std::vector<PhantomNodes> &phantom_nodes_vector,
                    const std::vector<NodeID> &total_packed_path,
                    const std::vector<std::size_t> &packed_leg_begin,
                    const int shortest_path_length,
                    InternalRouteResult &raw_route_data) const
    {
        raw_route_data.unpacked_path_segments.resize(packed_leg_begin.size() - 1);

        raw_route_data.shortest_path_length = shortest_path_length;

        for (const auto current_leg : util::irange<std::size_t>(0, packed_leg_begin.size() - 1))
        {
            auto leg_begin = total_packed_path.begin() + packed_leg_begin[current_leg];
            auto leg_end = total_packed_path.begin() + packed_leg_begin[current_leg + 1];
            const auto &unpack_phantom_node_pair = phantom_nodes_vector[current_leg];
            super::UnpackPath(leg_begin, leg_end, unpack_phantom_node_pair,
                              raw_route_data.unpacked_path_segments[current_leg]);

            raw_route_data.source_traversed_in_reverse.push_back(
                (*leg_begin != phantom_nodes_vector[current_leg].source_phantom.forward_node_id));
            raw_route_data.target_traversed_in_reverse.push_back(
                (*std::prev(leg_end) !=
                 phantom_nodes_vector[current_leg].target_phantom.forward_node_id));
        }
    }

    void operator()(const std::vector<PhantomNodes> &phantom_nodes_vector,
                    const std::vector<bool> &uturn_indicators,
                    InternalRouteResult &raw_route_data) const
    {
        BOOST_ASSERT(uturn_indicators.size() == phantom_nodes_vector.size() + 1);
        engine_working_data.InitializeOrClearFirstThreadLocalStorage(
            super::facade->GetNumberOfNodes());

        QueryHeap &forward_heap = *(engine_working_data.forward_heap_1);
        QueryHeap &reverse_heap = *(engine_working_data.reverse_heap_1);

        int total_distance_to_forward = 0;
        int total_distance_to_reverse = 0;
        bool search_from_forward_node =
            phantom_nodes_vector.front().source_phantom.forward_node_id != SPECIAL_NODEID;
        bool search_from_reverse_node =
            phantom_nodes_vector.front().source_phantom.reverse_node_id != SPECIAL_NODEID;

        std::vector<NodeID> prev_packed_leg_to_forward;
        std::vector<NodeID> prev_packed_leg_to_reverse;

        std::vector<NodeID> total_packed_path_to_forward;
        std::vector<std::size_t> packed_leg_to_forward_begin;
        std::vector<NodeID> total_packed_path_to_reverse;
        std::vector<std::size_t> packed_leg_to_reverse_begin;

        std::size_t current_leg = 0;
        // this implements a dynamic program that finds the shortest route through
        // a list of vias
        for (const auto &phantom_node_pair : phantom_nodes_vector)
        {
            int new_total_distance_to_forward = INVALID_EDGE_WEIGHT;
            int new_total_distance_to_reverse = INVALID_EDGE_WEIGHT;

            std::vector<NodeID> packed_leg_to_forward;
            std::vector<NodeID> packed_leg_to_reverse;

            const auto &source_phantom = phantom_node_pair.source_phantom;
            const auto &target_phantom = phantom_node_pair.target_phantom;

            BOOST_ASSERT(current_leg + 1 < uturn_indicators.size());
            const bool allow_u_turn_at_via = uturn_indicators[current_leg + 1];

            bool search_to_forward_node = target_phantom.forward_node_id != SPECIAL_NODEID;
            bool search_to_reverse_node = target_phantom.reverse_node_id != SPECIAL_NODEID;

            BOOST_ASSERT(!search_from_forward_node ||
                         source_phantom.forward_node_id != SPECIAL_NODEID);
            BOOST_ASSERT(!search_from_reverse_node ||
                         source_phantom.reverse_node_id != SPECIAL_NODEID);

            BOOST_ASSERT(search_from_forward_node || search_from_reverse_node);

            if (search_to_reverse_node || search_to_forward_node)
            {
                if (allow_u_turn_at_via)
                {
                    SearchWithUTurn(forward_heap, reverse_heap, search_from_forward_node,
                                    search_from_reverse_node, search_to_forward_node,
                                    search_to_reverse_node, source_phantom, target_phantom,
                                    total_distance_to_forward, total_distance_to_reverse,
                                    new_total_distance_to_forward, packed_leg_to_forward);
                    // if only the reverse node is valid (e.g. when using the match plugin) we
                    // actually need to move
                    if (target_phantom.forward_node_id == SPECIAL_NODEID)
                    {
                        BOOST_ASSERT(target_phantom.reverse_node_id != SPECIAL_NODEID);
                        new_total_distance_to_reverse = new_total_distance_to_forward;
                        packed_leg_to_reverse = std::move(packed_leg_to_forward);
                        new_total_distance_to_forward = INVALID_EDGE_WEIGHT;
                    }
                    else if (target_phantom.reverse_node_id != SPECIAL_NODEID)
                    {
                        new_total_distance_to_reverse = new_total_distance_to_forward;
                        packed_leg_to_reverse = packed_leg_to_forward;
                    }
                }
                else
                {
                    Search(forward_heap, reverse_heap, search_from_forward_node,
                           search_from_reverse_node, search_to_forward_node, search_to_reverse_node,
                           source_phantom, target_phantom, total_distance_to_forward,
                           total_distance_to_reverse, new_total_distance_to_forward,
                           new_total_distance_to_reverse, packed_leg_to_forward,
                           packed_leg_to_reverse);
                }
            }

            // No path found for both target nodes?
            if ((INVALID_EDGE_WEIGHT == new_total_distance_to_forward) &&
                (INVALID_EDGE_WEIGHT == new_total_distance_to_reverse))
            {
                raw_route_data.shortest_path_length = INVALID_EDGE_WEIGHT;
                raw_route_data.alternative_path_length = INVALID_EDGE_WEIGHT;
                return;
            }

            // we need to figure out how the new legs connect to the previous ones
            if (current_leg > 0)
            {
                bool forward_to_forward =
                    (new_total_distance_to_forward != INVALID_EDGE_WEIGHT) &&
                    packed_leg_to_forward.front() == source_phantom.forward_node_id;
                bool reverse_to_forward =
                    (new_total_distance_to_forward != INVALID_EDGE_WEIGHT) &&
                    packed_leg_to_forward.front() == source_phantom.reverse_node_id;
                bool forward_to_reverse =
                    (new_total_distance_to_reverse != INVALID_EDGE_WEIGHT) &&
                    packed_leg_to_reverse.front() == source_phantom.forward_node_id;
                bool reverse_to_reverse =
                    (new_total_distance_to_reverse != INVALID_EDGE_WEIGHT) &&
                    packed_leg_to_reverse.front() == source_phantom.reverse_node_id;

                BOOST_ASSERT(!forward_to_forward || !reverse_to_forward);
                BOOST_ASSERT(!forward_to_reverse || !reverse_to_reverse);

                // in this case we always need to copy
                if (forward_to_forward && forward_to_reverse)
                {
                    // in this case we copy the path leading to the source forward node
                    // and change the case
                    total_packed_path_to_reverse = total_packed_path_to_forward;
                    packed_leg_to_reverse_begin = packed_leg_to_forward_begin;
                    forward_to_reverse = false;
                    reverse_to_reverse = true;
                }
                else if (reverse_to_forward && reverse_to_reverse)
                {
                    total_packed_path_to_forward = total_packed_path_to_reverse;
                    packed_leg_to_forward_begin = packed_leg_to_reverse_begin;
                    reverse_to_forward = false;
                    forward_to_forward = true;
                }
                BOOST_ASSERT(!forward_to_forward || !forward_to_reverse);
                BOOST_ASSERT(!reverse_to_forward || !reverse_to_reverse);

                // in this case we just need to swap to regain the correct mapping
                if (reverse_to_forward || forward_to_reverse)
                {
                    total_packed_path_to_forward.swap(total_packed_path_to_reverse);
                    packed_leg_to_forward_begin.swap(packed_leg_to_reverse_begin);
                }
            }

            if (new_total_distance_to_forward != INVALID_EDGE_WEIGHT)
            {
                BOOST_ASSERT(target_phantom.forward_node_id != SPECIAL_NODEID);

                packed_leg_to_forward_begin.push_back(total_packed_path_to_forward.size());
                total_packed_path_to_forward.insert(total_packed_path_to_forward.end(),
                                                    packed_leg_to_forward.begin(),
                                                    packed_leg_to_forward.end());
                search_from_forward_node = true;
            }
            else
            {
                total_packed_path_to_forward.clear();
                packed_leg_to_forward_begin.clear();
                search_from_forward_node = false;
            }

            if (new_total_distance_to_reverse != INVALID_EDGE_WEIGHT)
            {
                BOOST_ASSERT(target_phantom.reverse_node_id != SPECIAL_NODEID);

                packed_leg_to_reverse_begin.push_back(total_packed_path_to_reverse.size());
                total_packed_path_to_reverse.insert(total_packed_path_to_reverse.end(),
                                                    packed_leg_to_reverse.begin(),
                                                    packed_leg_to_reverse.end());
                search_from_reverse_node = true;
            }
            else
            {
                total_packed_path_to_reverse.clear();
                packed_leg_to_reverse_begin.clear();
                search_from_reverse_node = false;
            }

            prev_packed_leg_to_forward = std::move(packed_leg_to_forward);
            prev_packed_leg_to_reverse = std::move(packed_leg_to_reverse);

            total_distance_to_forward = new_total_distance_to_forward;
            total_distance_to_reverse = new_total_distance_to_reverse;

            ++current_leg;
        }

        BOOST_ASSERT(total_distance_to_forward != INVALID_EDGE_WEIGHT ||
                     total_distance_to_reverse != INVALID_EDGE_WEIGHT);

        // We make sure the fastest route is always in packed_legs_to_forward
        if (total_distance_to_forward > total_distance_to_reverse)
        {
            // insert sentinel
            packed_leg_to_reverse_begin.push_back(total_packed_path_to_reverse.size());
            BOOST_ASSERT(packed_leg_to_reverse_begin.size() == phantom_nodes_vector.size() + 1);

            UnpackLegs(phantom_nodes_vector, total_packed_path_to_reverse,
                       packed_leg_to_reverse_begin, total_distance_to_reverse, raw_route_data);
        }
        else
        {
            // insert sentinel
            packed_leg_to_forward_begin.push_back(total_packed_path_to_forward.size());
            BOOST_ASSERT(packed_leg_to_forward_begin.size() == phantom_nodes_vector.size() + 1);

            UnpackLegs(phantom_nodes_vector, total_packed_path_to_forward,
                       packed_leg_to_forward_begin, total_distance_to_forward, raw_route_data);
        }
    }
};
}
}
}

#endif /* SHORTEST_PATH_HPP */

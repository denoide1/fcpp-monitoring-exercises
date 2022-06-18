// Copyright © 2022 Giorgio Audrito. All Rights Reserved.

/**
 * @file movement.hpp
 * @brief Implementation of the group movement behaviour.
 */

#ifndef FCPP_MOVEMENT_H_
#define FCPP_MOVEMENT_H_

#include "lib/fcpp.hpp"


/**
 * @brief Namespace containing all the objects in the FCPP library.
 */
namespace fcpp {

const int max_group_size = 100;
const int hi_x = 1200;
const int hi_y = 800;

//! @brief Namespace containing the libraries of coordination routines.
namespace coordination {

//! @brief Tags used in the node storage.
namespace tags {
    //! @brief General string that can be used for debugging.
    struct debug {};
    //! @brief Speed of the current node.
    struct speed {};
    //! @brief Offset radius for the current node.
    struct offset {};
}

//! @brief Reaches a target position following streets.
FUN real_t reach_on_streets(ARGS, vec<2> target, real_t max_v, times_t period) { CODE
    vec<2> t = node.net.path_to(node.position(), node.net.closest_space(target));
    node.storage(tags::debug{}) = to_string(make_tuple("sp:", node.net.closest_space(node.position()), "ob:", node.net.closest_obstacle(node.position()), "target:", target, "path:", t));
    if (isnan(t[0]) or isnan(t[1]))
        t = node.position();
    if (target[0] < 0 or target[1] < 0 or target[0] > 1200 or target[1] > 800)
        t = node.position();
    return follow_target(CALL, t, max_v, period);
}
FUN_EXPORT reach_on_streets_t = export_list<point_gravitational_force_t, neighbour_gravitational_force_t>;

//! @brief Regulates random movement in groups.
FUN void group_walk(ARGS) { CODE
    using namespace tags;

    vec<2> low = {0, 0};
    vec<2> hi = {hi_x, hi_y};
    times_t period = 1;
    device_t leader = node.uid - (node.uid % max_group_size);
    real_t max_v = node.storage(speed{});
    real_t radius = node.storage(offset{});
    bool first_round = old(CALL, true, false);
    if (node.uid == leader) {
        if (first_round)
            node.position() = node.net.closest_space(node.position());
        // leaders just walk randomly
        vec<2> target = random_rectangle_target(CALL, low, hi);
        old(CALL, target, [&](vec<2> t){
            real_t dist = reach_on_streets(CALL, t, max_v, period);
            return dist > max_v * period ? t : target;
        });
    } else {
        // followers chase the leader up to an offset
        vec<2> t = random_rectangle_target(CALL, make_vec(-radius, -radius), make_vec(radius, radius));
        t = constant(CALL, t) + node.net.node_at(leader).position();
        if (first_round)
            node.position() = node.net.closest_space(t); // on the first simulated round
        else
            reach_on_streets(CALL, t, max_v, period); // on following rounds
    }
}
//! @brief Export types used by the group_walk function.
FUN_EXPORT group_walk_t = export_list<rectangle_walk_t<2>, constant_t<vec<2>>, reach_on_streets_t, bool>;

//! @brief Executes a program independently in a partition of the network based on the value of a given key.
GEN(T, G) auto switcher(ARGS, T&& key, G&& f) { CODE
    internal::trace_key trace_process(node.stack_trace, key);
    return f();
}

}

//! @brief Namespace for component options.
namespace option {
    //! @brief Import tags to be used for component options.
    using namespace component::tags;
    //! @brief Import tags used by aggregate functions.
    using namespace coordination::tags;

    //! @brief Class generating a given arithmetic sequence.
    template <typename R, intmax_t start, intmax_t step>
    using arithmetic_sequence = functor::add<
        functor::acc<distribution::constant_n<R, step>, R>,
        distribution::constant_n<R, start - step>
    >;

    //! @brief The distribution of initial node positions (random in a 1200x800 rectangle).
    using rectangle_d = distribution::rect_n<1, 0, 0, hi_x, hi_y>;

    //! @brief Template asserting a condition on an FCPP option.
    template <bool condition>
    struct option_assert;

    //! @brief Template asserting a condition on an FCPP option (true overload only).
    template <>
    struct option_assert<true> {};

    //! @brief Option generating a group of nodes moving together.
    template <int group_id, int group_size, int group_radius, int group_speed = 0, int start_time = 0>
    DECLARE_OPTIONS(spawn_group,
        option_assert<group_id >= 0>, // group ID should be positive
        option_assert<0 < group_size and group_size < max_group_size>, // group size allowed between 1 and 99
        // group_size spawn events all at start_time
        spawn_schedule<sequence::multiple_n<group_size, start_time>>,
        init<
            uid,    arithmetic_sequence<device_t, max_group_size * group_id, 1>, // arithmetic sequence of device IDs
            x,      rectangle_d, // random displacement of devices in the simulation area
            speed,  distribution::constant_n<double, group_speed * 1000, 3600>, // store the group speed, converting from km/h to m/s
            offset, distribution::constant_n<double, group_radius> // store the group radius
        >
    );
}

}

#endif // FCPP_MOVEMENT_H_

#include <neoglobal/clustering.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NEOGLOBAL_INITIAL_CLUSTER_MEMBER_CAPACITY 2

static void neoglobal_clustering_set_error(
    char *error,
    size_t error_size,
    const char *message
)
{
    if (error != NULL && error_size > 0)
        snprintf(error, error_size, "%s", message);
}

static b32 neoglobal_clustering_problem_is_valid(const NeoGlobal_Problem *problem) {
    if (
        problem == NULL ||
        problem->dimension == 0 ||
        problem->lower == NULL ||
        problem->upper == NULL ||
        !isfinite(problem->alpha) ||
        problem->alpha < 0.0 ||
        problem->alpha > 1.0
    ) {
        return false;
    }

    for (u32 i = 0; i < problem->dimension; ++i) {
        if (
            !isfinite((double)problem->lower[i]) ||
            !isfinite((double)problem->upper[i]) ||
            problem->lower[i] >= problem->upper[i]
        ) {
            return false;
        }
    }

    return true;
}

b32 neoglobal_cluster_set_init(
    CFD_Arena *arena,
    u64 cluster_capacity,
    u64 member_capacity,
    NeoGlobal_Cluster_Set *clusters
)
{
    if (
        arena == NULL ||
        arena->buffer == NULL ||
        clusters == NULL ||
        cluster_capacity == 0 ||
        member_capacity == 0 ||
        cluster_capacity > SIZE_MAX / sizeof(*clusters->items)
    ) {
        return false;
    }

    NeoGlobal_Cluster *items = cfd_arena_push_array(
        arena,
        NeoGlobal_Cluster,
        cluster_capacity
    );
    if (items == NULL)
        return false;

    clusters->arena = arena;
    clusters->items = items;
    clusters->count = 0;
    clusters->capacity = cluster_capacity;
    clusters->member_count = 0;
    clusters->member_capacity = member_capacity;
    return true;
}

static b32 neoglobal_cluster_reserve_members(
    NeoGlobal_Cluster_Set *clusters,
    NeoGlobal_Cluster *cluster,
    u64 required_capacity
)
{
    if (required_capacity <= cluster->member_capacity)
        return true;

    u64 new_capacity = cluster->member_capacity;
    if (new_capacity == 0)
        new_capacity = NEOGLOBAL_INITIAL_CLUSTER_MEMBER_CAPACITY;

    while (new_capacity < required_capacity) {
        if (new_capacity > UINT64_MAX / 2) {
            new_capacity = required_capacity;
            break;
        }
        new_capacity *= 2;
    }

    u64 maximum_capacity =
        clusters->member_capacity - clusters->member_count + cluster->member_count;
    if (new_capacity > maximum_capacity)
        new_capacity = maximum_capacity;

    if (
        new_capacity < required_capacity ||
        new_capacity > SIZE_MAX / sizeof(*cluster->members)
    ) {
        return false;
    }

    NeoGlobal_Sample *members = cfd_arena_push_array(
        clusters->arena,
        NeoGlobal_Sample,
        new_capacity
    );
    if (members == NULL)
        return false;

    if (cluster->member_count > 0) {
        memcpy(
            members,
            cluster->members,
            (size_t)cluster->member_count * sizeof(*members)
        );
    }

    cluster->members = members;
    cluster->member_capacity = new_capacity;
    return true;
}

b32 neoglobal_cluster_add_sample(
    NeoGlobal_Cluster_Set *clusters,
    u64 cluster_index,
    const NeoGlobal_Sample *sample
)
{
    if (
        clusters == NULL ||
        clusters->items == NULL ||
        clusters->arena == NULL ||
        sample == NULL ||
        sample->x == NULL ||
        !isfinite((double)sample->objective) ||
        cluster_index >= clusters->count ||
        clusters->member_count >= clusters->member_capacity
    ) {
        return false;
    }

    NeoGlobal_Cluster *cluster = &clusters->items[cluster_index];
    if (!neoglobal_cluster_reserve_members(clusters, cluster, cluster->member_count + 1))
        return false;

    cluster->members[cluster->member_count++] = *sample;
    ++clusters->member_count;

    if (sample->objective < cluster->center.objective)
        cluster->center = *sample;

    return true;
}

b32 neoglobal_cluster_set_create(
    NeoGlobal_Cluster_Set *clusters,
    const NeoGlobal_Sample *sample,
    u64 *cluster_index
)
{
    if (
        clusters == NULL ||
        clusters->items == NULL ||
        clusters->arena == NULL ||
        clusters->count >= clusters->capacity ||
        clusters->member_count >= clusters->member_capacity ||
        sample == NULL ||
        sample->x == NULL ||
        !isfinite((double)sample->objective)
    ) {
        return false;
    }

    u64 new_index = clusters->count++;
    NeoGlobal_Cluster *cluster = &clusters->items[new_index];
    cluster->members = NULL;
    cluster->member_count = 0;
    cluster->member_capacity = 0;
    cluster->center = *sample;

    if (!neoglobal_cluster_add_sample(clusters, new_index, sample)) {
        --clusters->count;
        return false;
    }

    if (cluster_index != NULL)
        *cluster_index = new_index;

    return true;
}

static f64 neoglobal_cluster_distance_infinity(
    const NeoGlobal_Problem *problem,
    const NeoGlobal_Sample *a,
    const NeoGlobal_Sample *b
)
{
    /* TODO: Evaluate bound-normalized distance as an optional improvement.
       It may help when dimensions use different scales, but compare its
       optimization results with this paper-faithful baseline. */
    f64 distance = 0.0;

    for (u32 i = 0; i < problem->dimension; ++i) {
        f64 delta = fabs((f64)a->x[i] - (f64)b->x[i]);

        if (delta > distance)
            distance = delta;
    }

    return distance;
}

static f64 neoglobal_cluster_distance_threshold(
    const NeoGlobal_Problem *problem,
    const NeoGlobal_Cluster_Set *clusters,
    u32 reduced_count
)
{
    f64 exponent_count =
        (f64)clusters->member_count + (f64)reduced_count - 1.0;

    if (exponent_count <= 0.0)
        return 0.0;

    f64 probability = 1.0 - pow(problem->alpha, 1.0 / exponent_count);
    if (probability <= 0.0)
        return 0.0;

    if (probability >= 1.0)
        return 1.0;

    return pow(probability, 1.0 / (f64)problem->dimension);
}

static b32 neoglobal_sample_belongs_to_cluster(
    const NeoGlobal_Problem *problem,
    const NeoGlobal_Cluster_Set *clusters,
    u64 cluster_index,
    const NeoGlobal_Sample *sample,
    f64 distance_threshold
)
{
    const NeoGlobal_Cluster *cluster = &clusters->items[cluster_index];
    for (u64 i = 0; i < cluster->member_count; ++i) {
        const NeoGlobal_Sample *member = &cluster->members[i];

        if (
            sample->objective > member->objective &&
            neoglobal_cluster_distance_infinity(problem, sample, member)
                < distance_threshold
        ) {
            return true;
        }
    }

    return false;
}

b32 neoglobal_clusters_assign_reduced(
    const NeoGlobal_Problem *problem,
    NeoGlobal_Cluster_Set *clusters,
    NeoGlobal_Sample *reduced,
    u32 *reduced_count,
    char *error,
    size_t error_size
)
{
    if (
        !neoglobal_clustering_problem_is_valid(problem) ||
        clusters == NULL ||
        clusters->count > clusters->capacity ||
        clusters->member_count > clusters->member_capacity ||
        (clusters->count > 0 && clusters->items == NULL) ||
        reduced_count == NULL ||
        (*reduced_count > 0 && reduced == NULL)
    ) {
        neoglobal_clustering_set_error(error, error_size, "invalid clustering arguments");
        return false;
    }

    for (u32 i = 0; i < *reduced_count; ++i) {
        if (
            reduced[i].x == NULL ||
            !isfinite((double)reduced[i].objective)
        ) {
            neoglobal_clustering_set_error(error, error_size, "invalid reduced sample");
            return false;
        }
    }

    f64 distance_threshold = neoglobal_cluster_distance_threshold(
        problem,
        clusters,
        *reduced_count
    );

    b32 moved_any;
    do {
        moved_any = false;

        for (u64 c = 0; c < clusters->count; ++c) {
            u32 i = 0;

            while (i < *reduced_count) {
                if (!neoglobal_sample_belongs_to_cluster(
                        problem,
                        clusters,
                        c,
                        &reduced[i],
                        distance_threshold
                    )) {
                    ++i;
                    continue;
                }

                if (!neoglobal_cluster_add_sample(clusters, c, &reduced[i])) {
                    neoglobal_clustering_set_error(error, error_size, "cluster capacity exceeded");
                    return false;
                }

                u32 remaining = *reduced_count - i - 1;
                memmove(
                    &reduced[i],
                    &reduced[i + 1],
                    (size_t)remaining * sizeof(*reduced)
                );
                --*reduced_count;
                moved_any = true;
            }
        }
    } while (moved_any);

    return true;
}

b32 neoglobal_clusters_record_local_minimum(
    const NeoGlobal_Problem *problem,
    NeoGlobal_Cluster_Set *clusters,
    const NeoGlobal_Sample *start,
    const NeoGlobal_Sample *local_minimum,
    u32 pending_reduced_count,
    char *error,
    size_t error_size
)
{
    if (
        !neoglobal_clustering_problem_is_valid(problem) ||
        clusters == NULL ||
        clusters->count > clusters->capacity ||
        clusters->member_count > clusters->member_capacity ||
        start == NULL ||
        start->x == NULL ||
        !isfinite((double)start->objective) ||
        local_minimum == NULL ||
        local_minimum->x == NULL ||
        !isfinite((double)local_minimum->objective) ||
        pending_reduced_count == 0
    ) {
        neoglobal_clustering_set_error(error, error_size, "invalid local-minimum cluster arguments");
        return false;
    }

    if (
        clusters->member_capacity - clusters->member_count < 2 ||
        clusters->arena == NULL ||
        (clusters->count > 0 && clusters->items == NULL)
    ) {
        neoglobal_clustering_set_error(error, error_size, "cluster member capacity exceeded");
        return false;
    }

    f64 distance_threshold = neoglobal_cluster_distance_threshold(
        problem,
        clusters,
        pending_reduced_count
    );

    u64 nearest_cluster = NEOGLOBAL_NO_CLUSTER_MEMBER;
    f64 nearest_distance = INFINITY;

    for (u64 c = 0; c < clusters->count; ++c) {
        f64 distance = neoglobal_cluster_distance_infinity(
            problem,
            local_minimum,
            &clusters->items[c].center
        );

        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest_cluster = c;
        }
    }

    if (
        nearest_cluster != NEOGLOBAL_NO_CLUSTER_MEMBER &&
        nearest_distance < distance_threshold / 10.0
    ) {
        if (
            !neoglobal_cluster_add_sample(clusters, nearest_cluster, local_minimum) ||
            !neoglobal_cluster_add_sample(clusters, nearest_cluster, start)
        ) {
            neoglobal_clustering_set_error(error, error_size, "cluster capacity exceeded");
            return false;
        }

        return true;
    }

    if (clusters->count >= clusters->capacity) {
        neoglobal_clustering_set_error(error, error_size, "cluster capacity exceeded");
        return false;
    }

    u64 new_cluster;
    if (!neoglobal_cluster_set_create(clusters, local_minimum, &new_cluster)) {
        neoglobal_clustering_set_error(error, error_size, "failed to create cluster");
        return false;
    }

    if (!neoglobal_cluster_add_sample(clusters, new_cluster, start)) {
        neoglobal_clustering_set_error(error, error_size, "cluster capacity exceeded");
        return false;
    }

    return true;
}

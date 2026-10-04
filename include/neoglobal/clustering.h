#ifndef NEOGLOBAL_CLUSTERING_H
#define NEOGLOBAL_CLUSTERING_H

#include <neoglobal/sampling.h>

#define NEOGLOBAL_NO_CLUSTER_MEMBER ((u64)-1)

typedef struct {
    NeoGlobal_Sample *members;
    u64 member_count;
    u64 member_capacity;
    NeoGlobal_Sample center;
} NeoGlobal_Cluster;

typedef struct {
    CFD_Arena *arena;
    NeoGlobal_Cluster *items;
    u64 count;
    u64 capacity;

    u64 member_count;
    u64 member_capacity;
} NeoGlobal_Cluster_Set;

b32 neoglobal_cluster_set_init(
    CFD_Arena *arena,
    u64 cluster_capacity,
    u64 member_capacity,
    NeoGlobal_Cluster_Set *clusters
);

b32 neoglobal_cluster_set_create(
    NeoGlobal_Cluster_Set *clusters,
    const NeoGlobal_Sample *sample,
    u64 *cluster_index
);

b32 neoglobal_cluster_add_sample(
    NeoGlobal_Cluster_Set *clusters,
    u64 cluster_index,
    const NeoGlobal_Sample *sample
);

/* Moves reduced samples belonging to existing clusters out of the array. */
b32 neoglobal_clusters_assign_reduced(
    const NeoGlobal_Problem *problem,
    NeoGlobal_Cluster_Set *clusters,
    NeoGlobal_Sample *reduced,
    u32 *reduced_count,
    char *error,
    size_t error_size
);

/* Adds a local-search result and its starting sample to the closest cluster,
   or creates a new cluster when the optimum is sufficiently far away. */
b32 neoglobal_clusters_record_local_minimum(
    const NeoGlobal_Problem *problem,
    NeoGlobal_Cluster_Set *clusters,
    const NeoGlobal_Sample *start,
    const NeoGlobal_Sample *local_minimum,
    u32 pending_reduced_count,
    char *error,
    size_t error_size
);

#endif

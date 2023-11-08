#ifndef _NDP120_ORCHESTRATOR_H
#define _NDP120_ORCHESTRATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __KERNEL__
#include <stdint.h>
#ifdef X86_64
#include <stdio.h>
#include <stdlib.h>
#endif
#endif

#define MAX_NDP120_NNGRAPH_NODES 16
#define MAX_GATE_INPUT  4
#define MAX_GATE_OUTPUT 4

enum {
    NDP_NONE = 0,
	NDP_AND = 1,
	NDP_OR = 2,
    NDP_XOR = 3
};

enum {
	NDP_LOGIC_GATE = 0,
	NDP_NNETWORK = 1
};

/** NDP120 orchestration graph node
 * num_inputs = 0 for first set of NN nodes in the graph
 * num_outputs = 0 for terminal node of the graph
 */
typedef struct ndp120_nno_node {
    uint8_t next_ids[MAX_GATE_OUTPUT];  /**< next node id */
    uint8_t input[MAX_GATE_INPUT];      /**< ids of input nodes */
    uint8_t type;                       /**< type: network, logic */
    uint8_t action;                     /**< action type */
    uint8_t flowset_id;                 /**< DSP flow group id for this node */
    uint8_t num_inputs;                 /**< number of input edges */
    uint8_t num_outputs;                /**< number of output edges */
    uint8_t id;                         /**< node id */
    uint8_t output;                     /**< output logic */
    uint8_t status;                     /**< status: on/off */
    uint8_t timeout;                    /**< timeout for a NN node */
    uint8_t timer;                      /**< timer for a NN node */
    uint8_t data[2];                    /**< additional data, if any */
} ndp120_nno_node_t;

/* NDP120 NN orchestration graph */
struct ndp120_nn_graph {
    uint8_t num_nodes;                  /**< number of nodes */
    uint8_t flowmap;                    /**< flowmap of all the NNs */
    uint8_t nn_output;                  /**< NN output of the graph */
    uint8_t rsvd;                       /**< for future use */
    ndp120_nno_node_t nn_graph[MAX_NDP120_NNGRAPH_NODES]; /**< all the nodes */
};

/** NN graph orchestrator
 * @param nn, NN node number
 * @param match, match data
 * @param timeout, for this NN node
 * returns network output of this graph, -1 for error condition.
 */
uint8_t ndp_orchestrate(uint8_t nn, uint8_t match);

/** Get active set of NN nodes in the graph
 */
void ndp_trace_graph(uint8_t, uint32_t *output);

#ifdef __cplusplus
} /* extern "C" { */
#endif
#endif /* _NDP120_ORCHESTRATOR_H */

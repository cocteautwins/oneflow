#include "graph/stage_graph.h"
#include "glog/logging.h"

namespace oneflow {

namespace {

void OneToOneConnect( 
    const std::vector<StageNode*>& cur_stages,
    const std::vector<StageNode*>& succ_stages,
    std::function<void(StageNode*, StageNode*)> ConnectTwoNode) {
  size_t stage_num = cur_stages.size();
  for (size_t i = 0; i < cur_stages.size(); ++i) {
    ConnectTwoNode(cur_stages[i], succ_stages[i]);
  }
}

void FullConnect(
    const std::vector<StageNode*>& cur_stages,
    const std::vector<StageNode*>& succ_stages,
    std::function<void(StageNode*, StageNode*)> ConnectTwoNode) {
  for (StageNode* cur_stage_node : cur_stages) {
    for (StageNode* succ_stage_node : succ_stages) {
      ConnectTwoNode(cur_stage_node, succ_stage_node);
    }
  }
}

void ConnectRelatedStages(
    const std::vector<StageNode*>& cur_stages,
    const std::vector<StageNode*>& succ_stages,
    std::function<void(StageNode*, StageNode*)> ConnectTwoNode) {
  CHECK_EQ(cur_stages.empty(), false);
  CHECK_EQ(succ_stages.empty(), false);
  auto cur_policy = cur_stages.front()->chain_node()->parallel_desc().policy();
  auto succ_policy = succ_stages.front()->chain_node()->parallel_desc().policy();
  if (cur_policy == ParallelDesc::kDataParallel
      && succ_policy == ParallelDesc::kDataParallel) {
    CHECK_EQ(cur_stages.size(), succ_stages.size());
    OneToOneConnect(cur_stages, succ_stages, ConnectTwoNode);
  } else {
    FullConnect(cur_stages, succ_stages, ConnectTwoNode);
  }
}

}

void StageGraph::Init(std::shared_ptr<const ChainGraph> chain_graph) {
  Graph::Init();
  chain_graph_ = chain_graph;
  // Init Stages
  std::unordered_map<const ChainNode*,
                     std::vector<StageNode*>> chain2stages;
  for (const std::unique_ptr<ChainNode>& cur_chain : chain_graph->nodes()) {
    chain2stages[cur_chain.get()] = {};
    for (MachineId machine_id : cur_chain->parallel_desc().machines()) {
      StageNode* stage_node = NewFinalNode();
      stage_node->mutable_machine_id() = machine_id;
      stage_node->set_chain_node(cur_chain.get());
      chain2stages.at(cur_chain.get()).push_back(stage_node);
    }
  }
  // Connect Stages
  std::function<void(StageNode*, StageNode*)> ConnectTwoNode = [this]
      (StageNode* src_node, StageNode* dst_node) {
    Connect(src_node, this->NewFinalEdge(), dst_node);
  };
  for (const std::unique_ptr<ChainNode>& cur_chain : chain_graph->nodes()) {
    for (const ChainEdge* edge : cur_chain->out_edges()) {
      const std::vector<StageNode*>& cur_stages =
          chain2stages.at(cur_chain.get());
      const std::vector<StageNode*>& succ_stages =
          chain2stages.at(edge->dst_node());
      ConnectRelatedStages(cur_stages, succ_stages, ConnectTwoNode);
    }
  }
  // Post processing
  UpdateStartAndStop();
}

} // namespace oneflow

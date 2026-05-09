#include "service_impl.h"

grpc::Status LibsServiceImpl::Delete(grpc::ServerContext* /*context*/,
                                     const Proto::Request* request, Proto::Response* response) {
  for(auto& policy:request->policy()){
    std::string policy_name = policy.policy_name();
    for(auto& rule:policy.rule()){
      if(!rule.rule_id().empty()){
        ENVOY_LOG(info,"delete rule {}",rule.rule_id());
        cache_impl_->delRule(policy.policy_name(),rule.rule_id());
      }
    }
  }
  response->set_result(1);
  return grpc::Status::OK;
}

grpc::Status LibsServiceImpl::Update(grpc::ServerContext* /*context*/,
                                     const Proto::Request* request, Proto::Response* response) {
  for(auto& policy:request->policy()){
    std::string policy_name = policy.policy_name();
    for(auto& rule:policy.rule()){
      ENVOY_LOG(info,"update rule {}",rule.rule_id());
      cache_impl_->updateRule(policy_name,rule.rule_id(),rule.data());
    }
  }
  response->set_result(1);
  return grpc::Status::OK;
}

grpc::Status LibsServiceImpl::AddFull(grpc::ServerContext* /*context*/,
                                      const Proto::Request* request,
                                      Proto::Response* response) {
  for(auto& policy:request->policy()){
    std::string policy_name = policy.policy_name();
    for(auto& rule:policy.rule()){
      ENVOY_LOG(info,"replace rule {}",rule.rule_id());
      cache_impl_->updateRule(policy_name,rule.rule_id(),rule.data());
    }
  }
  response->set_result(1);
  return grpc::Status::OK;
}
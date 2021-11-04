#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer &search_server)
    : search_server_(search_server)
    , time_(0)
    , no_result_requests_(0) {
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query, DocumentStatus status) {
  const auto result = search_server_.FindTopDocuments(raw_query, status);
  AddStatistic(result.empty());
  return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query) {
  const auto result = search_server_.FindTopDocuments(raw_query);
  AddStatistic(result.empty());
  return result;
}

int RequestQueue::GetNoResultRequests() const {
  return no_result_requests_;
}

void RequestQueue::AddStatistic(const bool no_result) {
  ++time_;
  if (no_result) {
    ++no_result_requests_;
  }
  if (time_ > sec_in_day_) {
    if (requests_.front().no_result) {
      --no_result_requests_;
    }
    requests_.pop_front();
  }
  requests_.push_back({no_result});
}


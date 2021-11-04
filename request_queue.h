#pragma once
#include "search_server.h"

#include <deque>

class RequestQueue {
 public:
  explicit RequestQueue(const SearchServer &search_server);
  // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
  template<typename DocumentPredicate>
  std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate) {
    const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddStatistic(result.empty());
    return result;
  }
  std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentStatus status);
  std::vector<Document> AddFindRequest(const std::string &raw_query);
  int GetNoResultRequests() const;

 private:
  struct QueryResult {
    bool no_result;
  };
  std::deque<QueryResult> requests_;
  const static int sec_in_day_ = 1440;
  const SearchServer &search_server_;
  size_t time_;
  size_t no_result_requests_;

  void AddStatistic(bool no_result);
};

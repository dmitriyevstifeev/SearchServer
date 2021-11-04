#include "process_queries.h"

#include <execution>
#include <algorithm>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer &search_server,
    const std::vector<std::string> &queries) {

  std::vector<std::vector<Document>> results(queries.size());
  std::transform(std::execution::par, queries.begin(),
                 queries.end(),
                 results.begin(),
                 [&search_server](const auto &query) {
                   return search_server.FindTopDocuments(query);
                 });
    
  return results;

}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

  const std::vector<std::vector<Document>> docs_vector = ProcessQueries(search_server, queries);
  std::vector<Document>results;
  for (const auto &docs: docs_vector){
    results.insert(results.end(), docs.begin(), docs.end());
  }

  return results;
}
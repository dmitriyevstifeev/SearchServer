#pragma once
#include "string_processing.h"
#include "document.h"
#include "concurrent_map.h"

#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <stdexcept>
#include <execution>
#include <string_view>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
 public:
  template<typename StringContainer>
  explicit SearchServer(const StringContainer &stop_words)
      : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
  {
    using namespace std::literals;

    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
      throw std::invalid_argument("Some of stop words are invalid"s);
    }
  }

  explicit SearchServer(const std::string &stop_words_text);
  explicit SearchServer(const std::string_view &stop_words_text);

  void AddDocument(int document_id,
                   const std::string_view &document,
                   DocumentStatus status,
                   const std::vector<int> &ratings);

  template<typename DocumentPredicate>
  std::vector<Document> FindTopDocuments(const std::string_view &raw_query,
                                         DocumentPredicate document_predicate) const {

    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
  }
  template<typename DocumentPredicate, typename ExecutionPolicy>
  std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view &raw_query,
                                         DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document &lhs, const Document &rhs) {
      if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
        return lhs.rating > rhs.rating;
      } else {
        return lhs.relevance > rhs.relevance;
      }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
      matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
  }

  std::vector<Document> FindTopDocuments(const std::string_view &raw_query, DocumentStatus status) const;
  std::vector<Document> FindTopDocuments(const std::string_view &raw_query) const;
  std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy seq,
                                         const std::string_view &raw_query,
                                         DocumentStatus status) const;
  std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy seq, const std::string_view &raw_query) const;
  std::vector<Document> FindTopDocuments(const std::execution::parallel_policy par,
                                         const std::string_view &raw_query,
                                         DocumentStatus status) const;
  std::vector<Document> FindTopDocuments(const std::execution::parallel_policy par, const std::string_view &raw_query) const;
  int GetDocumentCount() const;
  std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view &raw_query,
                                                                          int document_id) const;
  std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy seq,
                                                                          const std::string_view &raw_query,
                                                                          int document_id) const;
  std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy par,
                                                                          const std::string_view &raw_query,
                                                                          int document_id) const;
  std::set<int>::const_iterator begin() const;
  std::set<int>::const_iterator end() const;
  const std::map<std::string_view, double, std::less<>> &GetWordFrequencies(int document_id) const;
  void RemoveDocument(int document_id);
  void RemoveDocument(const std::execution::sequenced_policy seq, int document_id);
  void RemoveDocument(const std::execution::parallel_policy par, int document_id);

 private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
    std::string doc_text;
  };
  const std::set<std::string, std::less<>> stop_words_;
  std::map<std::string_view, std::map<int, double>, std::less<>> word_to_document_freqs_;
  std::map<int, DocumentData> documents_;
  std::set<int> document_ids_;
  std::map<int, std::map<std::string_view, double, std::less<>>> id_to_words_freqs_;

  bool IsStopWord(const std::string_view &word) const;
  static bool IsValidWord(const std::string_view &word);
  std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view &text) const;
  static int ComputeAverageRating(const std::vector<int> &ratings);

  struct QueryWord {
    std::string_view data;
    bool is_minus;
    bool is_stop;
  };

  QueryWord ParseQueryWord(const std::string_view &text) const;

  struct Query {
    std::set<std::string_view> plus_words;
    std::set<std::string_view> minus_words;
  };

  Query ParseQuery(const std::string_view &text) const;
  // Existence required
  double ComputeWordInverseDocumentFreq(const std::string_view &word) const;

  template<typename DocumentPredicate>
  std::vector<Document> FindAllDocuments(const Query &query, DocumentPredicate document_predicate) const{
    return FindAllDocuments(std::execution::seq, query, document_predicate);
  }

  template<typename DocumentPredicate>
  std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy seq, const Query &query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view &word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
      for (const auto[document_id, term_freq] : word_to_document_freqs_.at(word)) {
        const auto &document_data = documents_.at(document_id);
        if (document_predicate(document_id, document_data.status, document_data.rating)) {
          document_to_relevance[document_id] += term_freq * inverse_document_freq;
        }
      }
    }

    for (const std::string_view &word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      for (const auto[document_id, _] : word_to_document_freqs_.at(word)) {
        document_to_relevance.erase(document_id);
      }
    }

    std::vector<Document> matched_documents;
    for (const auto[document_id, relevance] : document_to_relevance) {
      matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
  }

  template<typename DocumentPredicate>
  std::vector<Document> FindAllDocuments(const std::execution::parallel_policy par,
                                         const Query &query,
                                         DocumentPredicate document_predicate) const {

    ConcurrentMap<int, double> document_to_relevance(3);
    std::for_each(par, query.plus_words.begin(), query.plus_words.end(), [&](const auto &word) {
      if (word_to_document_freqs_.count(word) != 0) {
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto[document_id, term_freq] : word_to_document_freqs_.at(word)) {
          const auto &document_data = documents_.at(document_id);
          if (document_predicate(document_id, document_data.status, document_data.rating)) {
            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
          }
        }
      }
    });

    std::for_each(par, query.minus_words.begin(), query.minus_words.end(), [&](const auto &word) {
      if (word_to_document_freqs_.count(word) != 0) {
        for (const auto[document_id, _] : word_to_document_freqs_.at(word)) {
          document_to_relevance.Erase(document_id);
        }
      }
    });

    std::vector<Document> matched_documents;
    for (const auto[document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
      matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
  }
};

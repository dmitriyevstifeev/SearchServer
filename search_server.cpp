#include "search_server.h"

#include <cmath>
#include <execution>
#include <algorithm>


SearchServer::SearchServer(const std::string &stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(const std::string_view &stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id,
                               const std::string_view &document,
                               DocumentStatus status,
                               const std::vector<int> &ratings) {
  using namespace std::literals;
  if ((document_id < 0) || (documents_.count(document_id) > 0)) {
    throw std::invalid_argument("Invalid document_id"s);
  }
  const auto doc_data = documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status , std::string(document)});
  const auto words = SplitIntoWordsNoStop(doc_data.first->second.doc_text);

  const double inv_word_count = 1.0 / words.size();
  for (const std::string_view &word : words) {
    word_to_document_freqs_[word][document_id] += inv_word_count;
    id_to_words_freqs_[document_id][word] += inv_word_count;
  }

  document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view &raw_query, DocumentStatus status) const {
  return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
    return document_status == status;
  });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view &raw_query) const {
  return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy seq,
                                                     const std::string_view &raw_query,
                                                     DocumentStatus status) const {
  return FindTopDocuments(seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
    return document_status == status;
  });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy seq,
                                                     const std::string_view &raw_query) const {
  return FindTopDocuments(seq, raw_query, DocumentStatus::ACTUAL);
}
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy par,
                                                     const std::string_view &raw_query,
                                                     DocumentStatus status) const {
  return FindTopDocuments(par, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
    return document_status == status;
  });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy par,
                                                     const std::string_view &raw_query) const {
  return FindTopDocuments(par, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
  return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
  return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
  return document_ids_.end();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view &raw_query,
                                                                                      int document_id) const {

  return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>
           , DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy seq,
                                                         const std::string_view &raw_query,
                                                         int document_id) const {
  const auto query = ParseQuery(raw_query);

  std::vector<std::string_view> matched_words;
  for (const std::string_view &word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(word).count(document_id)) {
      matched_words.push_back(word);
    }
  }
  for (const std::string_view &word : query.minus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(word).count(document_id)) {
      matched_words.clear();
      break;
    }
  }
  return {matched_words, documents_.at(document_id).status};

}

std::tuple<std::vector<std::string_view>
           , DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy par,
                                                         const std::string_view &raw_query,
                                                         int document_id) const {
  const auto query = ParseQuery(raw_query);

  std::vector<std::string_view> matched_words(query.plus_words.size());
  std::copy_if(par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&](const auto &word) {
    if (word_to_document_freqs_.count(word) != 0) {
      if (word_to_document_freqs_.at(word).count(document_id)) {
        return true;
      }
    }
    return false;
  });
  matched_words.erase(std::remove(matched_words.begin(),matched_words.end(), ""), matched_words.end());

  const auto minus_word_it = std::find_if(par, query.minus_words.begin(), query.minus_words.end(), [&](const auto &word) {
    if (word_to_document_freqs_.count(word) != 0) {
      if (word_to_document_freqs_.at(word).count(document_id)) {
          return true;
      }
    }
    return false;
  });
  if (minus_word_it != query.minus_words.end()) {
    matched_words.clear();
  }

  return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const std::string_view &word) const {
  return stop_words_.find(word) != stop_words_.end();
}

bool SearchServer::IsValidWord(const std::string_view &word) {
  // A valid word must not contain special characters
  return std::none_of(word.begin(), word.end(), [](char c) {
    return c >= '\0' && c < ' ';
  });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view &text) const {
  using namespace std::literals;
  std::vector<std::string_view> words;
  for (const std::string_view &word : SplitIntoWords(text)) {
    if (!IsValidWord(word)) {
      throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
    }
    if (!IsStopWord(word)) {
      words.push_back(word);
    }
  }
  return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int> &ratings) {
  if (ratings.empty()) {
    return 0;
  }
  int rating_sum = 0;
  for (const int rating : ratings) {
    rating_sum += rating;
  }
  return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view &text) const {
  using namespace std::literals;
  if (text.empty()) {
    throw std::invalid_argument("Query word is empty"s);
  }
  std::string_view word = text;
  bool is_minus = false;
  if (word[0] == '-') {
    is_minus = true;
    word = word.substr(1);
  }
  if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
    throw std::invalid_argument("Query word "s + std::string(word) + " is invalid");
  }

  return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view &text) const {
  Query result;
  for (const std::string_view &word : SplitIntoWords(text)) {
    const auto query_word = ParseQueryWord(word);
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        result.minus_words.insert(query_word.data);
      } else {
        result.plus_words.insert(query_word.data);
      }
    }
  }
  return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view &word) const {
  return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void SearchServer::RemoveDocument(int document_id) {
  RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy seq, int document_id) {
  const auto it_document_ids = document_ids_.find(document_id);
  if (it_document_ids == document_ids_.end()) {
    return;
  }
  document_ids_.erase(it_document_ids);
  for (const auto &[word, _] : SearchServer::GetWordFrequencies(document_id)) {
    const auto it_words_freq = word_to_document_freqs_.find(word);
    auto &map_id_freq = it_words_freq->second;
    map_id_freq.erase(document_id);
    if (map_id_freq.empty()) {
      word_to_document_freqs_.erase(it_words_freq);
    }
  }
  id_to_words_freqs_.erase(document_id);
  documents_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy par, int document_id) {
  const auto it_document_ids = document_ids_.find(document_id);
  if (it_document_ids == document_ids_.end()) {
    return;
  }
  document_ids_.erase(it_document_ids);
  const auto &words_to_del = SearchServer::GetWordFrequencies(document_id);
  std::mutex mx;
  std::for_each(par, words_to_del.begin(), words_to_del.end(), [&](const auto &word) {
    const auto it_words_freq = word_to_document_freqs_.find(word.first);
    auto &map_id_freq = it_words_freq->second;
    map_id_freq.erase(document_id);
    if (map_id_freq.empty()) {
      {
        std::lock_guard lock(mx);
        word_to_document_freqs_.erase(it_words_freq);
      }
    }
  });

  id_to_words_freqs_.erase(document_id);
  documents_.erase(document_id);
}

const std::map<std::string_view, double, std::less<>> &SearchServer::GetWordFrequencies(int document_id) const {
  const auto it = id_to_words_freqs_.find(document_id);
  if (it != id_to_words_freqs_.end()) {
    return it->second;
  } else {
    static const std::map<std::string_view, double, std::less<>> result;
    return result;
  }
}


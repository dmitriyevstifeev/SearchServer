#include "search_server.h"
#include "test_example_functions.h"
#include "process_queries.h"

#include <execution>
#include <iostream>
#include <string>
#include <vector>
#include <cassert>

using namespace std;

int main() {
  SearchServer search_server("and with"s);
  const std::vector<std::string> test_strings = {
      "white cat and yellow hat"s, "curly cat curly tail"s, "nasty dog with big eyes"s, "nasty pigeon john"s
  };
  for (int id = 1; id <= test_strings.size(); ++id) {
    search_server.AddDocument(id, test_strings[id - 1], DocumentStatus::ACTUAL, {1, 2});
  }

  cout << "ACTUAL by default:"s << endl;
  // последовательная версия
  for (const Document &document : search_server.FindTopDocuments("curly nasty cat"s)) {
    PrintDocument(document);
  }
  cout << "BANNED:"s << endl;
  // последовательная версия
  for (const Document &document : search_server.FindTopDocuments(execution::seq,
                                                                 "curly nasty cat"s,
                                                                 DocumentStatus::BANNED)) {
    PrintDocument(document);
  }

  cout << "Even ids:"s << endl;
  // параллельная версия
  for (const Document &document : search_server.FindTopDocuments(execution::par,
                                                                 "curly nasty cat"s,
                                                                 [](int document_id,
                                                                    DocumentStatus status,
                                                                    int rating) { return document_id % 2 == 0; })) {
    PrintDocument(document);
  }

  {
    SearchServer search_server(""sv);
    search_server.AddDocument(0, "one two three four five"s, DocumentStatus::ACTUAL, {0});
    const auto &wfs = search_server.GetWordFrequencies(0);
    for (const auto &[word, freq]: wfs) {
      std::cout << '"' << word << '"' << " - " << freq << '\n';
    }
  }

  {
    SearchServer server(""sv);
    server.AddDocument(0, "hello", DocumentStatus::ACTUAL, {0});
    const auto md = server.MatchDocument(std::execution::par, "hello", 0);
    assert(std::get<0>(md).size() == 1);
  }

  {
    SearchServer server(""sv);
    server.AddDocument(0, "hello", DocumentStatus::ACTUAL, {0});
    const auto md = server.MatchDocument(std::execution::par, "hello world", 0);
    assert(std::get<0>(md).size() == 1);
    std::cout << "Success" << endl;
    const auto md2 = server.MatchDocument(std::execution::seq, "hello world", 0);
    assert(md == md2);
    std::cout << "Success" << endl;
  }

  {
    SearchServer server(""sv);
    server.AddDocument(0, "hello", DocumentStatus::ACTUAL, {0});
    const auto md = server.MatchDocument(std::execution::par, "hello -world", 0);
    assert(std::get<0>(md).size() == 1);
    std::cout << "Success" << endl;
    const auto md2 = server.MatchDocument(std::execution::seq, "hello -world", 0);
    assert(md == md2);
    std::cout << "Success" << endl;
  }

  {
    SearchServer server("in on"sv);
    server.AddDocument(0, "hello in helloween", DocumentStatus::ACTUAL, {0});
    const auto md = server.MatchDocument(std::execution::par, "hello in -world"sv, 0);
    assert(std::get<0>(md).size() == 1);
    std::cout << "Success" << endl;
    const auto md2 = server.MatchDocument(std::execution::seq, "hello in -world"sv, 0);
    assert(md == md2);
    std::cout << "Success" << endl;
  }

  {
    SearchServer server("in on"sv);
    server.AddDocument(0, ""sv, DocumentStatus::ACTUAL, {0});
    const auto md = server.MatchDocument(std::execution::par, "hello in -world"sv, 0);
    assert(std::get<0>(md).size() == 0);
    std::cout << "Success" << endl;
    const auto md2 = server.MatchDocument(std::execution::seq, "hello in -world"sv, 0);
    assert(md == md2);
    std::cout << "Success" << endl;
  }

  {
    SearchServer server("in on"sv);
    server.AddDocument(0, ""sv, DocumentStatus::ACTUAL, {0});
    server.AddDocument(1, "hello world"sv, DocumentStatus::ACTUAL, {0});
    server.AddDocument(2, "hello perfect world"sv, DocumentStatus::ACTUAL, {0});
    const auto md = server.MatchDocument(std::execution::par, "hello world -world"sv, 1);
    assert(std::get<0>(md).size() == 0);
    std::cout << "Success" << endl;
    const auto md2 = server.MatchDocument(std::execution::seq, "hello world -world"sv, 1);
    assert(md == md2);
    std::cout << "Success" << endl;
  }

  {
    SearchServer server("in on"sv);
    server.AddDocument(0, ""sv, DocumentStatus::ACTUAL, {0});
    server.AddDocument(1, "hello world"sv, DocumentStatus::ACTUAL, {0});
    server.AddDocument(2, "hello perfect world"sv, DocumentStatus::ACTUAL, {0});
    const auto md = server.MatchDocument(std::execution::par, "hello world"sv, 1);
    assert(std::get<0>(md).size() == 2);
    std::cout << "Success" << endl;
    const auto md2 = server.MatchDocument(std::execution::seq, "hello world"sv, 1);
    assert(md == md2);
    std::cout << "Success" << endl;
  }

  {
    SearchServer server("in on"sv);
    server.AddDocument(0, ""sv, DocumentStatus::ACTUAL, {0});
    server.AddDocument(1, "hello world"sv, DocumentStatus::ACTUAL, {0});
    server.AddDocument(2, "hello perfect world"sv, DocumentStatus::ACTUAL, {0});
    const auto md = server.MatchDocument(std::execution::par, "hello world"sv, 0);
    assert(std::get<0>(md).size() == 0);
    std::cout << "Success" << endl;
    const auto md2 = server.MatchDocument(std::execution::seq, "hello world"sv, 0);
    assert(md == md2);
    std::cout << "Success" << endl;
  }

  {
    SearchServer server("in on"sv);
    server.AddDocument(0, "cat dog elephant"sv, DocumentStatus::ACTUAL, {0});
    const auto md = server.MatchDocument(std::execution::par, "mouse dog sharp"sv, 0);
    assert(std::get<0>(md).size() == 1);
    std::cout << "Success" << endl;
    const auto md2 = server.MatchDocument(std::execution::seq, "mouse dog sharp"sv, 0);
    assert(md == md2);
    std::cout << "Success" << endl;
  }

  return 0;
}
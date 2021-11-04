#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string &text) {
  std::vector<std::string> words;
  std::string word;
  for (const char c : text) {
    if (c == ' ') {
      if (!word.empty()) {
        words.push_back(word);
        word.clear();
      }
    } else {
      word += c;
    }
  }
  if (!word.empty()) {
    words.push_back(word);
  }

  return words;
}

std::vector<std::string_view> SplitIntoWords(const std::string_view &text) {
  std::vector<std::string_view> words;
  auto word_begin = text.begin();
  size_t word_end = 0;
  for (size_t i =0; i < text.size(); ++i) {
    if (text[i] == ' ') {
      if (word_end != 0) {
        std::string_view word(word_begin, word_end);
        words.push_back(word);
        word_begin = word_begin+word_end+1;
        word_end = 0;
      }
    } else {
      ++word_end;
    }
  }
  if (word_end != 0) {
    std::string_view word(word_begin, word_end);
    words.push_back(word);
  }

  return words;
}

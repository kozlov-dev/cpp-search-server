// search_server_s3_t1_v1.cpp

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
  string s;
  getline(cin, s);
  return s;
}

int ReadLineWithNumber() {
  int result;
  cin >> result;
  ReadLine();
  return result;
}
vector<string> SplitIntoWords(const string &text) {
  vector<string> words;
  string word;
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

struct Document {
  Document() = default;

  Document(int id, double relevance, int rating)
      : id(id), relevance(relevance), rating(rating) {}

  int id = 0;
  double relevance = 0.0;
  int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer &strings) {
  set<string> non_empty_strings;
  for (const string &str : strings) {
    if (!str.empty()) {
      non_empty_strings.insert(str);
    }
  }
  return non_empty_strings;
}

enum class DocumentStatus {
  ACTUAL,
  IRRELEVANT,
  BANNED,
  REMOVED,
};

class SearchServer {
 public:
  // Defines an invalid document id
  // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
  inline static constexpr int INVALID_DOCUMENT_ID = -1;

  template <typename StringContainer>
  explicit SearchServer(const StringContainer &stop_words)
      : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    for (const string &str : stop_words) {
      if (!IsValidWord(str)) {
        throw invalid_argument("Error stop word "s);
      }
    }
  }

  explicit SearchServer(const string &stop_words_text)
      : SearchServer(
            SplitIntoWords(stop_words_text))  // Invoke delegating constructor
  // from string container
  {}

  void AddDocument(int document_id, const string &document,
                   DocumentStatus status, const vector<int> &ratings) {
    if (IsNegativeID(
            document_id) ||  //Попытка добавить документ с отрицательным id;
        IsAddingExistingID(document_id)) {  // Попытка добавить документ c id
      // ранее добавленного документа;
      throw invalid_argument("Error document_id "s);
    }
    if (!IsValidWord(document)) {  //Наличие недопустимых символов (с кодами от
      // 0 до 31) в тексте добавляемого документа.
      throw invalid_argument("Error valid word"s);
    } else {
      const vector<string> words = SplitIntoWordsNoStop(document);
      const double inv_word_count = 1.0 / words.size();
      for (const string &word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
      }
      documents_.emplace(document_id,
                         DocumentData{ComputeAverageRating(ratings), status});
      documents_id_.push_back(document_id);
    }
  }

  vector<Document> FindTopDocuments(const string &raw_query,
                                    DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status,
                            int rating) { return document_status == status; });
  }

  template <typename DocumentPredicate>
  vector<Document> FindTopDocuments(
      const string &raw_query, DocumentPredicate document_predicate) const {
    // провка валидности запроса
    if (!IsValidWord(raw_query) || IsIllegalMinusWord(raw_query)) {
      throw invalid_argument("Error valid word");
    }

    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
           if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

  vector<Document> FindTopDocuments(const string &raw_query) const {
    if (!IsValidWord(raw_query) || IsIllegalMinusWord(raw_query)) {
      throw invalid_argument("Error valid word"s);
    } else {
      return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }
  }

  tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query,
                                                      int document_id) const {
    tuple<vector<string>, DocumentStatus> result = {};

    if (!IsValidWord(raw_query) || IsIllegalMinusWord(raw_query)) {
      throw invalid_argument("Error valid word"s);
    }
    const Query query = ParseQuery(raw_query);

    vector<string> matched_words;
    for (const string &word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.push_back(word);
      }
    }
    for (const string &word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.clear();
        break;
      }
    }

    result = {matched_words, documents_.at(document_id).status};
    return result;
  }

  int GetDocumentCount() const { return static_cast<int>(documents_.size()); }

  int GetDocumentId(int index) const {  // index - порядковый номер в documents_
    int countDoc = GetDocumentCount();
    if (index < 0 || index >= countDoc) {
      throw out_of_range("Index of document is out of range"s);
    }

    else {
      return documents_id_[index];
    }
  }

 private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
  };
  const set<string> stop_words_;
  map<string, map<int, double>> word_to_document_freqs_;
  map<int, DocumentData> documents_;
  vector<int> documents_id_;

  bool IsStopWord(const string &word) const {
    IsValidWord(word);
    return stop_words_.count(word) > 0;
  }

  vector<string> SplitIntoWordsNoStop(const string &text) const {
    vector<string> words;
    for (const string &word : SplitIntoWords(text)) {
      if (!IsStopWord(word)) {
        words.push_back(word);
      }
    }
    return words;
  }

  static int ComputeAverageRating(const vector<int> &ratings) {
    if (ratings.empty()) {
      return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
      rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
  }

  struct QueryWord {
    string data;
    bool is_minus;
    bool is_stop;
  };

  QueryWord ParseQueryWord(string text) const {
    if (!IsValidWord(text) || IsIllegalMinusWord(text)) {
      throw invalid_argument("Error valid word");
    }

    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
      is_minus = true;
      text = text.substr(1);
    }
    return {text, is_minus, IsStopWord(text)};
  }

  struct Query {
    set<string> plus_words;
    set<string> minus_words;
  };

  Query ParseQuery(const string &text) const {
    if (!IsValidWord(text) || IsIllegalMinusWord(text)) {
      throw invalid_argument("Error valid word");
    }

    Query query;
    for (const string &word : SplitIntoWords(text)) {
      QueryWord query_word = ParseQueryWord(word);
      if (!query_word.is_stop) {
        if (query_word.is_minus) {
          query.minus_words.insert(query_word.data);
        } else {
          query.plus_words.insert(query_word.data);
        }
      }
    }
    return query;
  }

  // Existence required
  double ComputeWordInverseDocumentFreq(const string &word) const {
    IsValidWord(word);
    return log(GetDocumentCount() * 1.0 /
               word_to_document_freqs_.at(word).size());
  }

  template <typename DocumentPredicate>
  vector<Document> FindAllDocuments(
      const Query &query, DocumentPredicate document_predicate) const {
    map<int, double> document_to_relevance;
    for (const string &word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
      for (const auto [document_id, term_freq] :
           word_to_document_freqs_.at(word)) {
        const auto &document_data = documents_.at(document_id);
        if (document_predicate(document_id, document_data.status,
                               document_data.rating)) {
          document_to_relevance[document_id] +=
              term_freq * inverse_document_freq;
        }
      }
    }

    for (const string &word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
        document_to_relevance.erase(document_id);
      }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
      matched_documents.push_back(
          {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
  }

  static bool IsNegativeID(int document_id) { return document_id < 0; }

  bool IsAddingExistingID(int document_id) const {
    return documents_.count(document_id);
  }

  // === Error handling ===========================

  static bool IsValidWord(const string &word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(),
                   [](char c) { return c >= '\0' && c < ' '; });
  }

  static bool IsIllegalMinusWord(const string &raw_query) {
    if (raw_query.back() == '-') {  // кот-
      return true;
    } else {
      for (int i = 0; i < raw_query.size(); ++i) {
        if (raw_query[i] == '-' && raw_query[i + 1] == '-') {  //--кот
          return true;
        }
      }
    }
    return false;
  }
};

// ------------ Пример использования ----------------

void PrintDocument(const Document &document) {
  cout << "{ "s
       << "document_id = "s << document.id << ", "s
       << "relevance = "s << document.relevance << ", "s
       << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string> &words,
                              DocumentStatus status) {
  cout << "{ "s
       << "document_id = "s << document_id << ", "s
       << "status = "s << static_cast<int>(status) << ", "s
       << "words ="s;
  for (const string &word : words) {
    cout << ' ' << word;
  }
  cout << "}"s << endl;
}

void AddDocument(SearchServer &search_server, int document_id,
                 const string &document, DocumentStatus status,
                 const vector<int> &ratings) {
  try {
    search_server.AddDocument(document_id, document, status, ratings);
  } catch (const exception &e) {
    cout << "Ошибка добавления документа "s << document_id << ": "s << e.what()
         << endl;
  }
}

void FindTopDocuments(const SearchServer &search_server,
                      const string &raw_query) {
  cout << "Результаты поиска по запросу: "s << raw_query << endl;
  try {
    for (const Document &document : search_server.FindTopDocuments(raw_query)) {
      PrintDocument(document);
    }
  } catch (const exception &e) {
    cout << "Ошибка поиска: "s << e.what() << endl;
  }
}

void MatchDocuments(const SearchServer &search_server, const string &query) {
  try {
    cout << "Матчинг документов по запросу: "s << query << endl;
    const int document_count = search_server.GetDocumentCount();
    for (int index = 0; index < document_count; ++index) {
      const int document_id = search_server.GetDocumentId(index);
      //  cout<<document_id<<endl;
      const auto [words, status] =
          search_server.MatchDocument(query, document_id);
      PrintMatchDocumentResult(document_id, words, status);
    }
  } catch (const exception &e) {
    cout << "Ошибка матчинга документов на запрос "s << query << ": "s
         << e.what() << endl;
  }
}

int main() {
  SearchServer search_server("и в на"s);

  AddDocument(search_server, 1, "пушистый кот пушистый хвост"s,
              DocumentStatus::ACTUAL, {7, 2, 7});
  AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s,
              DocumentStatus::ACTUAL, {1, 2});
  AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s,
              DocumentStatus::ACTUAL, {1, 2});
  AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s,
              DocumentStatus::ACTUAL, {1, 3, 2});
  AddDocument(search_server, 4, "большой пёс скворец евгений"s,
              DocumentStatus::ACTUAL, {1, 1, 1});

  FindTopDocuments(search_server, "пушистый -пёс"s);
  FindTopDocuments(search_server, "пушистый --кот"s);
  FindTopDocuments(search_server, "пушистый -"s);

  MatchDocuments(search_server, "пушистый пёс"s);
  MatchDocuments(search_server, "модный -кот"s);
  MatchDocuments(search_server, "модный --пёс"s);
  MatchDocuments(search_server, "пушистый - хвост"s);
}

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cstdlib>
#include <iomanip>

using namespace std;

//*******************************************************************************************
template<typename Container>
void Print(ostream &os, const Container &container) {
    bool is_first = true;
    for (const auto &element : container) {
        if (!is_first) {
            os << ", "s;
        }
        is_first = false;
        os << element;
    }
}


// перегрузка<< для pair
template<typename Key, typename Value>
ostream &operator<<(ostream &os, const pair<Key, Value> &container) {
    os << container.first << ": "s << container.second;
    return os;
}

// перегрузка<< для vector
template<typename Element>
ostream &operator<<(ostream &os, const vector<Element> &container) {
    os << "["s;
    Print(os, container);
    os << "]"s;
    return os;
}

// перегрузка<< для set
template<typename Element>
ostream &operator<<(ostream &os, const set<Element> &container) {
    os << "{"s;
    Print(os, container);
    os << "}"s;
    return os;
}

// перегрузка<< для map
template<typename Key, typename Value>
ostream &operator<<(ostream &os, const map<Key, Value> &container) {
    os << "{"s;
    Print(os, container);
    os << "}"s;
    return os;
}

template<typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str, const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
                const string &hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


template<class TestFunc>
void RunTestImpl(TestFunc t_func, const string &func) {
    cerr << func;
    t_func();
    cerr << " OK";
    cerr << endl;
}

#define RUN_TEST(func) RunTestImpl((func),#func)

//*******************************************************************************************

const int MAX_RESULT_DOCUMENT_COUNT = 5;

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
    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

//*******************************************************************************************

class SearchServer {
public:
    void SetStopWords(const string &text) {
        for (const string &word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string &document,
                     DocumentStatus status, const vector<int> &ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string &word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
                           DocumentData{ComputeAverageRating(ratings), status});
    }

    template<typename PredicateDocFilter>
    vector<Document> FindTopDocuments(const string &raw_query,
                                      PredicateDocFilter predicate) const {
        const Query query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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
        return FindTopDocuments(
                raw_query, [](int document_id, DocumentStatus status, int rating) {
                    return status == DocumentStatus::ACTUAL;
                });
    }

    vector<Document> FindTopDocuments(const string &raw_query,
                                      DocumentStatus status_) const {
        return FindTopDocuments(
                raw_query, [status_](int document_id, DocumentStatus status,
                                     int rating) { return status == status_; });
    }

    int GetDocumentCount() const { return documents_.size(); }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query,
                                                        int document_id) const {
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
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string &word) const {
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
        bool is_minus = false;
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
        Query query;
        for (const string &word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
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
        return log(GetDocumentCount() * 1.0 /
                   word_to_document_freqs_.at(word).size());
    }

    template<typename PredicateDocFilter>
    vector<Document> FindAllDocuments(const Query &query,
                                      PredicateDocFilter predicate) const {
        map<int, double> document_to_relevance;
        for (const string &word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq =
                    ComputeWordInverseDocumentFreq(word);
            for (const auto[document_id, term_freq] :
                    word_to_document_freqs_.at(word)) {
                if (predicate(document_id, documents_.at(document_id).status,
                              documents_.at(document_id).rating)) {
                    // if (documents_.at(document_id).status == status) {
                    document_to_relevance[document_id] +=
                            term_freq * inverse_document_freq;
                }
            }
        }

        for (const string &word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto[document_id, _] :
                    word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto[document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

void PrintDocument(const Document &document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

//*******************************************************************************************//

// -------- Начало модульных тестов поисковой системы ----------
struct TestingDocs {
    const int id;
    const string text;
    DocumentStatus status;
    const vector<int> ratings;
};

//[+] 1.Добавление документов. Добавленный документ должен находиться по
//поисковому запросу, который содержит слова из документа.
void TestAddDocument() {
    {
        // Тестируем пустой сервер
        SearchServer server_test;
        ASSERT(server_test.FindTopDocuments("кот модный"s).empty());
    }
    {
        // Добавляем документ
        SearchServer server_test;
        server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});

        ASSERT_EQUAL(server_test.GetDocumentCount(), 1);
        ASSERT_EQUAL(server_test.FindTopDocuments("кот модный"s).size(), 1U);
        ASSERT_EQUAL(server_test.FindTopDocuments("кот модный"s)[0].id, 0);
    }
}

// [+] 2.Тест проверяет, что поисковая система исключает стоп-слова при
// добавлении документов Тест проверяет, что поисковая система исключает
// стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server_test;
        // Добавляем документ
        server_test.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        // Тестируем заполненный сервер
        ASSERT_EQUAL(server_test.FindTopDocuments("in"s).size(), 1U);
        ASSERT_EQUAL(server_test.FindTopDocuments("in"s)[0].id, doc_id);
    }
    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server_test;
        server_test.SetStopWords("in the"s);
        server_test.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server_test.FindTopDocuments("in"s).empty());
    }

}

//[-] Поддержка минус - слов. Документы, содержащие минус - слова поискового
//запроса, не должны включаться в результаты поиска.
void TestExcludeMinusWordsFromAddedDocument() {
    {
        // Тестируем пустой сервер
        SearchServer server_test;
        ASSERT(server_test.FindTopDocuments("кот и"s).empty());
        ASSERT(server_test.FindTopDocuments("-кот и"s).empty());

        // Добавляем документ
        server_test.AddDocument(0, "белый кот и модный поводок"s, DocumentStatus::ACTUAL, {8, -3});

        // Тестируем заполненный сервер
        ASSERT_EQUAL(server_test.GetDocumentCount(), 1);
        ASSERT_EQUAL(server_test.FindTopDocuments(" кот модный"s).size(), 1U);  // плюс-слова
        ASSERT_EQUAL(server_test.FindTopDocuments("кот модный"s)[0].id, 0);

        ASSERT(server_test.FindTopDocuments("-кот -модный"s).empty());          // минус-слова
        ASSERT_EQUAL(server_test.FindTopDocuments("-кот -модный"s).size(), 0U);

        ASSERT(server_test.FindTopDocuments("модный -модный"s).empty());        // корнер
        ASSERT_EQUAL(server_test.FindTopDocuments("-модный"s).size(), 0U);

        ASSERT(server_test.FindTopDocuments(""s).empty());
    }
}

/*
[-] Матчинг документов.При матчинге документа по поисковому запросу должны быть
    возвращены все слова из поискового запроса, присутствующие в документе.
    Если есть соответствие хотя бы по одному минус -слову, должен возвращаться
    пустой список слов.
*/
void TestMatchingWordsFromAddedDocumentContent() {
    {
        // Тестируем пустой сервер
        SearchServer server_test;
        ASSERT(server_test.FindTopDocuments("кот модный"s).empty());
    }

    SearchServer server_test;

    // Добавляем стоп слово
    server_test.SetStopWords("и"s);

    // Добавляем документы
    server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    server_test.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server_test.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server_test.AddDocument(3, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});
    server_test.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

    // Тестируем заполненный сервер
    ASSERT_EQUAL(server_test.GetDocumentCount(), 5);

    //проверяем, 2 слова из поискового запроса
    auto doc_1 = server_test.MatchDocument("белый и ошейник"s, 0);
    vector plus_words = get<0>(doc_1);
    ASSERT_EQUAL(plus_words.size(), 2);

    //в запросе минус слова --> 0 слов?
    auto doc_2 = server_test.MatchDocument("-пушистый -хвост"s, 1);
    vector minus_words = get<0>(doc_2);
    ASSERT_EQUAL(minus_words.size(), 0); //в запросе минус слова --> 0 слов?

    //в запросе минус слова --> 0 слов?
    auto doc_3 = server_test.MatchDocument("ухоженный -пёс"s, 2);
    vector plus_words2 = get<0>(doc_3);
    ASSERT_EQUAL(plus_words2.size(), 0); //в запросе минус слова --> 0 слов?

//4)Матчинг документов. При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса,
// присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
}

/*
[-] Сортировка найденных документов по релевантности.
    Возвращаемые при поиске документов результаты должны быть отсортированы
    в порядке убывания релевантности.
*/
void TestSortingRelevanceDocument() {
    {
        // Тестируем пустой сервер
        SearchServer server_test;
        ASSERT(server_test.FindTopDocuments("кот модный"s).empty());
    }
    {
        SearchServer server_test;

        // Добавляем стоп слово
        server_test.SetStopWords("и"s);

        // Добавляем документы
        server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3}); // avr 2.5
        server_test.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server_test.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server_test.AddDocument(3, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});
        server_test.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

        // Тестируем заполненный сервер
        auto funding_docs = server_test.FindTopDocuments("ухоженный пёс"s);  // сначала актуальный
        // [id 3,rel 0.35677908891003646, rating 0]
        // [id 2,rel 0.35677908891003646, rating -1]
        ASSERT_EQUAL(funding_docs.size(), 2);

        ASSERT_EQUAL(funding_docs[0].id, 3);
        ASSERT_EQUAL(funding_docs[1].id, 2);

        auto found_docs2 = server_test.FindTopDocuments("ухоженный"s);  // сначала актуальный
        auto found_docs3 = server_test.FindTopDocuments("ухоженный"s);
//        // [id 3,rel 0.12770640594149768, rating 0]
//        // [id 2,rel 0.12770640594149768, rating -1]
        ASSERT_EQUAL(found_docs2.size(), 2);
        ASSERT_EQUAL(found_docs2[0].id, 3);
        ASSERT_EQUAL(found_docs2[1].id, 2);

    }
}

/*
[+] Вычисление рейтинга документов.Рейтинг добавленного документа равен
    среднему арифметическому оценок документа.
 */
void TestAvgRatingsSearchedDocumentContent() {
    {
        SearchServer server_test;

        // Добавляем стоп слово
        server_test.SetStopWords("и"s);

        // Добавляем документы
        server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server_test.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server_test.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server_test.AddDocument(3, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});
        server_test.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

        // Тестируем заполненный сервер
        ASSERT_EQUAL(server_test.GetDocumentCount(), 5);

        auto find = server_test.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(find.size(), 4U);

        ASSERT_EQUAL(find[0].rating, 5);
        ASSERT_EQUAL(find[1].rating, 2);
        ASSERT_EQUAL(find[2].rating, 0);
        ASSERT_EQUAL(find[3].rating, -1);
    }

}

// [-] Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestCustomSearchDocumentContent() {
    {
        {
            SearchServer server_test;

            // Добавляем стоп слово
            server_test.SetStopWords("и"s);

            // Добавляем документы
            server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
            server_test.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
            server_test.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
            server_test.AddDocument(3, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});
            server_test.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

            // Тестируем заполненный сервер
            ASSERT_EQUAL(server_test.GetDocumentCount(), 5);

            auto find_string = server_test.FindTopDocuments("пушистый ухоженный кот"s);
            ASSERT_EQUAL(find_string.size(), 4U);

            auto predicat_status_rating = []([[maybe_unused]] int document_id, DocumentStatus status, int rating) {
                return rating >= 4;
            };
            auto found_docs = server_test.FindTopDocuments("ухоженный кот и пёс"s, predicat_status_rating);
            ASSERT_EQUAL(found_docs.size(), 2U);

            auto predicat_status = []([[maybe_unused]] int document_id, DocumentStatus status, int rating) {
                return (status == DocumentStatus::BANNED) || (status == DocumentStatus::REMOVED);
            };
            found_docs = server_test.FindTopDocuments("пушистый ухоженный кот"s, predicat_status);
            ASSERT_EQUAL(found_docs.size(), 1U);
            ASSERT_EQUAL(found_docs[0].id, 4);

            auto predicat_id = []([[maybe_unused]] int document_id, DocumentStatus status, int rating) {
                return (document_id % 2 == 0);
            };
            found_docs = server_test.FindTopDocuments("пушистый ухоженный кот"s, predicat_id);
            ASSERT_EQUAL(found_docs.size(), 3U);
            ASSERT_EQUAL(found_docs[0].id, 0);
            ASSERT_EQUAL(found_docs[1].id, 4);
            ASSERT_EQUAL(found_docs[2].id, 2);
        }
    }
}

/*
[-] Поиск документов, имеющих заданный статус.
*/
void TestStatusSearchedDocument() {
    {
        SearchServer server_test;

        // Добавляем стоп слово
        server_test.SetStopWords("и"s);

        // Добавляем документы
        server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server_test.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server_test.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server_test.AddDocument(3, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});
        server_test.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});
        server_test.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::IRRELEVANT, {9});
        server_test.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::REMOVED, {5, -12, 2, 1});
        server_test.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::IRRELEVANT, {9});
        server_test.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::REMOVED, {5, -12, 2, 1});

        // Тестируем заполненный сервер
        ASSERT_EQUAL(server_test.GetDocumentCount(), 5);

        auto find = server_test.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(find.size(), 4U);

        string query = "выразительные глаза"s;
        auto find_doc = server_test.FindTopDocuments(query);
        ASSERT_EQUAL(server_test.FindTopDocuments(query).size(), 2U);
        ASSERT_EQUAL(find_doc[0].id, 2);

        auto actual = server_test.FindTopDocuments(query, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(actual.size(), 2U);
        ASSERT_EQUAL(actual[0].id, 2);
        ASSERT_EQUAL(actual[1].id, 3);

        auto banned = server_test.FindTopDocuments(query, DocumentStatus::BANNED);
        ASSERT_EQUAL(banned.size(), 0U);

        auto irrelevant = server_test.FindTopDocuments("выразительные глаза"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(irrelevant.size(), 0U);

        auto removed = server_test.FindTopDocuments("выразительные глаза"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(removed.size(), 0U);
    }
}

/*
[-] Корректное вычисление релевантности найденных
документов.
 */
void TestRelevanceSearchedDocumentContent() {
    {

        {
            // Тестируем пустой сервер
            SearchServer server_test;
            ASSERT(server_test.FindTopDocuments("кот модный"s).empty());
        }

        SearchServer server_test;

        // Добавляем стоп слово
        server_test.SetStopWords("и"s);

        // Добавляем документы
        server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server_test.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server_test.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server_test.AddDocument(3, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});
        server_test.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});


        // Тестируем заполненный сервер
        ASSERT_EQUAL(server_test.GetDocumentCount(), 5);
        const auto found_docs = server_test.FindTopDocuments("и кот ухоженный");
        vector<double> test_relevance = {0.22907, 0.22907, 0.12770, 0.12770};
        for (size_t i = 0; i < test_relevance.size(); i++) {
            ASSERT(std::abs(test_relevance[i] - found_docs[i].relevance) < 1E-5);
        }
    }

}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWordsFromAddedDocument);
    RUN_TEST(TestMatchingWordsFromAddedDocumentContent);
    RUN_TEST(TestSortingRelevanceDocument);
    RUN_TEST(TestAvgRatingsSearchedDocumentContent);
    RUN_TEST(TestCustomSearchDocumentContent);
    RUN_TEST(TestStatusSearchedDocument);
    RUN_TEST(TestRelevanceSearchedDocumentContent);
}

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
#pragma once

#include <algorithm>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"
#include <execution>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer
{

public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words);
    explicit SearchServer(const std::string &stop_words_text);
    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status,
                     const std::vector<int> &ratings);

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentStatus status) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <class ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    //------------------------------------------------------------------------------------------------------------

    int GetDocumentCount() const;

    std::set<int>::iterator begin() const;
    std::set<int>::iterator end() const;

    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::sequenced_policy seq_police, int document_id);
    void RemoveDocument(std::execution::parallel_policy par_police, int document_id);

    const std::map<std::string_view, double> &GetWordFrequencies(int document_id) const;

    using MatchResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    MatchResult MatchDocument(std::string_view raw_query, int document_id) const;
    MatchResult MatchDocument(std::execution::sequenced_policy seq_police, std::string_view raw_query, int document_id) const;
    MatchResult MatchDocument(const std::execution::parallel_policy &par_police, std::string_view raw_query, int document_id) const;

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
        std::set<std::string, std::less<>> words;
    };

    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_; // <word<doc_id,freq>>
    std::map<int, DocumentData> documents_;

    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;

    //-------------------------------------------------------------------------------------
    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int> &ratings);

    struct QueryWord
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view &text) const;

    struct Query
    {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text, bool needUnique = true) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query &query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy seq_police, const Query &query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy par_police, const Query &query, DocumentPredicate document_predicate) const;
};
//-------------------------------------------------------------------------------------

void AddDocument(SearchServer &search_server, int document_id, std::string_view document,
                 DocumentStatus status, const std::vector<int> &ratings);

void FindTopDocuments(const SearchServer &search_server, std::string_view raw_query);

void MatchDocuments(const SearchServer &search_server, std::string_view query);

void RemoveDuplicates(SearchServer &search_server);

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer &stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) // Extract non-empty stop words
{
    using namespace std::string_literals;
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord))
    {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query) const
{
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(policy, raw_query,
                            [status](int document_id, DocumentStatus document_status, int rating)
                            {
                                return document_status == status;
                            });
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const
{
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <class ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentPredicate document_predicate) const
{
    const auto query = ParseQuery(raw_query, true);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs)
         {
             return lhs.relevance > rhs.relevance || (std::abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query &query, DocumentPredicate document_predicate) const
{
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy seq_police, const Query &query, DocumentPredicate document_predicate) const
{
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
        {
            const auto &document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating))
            {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (std::string_view word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word))
        {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy par_police, const Query &query, DocumentPredicate document_predicate) const

{
    ConcurrentMap<int, double> document_to_relevance(101);

    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [this, &document_to_relevance, &document_predicate](std::string_view word)
                  {
        if ( word_to_document_freqs_.count(word) != 0 ) {
            auto it = word_to_document_freqs_.find(word);
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for ( const auto [document_id, term_freq] : it->second ) {
                const auto &document_data = documents_.at(document_id);
                if ( document_predicate(document_id, document_data.status, document_data.rating) ) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        } });

    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [this, &document_to_relevance](std::string_view word)
                  {
        if ( word_to_document_freqs_.count(word) != 0 ) {
            auto it = word_to_document_freqs_.find(word);
            for ( const auto [document_id, _] : it->second ) {
                document_to_relevance.Erase(document_id);
            }
        } });

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap())
    {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}
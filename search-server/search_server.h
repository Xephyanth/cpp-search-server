#pragma once

#include "document.h"
#include "string_processing.h"

#include <string>
#include <set>
#include <vector>
#include <tuple>
#include <map>
#include <algorithm>
#include <stdexcept>

using std::literals::string_literals::operator""s;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double accuracy = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
        if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw std::invalid_argument("Some of stop words are invalid"s);
        }
    }

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {}
    
    void AddDocument(int document_id, const std::string& document,
                     DocumentStatus status, const std::vector<int>& ratings);
    
    void RemoveDocument(int document_id);
    
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;
    
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;
    
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;
    
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(
        const std::string& raw_query, int document_id) const;

    // Метод получения частот слов по ID документа
    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;
    
    size_t GetDocumentCount() const;
    
    std::set<int>::const_iterator begin() const {
        return document_ids_.begin();
    }

    std::set<int>::const_iterator end() const {
        return document_ids_.end();
    }
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    const std::set<std::string> stop_words_;
    // Слово, Документ - TF
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    // Частота слов по ID док-та
    std::map<int, std::map<std::string, double>> document_to_word_freqs_;
    // Документ, Рейтинг - Статус
    std::map<int, DocumentData> documents_;
    // Порядковый номер ~ ID док-та
    std::set<int> document_ids_;
    
    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };
    
    bool IsStopWord(const std::string& word) const;

    static bool IsValidWord(const std::string& word);
    
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
    
    static int ComputeAverageRating(const std::vector<int>& ratings);

    double ComputeWordInverseDocumentFreq(const std::string& word) const;
    
    QueryWord ParseQueryWord(std::string text) const;

    Query ParseQuery(const std::string& raw_query) const;
    
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
};

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(
    const std::string& raw_query, DocumentPredicate document_predicate) const
{
    const auto query = ParseQuery(raw_query);
    
    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < accuracy) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        }
    );
    
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
    const Query& query, DocumentPredicate document_predicate) const
{
    std::map<int, double> document_to_relevance;
    
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }
    
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating }
        );
    }
    
    return matched_documents;
}


// 


void AddDocument(SearchServer& search_server, int document_id,
    const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
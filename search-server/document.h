//
// Created by DevStation on 20.02.2022.
//
#pragma once


#include <string>
#include <vector>
#include <map>
#include <iostream>

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating);

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

std::ostream &operator<<(std::ostream &out, const Document &document);

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

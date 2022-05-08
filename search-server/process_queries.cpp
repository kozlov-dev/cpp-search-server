#include "process_queries.h"
#include <execution>
#include <iterator>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    
    std::vector<std::vector<Document>> documents_lists(queries.size());    
    transform(std::execution::par, queries.begin(), queries.end(), documents_lists.begin(),
              [&search_server](const std::string& query) { 
                  return search_server.FindTopDocuments(query); 
    });
    
    /*for (const std::string& query : queries) {
        documents_lists.push_back(search_server.FindTopDocuments(query));
    }*/
 
    return documents_lists;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> documents_lists(queries.size());    
    transform(std::execution::par, queries.begin(), queries.end(), documents_lists.begin(),
              [&search_server](const std::string& query) { 
                  return search_server.FindTopDocuments(query); 
    });
    
    size_t size = 0;
    for (const auto& documents_list: documents_lists){
        size += documents_list.size();
    }
    /*auto size = transform_reduce(matrix.begin(), matrix.end(), 
        plus<>{},  // reduce-операция (группирующая функция)
        [](const vector<int>& v) { return v.size(); });*/

    std::vector<Document> result;
    result.reserve(size);
    auto it_begin = result.begin();
    for (auto& documents_list: documents_lists) {
        result.resize(result.size() + documents_list.size());
        it_begin = transform(std::make_move_iterator(documents_list.begin()), std::make_move_iterator(documents_list.end()), it_begin,
              [](const Document& value) { 
                  return value;
        });
    }
    
    return result;
}
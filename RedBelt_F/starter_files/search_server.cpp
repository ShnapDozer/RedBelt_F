#include "search_server.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>
#include <deque>
#include <windows.h>
#include "iterator_range.h"
#include "profile.h"


std::vector<std::string> SplitIntoWords(std::string line)
{
    std::istringstream words_input(move(line));
 
  return { std::istream_iterator<std::string>(words_input), std::istream_iterator<std::string>() };
}

void Perform_Fur(vector<future<std::string>> requests, std::ostream& search_results_output)
{
    for (auto& i : requests) { search_results_output << i.get(); }
}

SearchServer::SearchServer(std::istream& document_input) { UpdateDocumentBase(document_input); }

void SearchServer::UpdateDocumentBase(std::istream& document_input)
{
    std::unique_lock<std::shared_mutex> Lock_Write(Mut_Th);

    LOG_DURATION("UpdateDocumentBase");

    InvertedIndex* new_index = new InvertedIndex;

    for (std::string current_document; std::getline(document_input, current_document); )
    {
        new_index->Add(move(current_document));
    }

    delete(index);
    index = new_index;
}

std::string SearchServer::SingleRequest(std::string current_query)
{
    std::shared_lock<std::shared_mutex> Lock_Read(Mut_Th);

    std::string search_results_output;

    const auto words = SplitIntoWords(current_query);

    map<size_t, size_t> docid_count;

    for (const auto& word : words) 
    {
        for (const size_t docid : index->Lookup(word)) 
        {
            docid_count[docid]++;
        }
    }

    std::vector<pair<size_t, size_t>> search_results;
    search_results.reserve(5);

    std::deque< pair<size_t, map<size_t, size_t>::iterator>> Maxs_pos(1);

    for (auto i = docid_count.begin(); i != docid_count.end(); ++i)
    {
        if (i->second > Maxs_pos.back().first) 
        {
            for (auto j = Maxs_pos.begin(); j != Maxs_pos.end(); ++j)
            {
                if (i->second > j->first)
                {
                    Maxs_pos.insert(j, { i->second, i });
                    if (Maxs_pos.size() > 5) { Maxs_pos.pop_back(); }
                    break;
                }
            }
        }
    }

    for (auto i : Maxs_pos) 
    {
        if (i.first == 0) { break; }
        search_results.push_back(move(*i.second));
    }
    
    search_results_output += current_query;
    search_results_output +=':';

    for (auto [docid, hitcount] : search_results)
    {
        search_results_output += " {docid: ";
        search_results_output += std::to_string(docid);
        search_results_output +=  ", hitcount: ";
        search_results_output += std::to_string(hitcount);
        search_results_output += '}';
    }

    search_results_output += "\n";

    return search_results_output;
}

void SearchServer::AddQueriesStream(std::istream& query_input, std::ostream& search_results_output)
{
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);

    size_t Count_Threads = sysinfo.dwNumberOfProcessors;

    std::vector<std::future<std::string>> Requests;
    Requests.reserve(Count_Threads);

    for (std::string current_query; getline(query_input, current_query); )
    {
        if (Requests.size() == Requests.capacity()) 
        {
            Perform_Fur(move(Requests), search_results_output);
            Requests.reserve(Count_Threads);
        }
        Requests.push_back(async(&SearchServer::SingleRequest, this, 
            move(current_query)));
    }

    Perform_Fur(move(Requests), search_results_output);
}

SearchServer::~SearchServer() { delete(index); }

void InvertedIndex::Add(std::string document) 
{
    docs.push_back(move(document));

    const size_t docid = docs.size() - 1;
    for (const auto& word : SplitIntoWords(docs.back())) { index[word].push_back(docid); }
}

std::list<size_t> InvertedIndex::Lookup(const std::string& word) const
{
    auto it = index.find(word);
    
    if (it != index.end()) 
    {
        return it->second;
    }
    else {
        return {};
    }
}

//#include "search_server.h"
//#include "profile.h"
//#include "iterator_range.h"
//
//#include <algorithm>
//#include <iterator>
//#include <sstream>
//#include <iostream>
//
////----------------------------------------------------------------------------------------------------
//std::vector<std::string_view> SplitIntoWords(std::string_view line)
//{
//    std::vector<std::string_view> result;
//    size_t curr = line.find_first_not_of(' ', 0);
//    while (true)
//    {
//        auto space = line.find(' ', curr);
//        result.emplace_back(line.substr(curr, space - curr));
//
//        if (space == line.npos)
//        {
//            break;
//        }
//        else
//        {
//            curr = line.find_first_not_of(' ', space);
//        }
//
//        if (curr == line.npos) break;
//    }
//    return result;
//}
////----------------------------------------------------------------------------------------------------
//SearchServer::SearchServer(std::istream& document_input)
//{
//    UpdateDocumentBase(document_input);
//}
////----------------------------------------------------------------------------------------------------
//SearchServer::~SearchServer()
//{
//    std::for_each(vec_futures.begin(), vec_futures.end(), [](std::future<void>& future)
//        {
//            future.get();
//        });
//}
////----------------------------------------------------------------------------------------------------
//void SearchServer::UpdateDocumentBase(std::istream& document_input)
//{
//    LOG_DURATION("UpdateDocumentBase")
//        InvertedIndex new_index;
//    for (std::string current_document; getline(document_input, current_document); )
//    {
//        new_index.Add(move(current_document));
//    }
//
//    {
//        std::lock_guard<std::mutex> lock(mutex);
//        std::swap(new_index, index);
//    }
//}
////----------------------------------------------------------------------------------------------------
//void SearchServer::AddQueriesStream(std::istream& query_input, std::ostream& search_results_output)
//{
//    LOG_DURATION("AddQueriesStream")
//
//        auto future = [&query_input, &search_results_output, this]()
//    {
//
//        std::vector<size_t> docs(50000);
//        std::vector<size_t> indx(50000);
//        for (std::string current_query; getline(query_input, current_query);)
//        {
//            size_t curr_ind = 0;
//            for (const auto& word : SplitIntoWords(current_query))
//            {
//                std::vector<std::pair<size_t, size_t>> vec;
//                {
//                    std::lock_guard<std::mutex> lock(mutex);
//                    vec = index.Lookup(word);
//                }
//                for (const auto& [docid, count] : vec)
//                {
//                    if (docs[docid] == 0)
//                    {
//                        indx[curr_ind++] = docid;
//                    }
//                    docs[docid] += count;
//                }
//            }
//
//            std::vector<std::pair<size_t, size_t>> search_result;
//            for (size_t docid = 0; docid < curr_ind; ++docid)
//            {
//                size_t count = 0;
//                size_t id = 0;
//                std::swap(count, docs[indx[docid]]);
//                std::swap(id, indx[docid]);
//                search_result.emplace_back(id, count);
//            }
//
//            const size_t ANSWERS_COUNT = 5;
//            std::partial_sort(std::begin(search_result), std::begin(search_result) +
//                std::min<size_t>(ANSWERS_COUNT, search_result.size()), std::end(search_result),
//                [](std::pair<size_t, size_t> lhs, std::pair<size_t, size_t> rhs)
//                {
//                    int64_t lhs_docid = lhs.first;
//                    auto lhs_hit_count = lhs.second;
//                    int64_t rhs_docid = rhs.first;
//                    auto rhs_hit_count = rhs.second;
//                    return std::make_pair(lhs_hit_count, -lhs_docid) > std::make_pair(rhs_hit_count, -rhs_docid);
//                });
//
//            search_results_output << current_query << ':';
//            for (auto [docid, hitcount] : Head(search_result, ANSWERS_COUNT))
//            {
//                search_results_output << " {"
//                    << "docid: " << docid << ", "
//                    << "hitcount: " << hitcount << '}';
//            }
//            search_results_output << "\n";
//        }
//    };
//    vec_futures.push_back(std::async(future));
//}
////----------------------------------------------------------------------------------------------------
//void InvertedIndex::Add(std::string&& document)
//{
//    docs.push_back(std::move(document));
//    const size_t docid = docs.size() - 1;
//
//    for (const auto& word : SplitIntoWords(docs.back()))
//    {
//        auto& vec_pair = index[word];
//        if (!vec_pair.empty() && vec_pair.back().first == docid)
//        {
//            vec_pair.back().second += 1;
//        }
//        else
//        {
//            vec_pair.emplace_back(docid, 1);
//        }
//    }
//}
////----------------------------------------------------------------------------------------------------
//std::vector<std::pair<size_t, size_t>> InvertedIndex::Lookup(std::string_view word) const
//{
//    if (auto it = index.find(word); it != index.end())
//    {
//        return it->second;
//    }
//    else
//    {
//        return {};
//    }
//}
////----------------------------------------------------------------------------------------------------
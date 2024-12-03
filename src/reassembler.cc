#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
    if(is_last_substring){
        eol = first_index + data.size();
    }
    uint64_t windowBegin = output_.writer().bytes_pushed();
    uint64_t windowEnd = windowBegin + output_.writer().available_capacity();
    uint64_t last_index = first_index + data.size();
    if(first_index < windowBegin) {
        data = data.erase(0, std::min(windowBegin, last_index) - first_index);
        first_index = windowBegin;
    }
    if(last_index > windowEnd){
        data = data.substr(0, data.size() - last_index + std::max(first_index, windowEnd));
        last_index = windowEnd;
    }
    if(data.empty()){
        if(output_.writer().bytes_pushed() >= eol){
            output_.writer().close();
        }
        return;
    }
    if(intervals.empty() && first_index <= windowEnd){
        intervals.emplace_back(first_index, data);
    }
    auto it = intervals.begin();
    while(it != intervals.end() && it->first + it->second.size() < first_index)
        it++;

    if(it != intervals.end()){
        auto it2 = it;
        uint64_t maxidx = last_index;
        uint64_t leftmostIntFirst = std::min(it->first, first_index);
        std::string rightmostIntDat;
        int64_t offset = first_index - it->first;
        std::string leftmostIntData = it->second.substr(0, std::max(offset, (int64_t)0));
        while(it2->first <= last_index && it2 != intervals.end()){
            if(it2->first + it2->second.size() > maxidx){
                maxidx = it2->first + it2->second.size();
                rightmostIntDat = it2->second.erase(0, last_index - it2->first);
            }
            it2++;
        }
        it = intervals.erase(it, it2);
        data = leftmostIntData + data + rightmostIntDat;
        intervals.insert(it, {leftmostIntFirst, data});
    }else{
        if(first_index <= windowEnd)
            intervals.emplace_back(first_index, data);
    }
    if(!intervals.empty() && intervals.begin()->first == windowBegin){
        data = intervals.begin()->second;
        output_.writer().push(data);
        intervals.pop_front();
        if(output_.writer().bytes_pushed() >= eol){
            output_.writer().close();
        }
    }
}

uint64_t Reassembler::bytes_pending() const
{
    uint64_t total = 0;
    for(const auto& it : intervals){
        total += it.second.size();
    }
    return total;
}

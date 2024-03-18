//
// Created by OMEN on 2024/2/12.
//

#include <list>
#include <mutex>

#ifndef FASTERTRANSFORMER_LIST_H
#define FASTERTRANSFORMER_LIST_H

template <typename T>
class List{
private:
    std::list<T> myList;
    std::mutex mu;
public:
    void push_front(const T& x){
        std::lock_guard<std::mutex> guard(this->mu);
        myList.push_front(x);
    }
    void push_back(const T& x){
        std::lock_guard<std::mutex> guard(this->mu);
        myList.push_back(x);
    }
    void pop_front(const T& x){
        std::lock_guard<std::mutex> guard(this->mu);
        if(!myList.empty()){
            myList.pop_front();
        }
    }
    void pop_back(const T& x){
        std::lock_guard<std::mutex> guard(this->mu);
        if(!myList.empty()){
            myList.pop_back();
        }
    }
    [[nodiscard]] size_t size(){
        std::lock_guard<std::mutex> guard(this->mu);
        return myList.size();
    }
    typename std::list<T>::iterator begin(){
        std::lock_guard<std::mutex> guard(this->mu);
        return myList.begin();
    }
    typename std::list<T>::iterator end(){
        std::lock_guard<std::mutex> guard(this->mu);
        return myList.end();
    }
    void erase(typename std::list<T>::iterator& it){
        std::lock_guard<std::mutex> guard(this->mu);
        it = myList.erase(it);
    }
};

#endif //FASTERTRANSFORMER_LIST_H

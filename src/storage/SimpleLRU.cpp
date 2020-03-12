#include "SimpleLRU.h"
#include <iostream>

namespace Afina {
namespace Backend {

bool SimpleLRU::CleanUp(std::size_t size) {
    if (size > _max_size) {
        return false;
    }

    while (_lru_tail && _cur_size + size > _max_size) {
        Delete(_lru_tail->key);
    }

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    auto lookup = _lru_index.find(key);
    if (lookup != _lru_index.end()) {
        return Set(key, value);
    } else {
        return PutIfAbsent(key, value);
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto lookup = _lru_index.find(key);
    if (lookup != _lru_index.end()) {
        return false;
    }

    if (!CleanUp(key.size() + value.size())) {
        return false;
    }

    _cur_size += key.size() + value.size();

    std::unique_ptr<lru_node> new_node(new lru_node(key));
    new_node->value = value;
    new_node->next.swap(_lru_head);
    if (new_node->next) {
        new_node->next->prev = new_node.get();
    }
    new_node.swap(_lru_head);

    if (!_lru_tail) {
        _lru_tail = _lru_head.get();
    }

    _lru_index.insert({_lru_head->key, *_lru_head.get()});

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto lookup = _lru_index.find(key);
    if (lookup == _lru_index.end()) {
        return false;
    }

    lru_node &found = lookup->second;
    if (value.size() > found.value.size() && !CleanUp(value.size())) {
        return false;
    }

    _cur_size += value.size() - found.value.size();

    found.value = value;
    if (found.prev) {
        if (found.next) {
            found.next->prev = found.prev;
        } else {
            _lru_tail = found.prev;
        }
        std::unique_ptr<lru_node> buf_ptr = std::move(found.next);
        found.prev->next.swap(buf_ptr);
        _lru_head.swap(buf_ptr);
        found.next.swap(buf_ptr);
        _lru_head->next->prev = _lru_head.get();
        _lru_head->prev = nullptr;
    }

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto lookup = _lru_index.find(key);
    if (lookup == _lru_index.end()) {
        return false;
    }
    lru_node &found = lookup->second;
    _lru_index.erase(lookup);
    std::size_t size = found.key.size() + found.value.size();

    if (key == _lru_head->key) {
        _lru_head.reset(_lru_head->next.release());
        _lru_head->prev = nullptr;
        _lru_tail = nullptr;
    } else {
        if (found.next) {
            found.next->prev = found.prev;
        } else {
            _lru_tail = found.prev;
        }
        std::unique_ptr<lru_node> buf_ptr = std::move(found.next);
        found.prev->next.swap(buf_ptr);
    }

    _cur_size -= size;

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto lookup = _lru_index.find(key);
    if (lookup == _lru_index.end()) {
        return false;
    }

    lru_node &found = lookup->second;
    value = found.value;

    return true;
}

} // namespace Backend
} // namespace Afina

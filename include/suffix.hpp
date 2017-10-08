#ifndef SUFFIX_H_
#define SUFFIX_H_

#include "bitvector.hpp"

#include <assert.h>

#include <vector>

#include "config.hpp"
#include "hash.hpp"

namespace surf {

// Max suffix_len_ = 64 bits
// For kReal suffixes, if the stored key is not long enough to provide
// suffix_len_ suffix bits, its suffix field is cleared (i.e., all 0's)
// to indicate that there is no suffix info associated with the key.
class BitvectorSuffix : public Bitvector {
public:
    BitvectorSuffix() : type_(kNone), suffix_len_(0) {};

    BitvectorSuffix(const SuffixType type, const level_t suffix_len,
		 const std::vector<std::vector<word_t> >& bitvector_per_level, 
		 const std::vector<position_t>& num_bits_per_level,
		 const level_t start_level = 0, 
		 level_t end_level = 0/* non-inclusive */)
	: Bitvector(bitvector_per_level, num_bits_per_level, start_level, end_level) {
	assert(suffix_len <= kWordSize);
	type_ = type;
	suffix_len_ = suffix_len;
    }

private:
    static inline word_t constructHashSuffix(const std::string& key, const level_t len) {
	word_t suffix = suffixHash(key);
	clearMSBits(suffix, len);
	return suffix;
    }

    static inline word_t constructRealSuffix(const std::string& key, const level_t level, const level_t len) {
	word_t suffix = 0;
	level_t key_len = (level_t)key.length();
	level_t num_complete_bytes = len / 8;
	if (num_complete_bytes > 0) {
	    if (level < key_len)
		suffix += (word_t)key[level];
	    for (position_t i = 1; i < num_complete_bytes; i++) {
		suffix <<= 8;
		if ((level + i) < key_len)
		    suffix += (word_t)key[level + i];
	    }
	}
	level_t offset = len % 8;
	if (offset > 0) {
	    suffix << offset;
	    word_t remaining_bits = 0;
	    if ((level + num_complete_bytes) < key_len)
		remaining_bits = (word_t)key[level + num_complete_bytes];
	    remaining_bits >>= (8 - offset);
	    suffix += remaining_bits;
	}
	return suffix;
    }

public:
    static word_t constructSuffix(const std::string& key, const level_t level, 
				  const SuffixType type, const level_t len) {
	switch (type) {
	case kHash:
	    return constructHashSuffix(key, len);
	case kReal:
	    return constructRealSuffix(key, level, len);
	default:
	    return 0;
    }
    }

    SuffixType getType() const {
	return type_;
    }

    level_t getSuffixLen() const {
	return suffix_len_;
    }

    position_t size() const {
        position_t bitvector_mem = (num_bits_ / kWordSize) * (kWordSize / 8);
	if (num_bits_ % kWordSize == 0)
	    bitvector_mem += (kWordSize / 8);
	return (sizeof(BitvectorSuffix) + bitvector_mem);
    }

    word_t read(const position_t idx) const;
    bool checkEquality(const position_t idx, const std::string& key, const level_t level) const;

    // Compare stored suffix to querying suffix.
    // kReal suffix type only.
    int compare(const position_t idx, const std::string& key, const level_t level) const;

private:
    SuffixType type_;
    level_t suffix_len_; // in bits
};

word_t BitvectorSuffix::read(const position_t idx) const {
    assert(idx * suffix_len_ < num_bits_);
    position_t bit_pos = idx * suffix_len_;
    position_t word_id = bit_pos / kWordSize;
    position_t offset = bit_pos & (kWordSize - 1);
    return (bits_[word_id] << offset) >> (kWordSize - suffix_len_);
}

bool BitvectorSuffix::checkEquality(const position_t idx, const std::string& key, const level_t level) const {
    if (type_ == kNone) return true;
    assert(idx * suffix_len_ < num_bits_);
    word_t stored_suffix = read(idx);
    // if no suffix info for the stored key
    if (type_ == kReal && stored_suffix == 0) return true;
    // if the querying key is shorter than the stored key
    if (type_ == kReal && ((key.length() - level) * 8) < suffix_len_) return false;
    word_t querying_suffix = constructSuffix(key, level, type_, suffix_len_);
    if (stored_suffix == querying_suffix) return true;
    return false;
}

int BitvectorSuffix::compare(const position_t idx, const std::string& key, const level_t level) const {
    assert(type_ == kReal);
    assert(idx * suffix_len_ < num_bits_);
    word_t stored_suffix = read(idx);
    if (stored_suffix == 0) return -1;
    word_t querying_suffix = constructRealSuffix(key, level, suffix_len_);
    if (stored_suffix < querying_suffix) return -1;
    else if (stored_suffix == querying_suffix) return 0;
    else return 1;
}

} // namespace surf

#endif // SUFFIXVECTOR_H_
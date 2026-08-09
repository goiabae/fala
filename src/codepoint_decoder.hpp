// Header-only library for decoding UTF-8 codepoints iteratively
#ifndef CODEPOINT_DECODER_HPP
#define CODEPOINT_DECODER_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>

#include "codepoint.hpp"

class CodepointDecoder {
	enum class State {
		WAITING_PREFIX,
		THREE_MORE,
		TWO_MORE,
		ONE_MORE,
	};

 public:
	std::optional<Codepoint> push(std::uint8_t byte);

 private:
	std::uint32_t concat_flush_bytes();
	void push_byte(std::uint8_t byte);

 private:
	std::size_t m_next_index {0};
	std::size_t m_expecting {0};
	State m_state {CodepointDecoder::State::WAITING_PREFIX};
	std::array<std::uint8_t, 4> m_accumulator {0, 0, 0, 0};
};

inline void CodepointDecoder::push_byte(std::uint8_t byte) {
	if (m_expecting == 1) {
		assert(m_next_index == 0);
		m_accumulator[3] = byte;
	} else if (m_expecting == 2) {
		if (m_next_index == 0) {
			m_accumulator[2] = byte;
		} else if (m_next_index == 1) {
			m_accumulator[3] = byte;
		}
		m_next_index++;
	} else if (m_expecting == 3) {
		if (m_next_index == 0) {
			m_accumulator[1] = byte;
		} else if (m_next_index == 1) {
			m_accumulator[2] = byte;
		} else if (m_next_index == 2) {
			m_accumulator[3] = byte;
		}
		m_next_index++;
	} else if (m_expecting == 4) {
		if (m_next_index == 0) {
			m_accumulator[0] = byte;
		} else if (m_next_index == 1) {
			m_accumulator[1] = byte;
		} else if (m_next_index == 2) {
			m_accumulator[2] = byte;
		} else if (m_next_index == 2) {
			m_accumulator[3] = byte;
		}
		m_next_index++;
	} else {
		assert(false);
	}
}

inline std::optional<Codepoint> CodepointDecoder::push(std::uint8_t byte) {
	if (m_state == State::WAITING_PREFIX) {
		if ((byte & 0b10000000) == 0b00000000) {
			m_state = State::WAITING_PREFIX;
			m_expecting = 1;
			push_byte(byte);
			return concat_flush_bytes();
		} else if ((byte & 0b11100000) == 0b11000000) {
			m_state = State::ONE_MORE;
			m_expecting = 2;
			push_byte(byte);
			return std::nullopt;
		} else if ((byte & 0b11110000) == 0b11100000) {
			m_state = State::TWO_MORE;
			m_expecting = 3;
			push_byte(byte);
			return std::nullopt;
		} else if ((byte & 0b11111000) == 0b11110000) {
			m_state = State::THREE_MORE;
			m_expecting = 4;
			push_byte(byte);
			return std::nullopt;
		} else {
			throw std::runtime_error("invalid codepoint");
		}
	} else if (m_state == State::ONE_MORE) {
		if ((byte & 0b11000000) == 0b10000000) {
			push_byte(byte);
			m_state = State::WAITING_PREFIX;
			return concat_flush_bytes();
		} else {
			throw std::runtime_error("invalid codepoint");
		}
	} else if (m_state == CodepointDecoder::State::TWO_MORE) {
		if ((byte & 0b11000000) == 0b10000000) {
			push_byte(byte);
			m_state = State::ONE_MORE;
			return std::nullopt;
		} else {
			throw std::runtime_error("invalid codepoint");
		}
	} else if (m_state == CodepointDecoder::State::THREE_MORE) {
		if ((byte & 0b11000000) == 0b10000000) {
			push_byte(byte);
			m_state = State::TWO_MORE;
			return std::nullopt;
		} else {
			throw std::runtime_error("invalid codepoint");
		}
	} else {
		assert(false);
	}
}

inline std::uint32_t CodepointDecoder::concat_flush_bytes() {
	std::uint32_t result = ((std::uint32_t)m_accumulator[0] << (8 * 3))
	                     | ((std::uint32_t)m_accumulator[1] << (8 * 2))
	                     | ((std::uint32_t)m_accumulator[2] << (8 * 1))
	                     | ((std::uint32_t)m_accumulator[3] << (8 * 0));
	std::ranges::fill(m_accumulator, 0);
	m_next_index = 0;
	m_expecting = 0;
	return result;
}

#endif

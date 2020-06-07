#pragma once

#include <vector>

#include "XyzMessage.hpp"

namespace XyzMessageDecoder {
	class XyzMessageDecoder {
		public:

		std::vector<std::byte> message;

		explicit XyzMessageDecoder (std::vector<std::byte> t_message) : message(std::move(t_message)) {}

		std::vector<XyzMessage> decode () {
			std::vector<XyzMessage> messages;
			for (size_t i = 0; i < message.size();) {
				std::array<std::byte, 4> length_bytes = {message.at(i), message.at(i + 1), message.at(i + 2), message.at(i + 3)};
				uint32_t length = XyzUtils::detail::asU32(length_bytes.at(3), length_bytes.at(2), length_bytes.at(1), length_bytes.at(0));
				i += 4;

				int type = static_cast<int>(message.at(i));
				i++;

				std::vector<std::byte> messageBytes;
				messageBytes.reserve(length);
				std::copy(message.begin() + i, message.begin() + i + length, std::back_inserter(messageBytes));
				i += length;

				XyzMessage xyz_message(std::move(messageBytes), type);

				messages.push_back(xyz_message);
			}

			return messages;
		}
	};
}
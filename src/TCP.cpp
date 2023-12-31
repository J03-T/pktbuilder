#include <cstddef>
#include <cstdint>
#include <pktbuilder/TCP.h>
#include <pktbuilder/checksum.h>
#include <pktbuilder/utils.h>
#include <cmath>
#include <unordered_map>
#include <stdexcept>
#include <random>

namespace pktbuilder {
    namespace TCP {
        Packet::Packet(uint16_t destination_port, uint16_t source_port,
                    uint8_t flags, uint16_t window_size, 
                    uint32_t sequence_number, 
                    std::vector<Option> const& options,
                    uint32_t ack_number, uint16_t urgent_pointer) {
            this->source_port = source_port;
            this->destination_port = destination_port;
            if (sequence_number > 0) {
                this->sequence_number = sequence_number;
            } else {
                std::mt19937 mt(std::random_device{}());
                std::uniform_int_distribution<uint32_t> seq_n_range(0);
                this->sequence_number = seq_n_range(mt);
            }
            this->ack_number = ack_number;
            this->flags = flags;
            this->window_size = window_size;
            this->options = options;
            this->urgent_pointer = urgent_pointer;
            this->source_address = { 0, 0, 0, 0 };
            this->destination_address = { 0, 0, 0, 0 };
        }

        Packet::Packet(uint16_t destination_port, ipv4_addr_t destination_address, 
                    uint16_t source_port, uint8_t flags, ipv4_addr_t source_address,
                    uint16_t window_size, uint32_t sequence_number, 
                    std::vector<Option> const& options,
                    uint32_t ack_number, uint16_t urgent_pointer) {

            if (source_address == ipv4_addr_t({0, 0, 0, 0})) {
                this->source_address = getDefaultInterfaceIPv4Address();
            } else {
                this->source_address = source_address;
            }
            this->source_port = source_port;
            this->destination_address = destination_address;
            this->destination_port = destination_port;
            if (sequence_number > 0) {
                this->sequence_number = sequence_number;
            } else {
                std::mt19937 mt(std::random_device{}());
                std::uniform_int_distribution<uint32_t> seq_n_range(0);
                this->sequence_number = seq_n_range(mt);
            }
            this->ack_number = ack_number;
            this->flags = flags;
            this->window_size = window_size;
            this->options = options;
            this->urgent_pointer = urgent_pointer;
        }

        IPv4::Packet Packet::operator| (const IPv4::Packet& other) {
            this->source_address = other.getSourceAddress();
            this->destination_address = other.getDestinationAddress();
            IPv4::Packet new_packet = other;
            if (other.getProtocolNumber()) {
                new_packet.setProtocolNumber(other.getProtocolNumber());
            } else {
                new_packet.setProtocolNumber(IPv4::ProtocolNumber::TCP);
            }
            new_packet.setPayload(this->build());
            return new_packet;
        }

        std::vector<uint8_t> Packet::build() const {
            std::vector<uint8_t> data;
            std::array<uint8_t, 2> src_port_bytes = splitBytesBigEndian(this->source_port);
            data.insert(data.end(), src_port_bytes.begin(), src_port_bytes.end());
            std::array<uint8_t, 2> dest_port_bytes = splitBytesBigEndian(this->destination_port);
            data.insert(data.end(), dest_port_bytes.begin(), dest_port_bytes.end());
            std::array<uint8_t, 4> seq_num_bytes = splitBytesBigEndian(this->sequence_number);
            data.insert(data.end(), seq_num_bytes.begin(), seq_num_bytes.end());
            std::array<uint8_t, 4> ack_num_bytes = splitBytesBigEndian(this->ack_number);
            data.insert(data.end(), ack_num_bytes.begin(), ack_num_bytes.end());
            uint8_t header_size = 20;
            for (Option const &option: this->options) {
                header_size += 2 + option.option_data.size(); // nop + type + data
            }
            uint8_t padding_bytes = 0;
            if (header_size % 4 != 0) {
                padding_bytes = static_cast<uint8_t>(std::ceil(header_size / 4.) * 4 - header_size);
            }
            header_size += padding_bytes;
            const uint8_t data_offset = header_size / 4;
            if (data_offset != (data_offset & 0xffff)) {
                throw(std::runtime_error("TCP Header too long"));
            }
            data.push_back(data_offset << 4u);
            data.push_back(this->flags);
            std::array<uint8_t, 2> window_size_bytes = splitBytesBigEndian(this->window_size);
            data.insert(data.end(), window_size_bytes.begin(), window_size_bytes.end());
            data.push_back(0);
            data.push_back(0);
            std::array<uint8_t, 2> urg_ptr_bytes = splitBytesBigEndian(this->urgent_pointer);
            data.insert(data.end(), urg_ptr_bytes.begin(), urg_ptr_bytes.end());

            for (Option const &option : this->options) {
                data.push_back(0x01);
                data.push_back(option.option_kind);
                data.insert(data.end(), option.option_data.begin(), option.option_data.end());
            }

            for (auto i = 0; i < padding_bytes; i++) {
                data.push_back(0);
            }
            data.insert(data.end(), this->payload.begin(), this->payload.end());
            std::vector<uint8_t> pseudo_header;
            pseudo_header.reserve(12);
            pseudo_header.insert(pseudo_header.end(), this->source_address.begin(), this->source_address.end());
            pseudo_header.insert(pseudo_header.end(), this->destination_address.begin(), this->destination_address.end());
            pseudo_header.push_back(0);
            pseudo_header.push_back(IPv4::ProtocolNumber::TCP);
            if (data.size() > UINT16_MAX) {
                throw(std::runtime_error("TCP Packet too large for pseudoheader"));
            }

            const auto tcp_length = static_cast<uint16_t>(data.size());
            std::array<uint8_t, 2> length_bytes = splitBytesBigEndian(tcp_length);
            pseudo_header.insert(pseudo_header.end(), length_bytes.begin(), length_bytes.end());
            std::vector<uint8_t> to_checksum;
            to_checksum.reserve(data.size() + pseudo_header.size());
            to_checksum.insert(to_checksum.end(), pseudo_header.begin(), pseudo_header.end());
            to_checksum.insert(to_checksum.end(), data.begin(), data.end());
            const uint16_t checksum = calculateInternetChecksum(to_checksum);
            const std::array<uint8_t, 2> checksum_bytes = splitBytesBigEndian(checksum);
            data.at(16) = checksum_bytes[0];
            data.at(17) = checksum_bytes[1];
            return data;
        }

		Packet Packet::decodeFrom(const uint8_t* data, size_t length) {
			if (length < 20) {
				throw std::invalid_argument("TCP header too short");
			}
			Packet pkt;
			pkt.source_port = combineBytesBigEndian(data[0], data[1]);
			pkt.destination_port = combineBytesBigEndian(data[2], data[3]);
			pkt.sequence_number = combineBytesBigEndian(data[4], data[5], data[6], data[7]);
			pkt.ack_number = combineBytesBigEndian(data[8], data[9], data[10], data[11]);
			uint8_t data_offset = data[12] >> 4u;
			uint8_t header_length = data_offset * 4;
			if (header_length > length || header_length < 20) {
				throw std::invalid_argument("TCP data offset invalid");
			}
			pkt.flags = data[13];
			pkt.window_size = combineBytesBigEndian(data[14], data[15]);
			// ignore checksum for now bc idk the best way to pass the pseudoheader data
			pkt.urgent_pointer = combineBytesBigEndian(data[18], data[19]);
			uint8_t option_start = 20;
			while (option_start < header_length) {
				uint8_t option_kind = data[option_start];
				if (option_kind == OptionCode::EOL) {
					break;
				} else if (option_kind == OptionCode::NOP) {
					option_start++;
				} else {
					if (option_start == header_length - 1) {
						throw std::invalid_argument("TCP option length required");
					}
					uint8_t option_length = data[option_start + 1];
					if (option_start + option_length > header_length) {
						throw std::invalid_argument("TCP option length too large");
					}
					Option option;
					option.option_kind = option_kind;
					option.option_data.insert(
						option.option_data.end(),
						data + option_start + 2, 
						data + option_start + option_length
					);
					pkt.options.push_back(option);
					option_start += option_length;
				}
			}
			if (length > header_length) {
				pkt.payload.insert(pkt.payload.end(), data + header_length, data + length);
			}
			return pkt;
		}
		
		Packet Packet::decodeFrom(std::vector<uint8_t> const& data) {
			return Packet::decodeFrom(data.data(), data.size());
		}

		uint16_t Packet::getSourcePort() {
			return this->source_port;
		}

		uint16_t Packet::getDestinationPort() {
			return this->destination_port;
		}

		uint32_t Packet::getSequenceNumber() {
			return this->sequence_number;
		}

		uint32_t Packet::getAckNumber() {
			return this->ack_number;
		}

		uint8_t Packet::getFlags() {
			return this->flags;
		}

		uint16_t Packet::getWindowSize() {
			return this->window_size;
		}

		uint16_t Packet::getUrgentPointer() {
			return this->urgent_pointer;
		}

		std::vector<Option> const& Packet::getOptions() {
			return this->options;
		}
	}
}

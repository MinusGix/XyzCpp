#pragma once

#define BOOST_ASIO_HAS_MOVE

#include <vector>
#include <functional>

#include <boost/asio.hpp>

#include "XyzUtils.hpp"
#include "XyzMessage.hpp"
#include "XyzMessageBuilder.hpp"

#include <iostream>

namespace XyzCpp {
	class TcpClient {
		protected:

		// Has to be made shared so it can be 'moved'..
		std::shared_ptr<boost::asio::io_context> io_context;
		boost::asio::ip::tcp::resolver resolver;
		boost::asio::ip::tcp::socket socket;
		// we have to create this work instance so that the io_context.run() does not exit
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work;

		public:
		std::thread io_thread;

		// unsure if I really need resolver or work when we're being supplied the socket
		/// This constructor is made primarily for servers that are receiving a connection
		explicit TcpClient (boost::asio::ip::tcp::socket&& t_socket) : io_context(std::make_shared<boost::asio::io_context>()), resolver(*io_context), socket(std::move(t_socket)), work(boost::asio::make_work_guard(*io_context)) {}
		explicit TcpClient () : io_context(std::make_shared<boost::asio::io_context>()), resolver(*io_context), socket(*io_context), work(boost::asio::make_work_guard(*io_context)) {}

		/// Connect to the [host] on [port]
		/// Can throw a boost::system::system_error
		void connect (std::string host, int port) {
			// Note: in C#-version TCPClient only allows port to be between 0 and 65535

			boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
			// non-async connection, see: original
			boost::asio::connect(socket, endpoints);

			io_thread = std::thread([this] () {
				this->io_context->run();
			});
		}

		bool isConnected () {
			return socket.is_open();
		}

		void close () {
			socket.close();
		}

		template<typename B, typename F>
		void readAsync (B buffer, F callback) {
			boost::asio::async_read(socket, buffer, callback);
		}

		template<typename B, typename F>
		void writeAsync (B buffer, F callback) {
			boost::asio::async_write(socket, buffer, callback);
		}

		template<typename B>
		void writeSync (B buffer) {
			boost::asio::write(socket, buffer);
		}
	};

	class XyzClient {
		public:
		static constexpr size_t length_size = 4;
		static constexpr size_t type_size = 1;

		enum class State {
			Length, // -> Type
			Type, // -> Message
			Message
		};

		protected:
		TcpClient client;

		size_t length = 0;
		int type = 0;
		std::vector<std::byte> message;

		public:

		std::function<void(void)> onConnect;
		std::function<void(void)> onDisconnect;
		std::function<void(std::vector<std::byte>, int)> onMessage;

		State state = State::Length;
		std::vector<std::byte> temp_buffer;

		explicit XyzClient () {}
		/// Note: this could cause issues, since it will fail/succeed to connect before you can apply the callbacks.
		explicit XyzClient (std::string host, int port) {
			assureBufferSize(64);
			connect(host, port);
		}
		explicit XyzClient (TcpClient&& t_client) : client(std::move(t_client)) {}

		/// Read a message from server
		/// Note: this is ran automatically (repeatedly) on-connect, so you likely don't need to call this
		void read () {
			readLength();
		}

		void disconnect (bool forced=false) {
			if (forced || client.isConnected()) {
				
				client.close();
				invokeOnDisconnect();
			}
		}

		bool connect (std::string host, int port) {
			std::cout << "Connecting\n";
			try {
				client.connect(host, port);

				invokeOnConnect();

				read();
				return true;
			} catch (std::exception& err) { // TODO: this is bad, since we don't properly showcase the error, but it's what the original does
				invokeOnDisconnect();
			}
			return false;
		}

		bool connected () {
			// TODO: check for differences between this and original
			return client.isConnected();
		}

		void send (const std::vector<std::byte>& data, int type=0) {
			send(data.data(), data.size(), type);
		}

		void send (const std::string& data, int type=0) {
			send(reinterpret_cast<const std::byte*>(data.data()), data.size(), type);
		}

		void send (const std::vector<XyzMessage>& messages, int type=0) {
			XyzMessageBuilder builder;
			for (const XyzMessage& message : messages) {
				builder.add(message);
			}
			std::vector<std::byte> data = builder.toVector();
			send(data, type);
		}

		void send (const XyzMessage& message, int type=0) {
			XyzMessageBuilder builder;
			std::vector<std::byte> payload = builder.add(message).toVector();
			send(payload, type);
		}

		void sendSync (std::vector<std::byte>& data, int type=0) {
			sendSync(data.data(), data.size());
		}

		void sendSync (const std::string& data, int type=0) {
			sendSync(reinterpret_cast<const std::byte*>(data.data()), data.size(), type);
		}

		void sendSync (const std::vector<XyzMessage>& messages, int type=0) {
			XyzMessageBuilder builder;
			for (const XyzMessage& message : messages) {
				builder.add(message);
			}
			std::vector<std::byte> data = builder.toVector();
			sendSync(data, type);
		}

		void sendSync (const XyzMessage& message, int type=0) {
			XyzMessageBuilder builder;
			std::vector<std::byte> payload = builder.add(message).toVector();
			sendSync(payload, type);
		}

		private:

		void send (const std::byte* data, size_t length, int type=0) {
			try {
				std::vector<std::byte> payload = XyzMessageBuilder()
					.add(XyzUtils::deflate(data, length), type)
					.toVector();

				client.writeAsync(boost::asio::buffer(payload), [this] (boost::system::error_code ec, size_t length) {
					if (ec) {
						this->resetState();
						this->invokeOnDisconnect();
					}
				});
			} catch (...) {} // TODO: handle error?
		}

		void sendSync (const std::byte* data, size_t length, int type=0) {
			try {
				std::vector<std::byte> payload = XyzMessageBuilder()
					.add(XyzUtils::deflate(data, length))
					.toVector();

				client.writeSync(boost::asio::buffer(payload));
			} catch (...) {}
		}

		void readLength () {
			state = State::Length;
			assureBufferSize(length_size);
			client.readAsync(boost::asio::buffer(temp_buffer.data(), length_size), [this] (boost::system::error_code ec, size_t length) {
				this->receivedLength(ec, length);
			});
		}

		void receivedLength (boost::system::error_code ec, size_t) {
			if (ec) {
				handleReceiveError();
				return;
			}

			try {
				// todo: throw error if there's not enough space in message (check what BitConverter.ToInt32 would throw)
				length = XyzUtils::detail::asU32(temp_buffer.at(3), temp_buffer.at(2), temp_buffer.at(1), temp_buffer.at(0));

				state = State::Type;
				assureBufferSize(type_size);
				client.readAsync(boost::asio::buffer(temp_buffer.data(), type_size), [this] (boost::system::error_code ec, size_t length) {
					this->receivedType(ec, length);
				});
			} catch (...) { // TODO: handle error properly?
				handleReceiveError();
			}
		}

		void receivedType (boost::system::error_code ec, size_t) {
			if (ec) {
				handleReceiveError();
				return;
			}

			try {
				// TODO: throw error if it can't acccess
				type = static_cast<int>(temp_buffer.at(0));

				state = State::Message;
				assureBufferSize(length);
				client.readAsync(boost::asio::buffer(temp_buffer.data(), length), [this] (boost::system::error_code ec, size_t length) {
					this->receivedMessage(ec, length);
				});
			} catch (...) { // TODO: handle error properly?
				handleReceiveError();
			}
		}

		void receivedMessage (boost::system::error_code ec, size_t) {
			if (ec) {
				handleReceiveError();
				return;
			}

			try {
				message = XyzUtils::inflate(temp_buffer.data(), temp_buffer.size());
				invokeOnMessage(std::move(message), type);

				readLength();
			} catch (...) { // TODO: handle error properly?
				handleReceiveError();
			}
		}

		void handleReceiveError () {
			resetState();
			invokeOnDisconnect();
		}

		/// Makes sure the buffer has enough space to hold [length]
		void assureBufferSize (size_t length) {
			temp_buffer.resize(length);
		}

		void resetState () {
			state = State::Length;
		}

		void invokeOnConnect () {
			if (onConnect) {
				onConnect();
			}
		}

		void invokeOnDisconnect () {
			if (onDisconnect) {
				onDisconnect();
			}
		}

		void invokeOnMessage (std::vector<std::byte> data, int type) {
			if (onMessage) {
				onMessage(std::move(data), type);
			}
		}
	};
}
//================================================================================================
/// @file can_transport_protocol.cpp
///
/// @brief A protocol that handles the ISO11783/J1939 transport protocol.
/// It handles both the broadcast version (BAM) and and the connection mode version.
/// @author Adrian Del Grosso
///
/// @copyright 2022 Adrian Del Grosso
//================================================================================================

#include "isobus/isobus/can_transport_protocol.hpp"

#include "isobus/isobus/can_general_parameter_group_numbers.hpp"
#include "isobus/isobus/can_network_configuration.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/utility/system_timing.hpp"
#include "isobus/utility/to_string.hpp"

#include <algorithm>
#include <memory>

namespace isobus
{
	TransportProtocolManager::TransportProtocolSession::TransportProtocolSession(Direction sessionDirection, CANTransportMessage &&message) :

	  sessionMessage(std::move(message)),
	  sessionDirection(sessionDirection)
	{
	}

	bool TransportProtocolManager::TransportProtocolSession::operator==(const TransportProtocolSession &obj) const
	{
		return ((sessionMessage.get_source().lock() == obj.sessionMessage.get_source().lock()) &&
		        (sessionMessage.get_destination().lock() == obj.sessionMessage.get_destination().lock()) &&
		        (sessionMessage.get_pgn() == obj.sessionMessage.get_pgn()));
	}

	std::uint32_t TransportProtocolManager::TransportProtocolSession::get_message_data_length() const
	{
		return static_cast<std::uint32_t>(sessionMessage.get_data().size());
	}

	void TransportProtocolManager::TransportProtocolSession::set_state(StateMachineState value)
	{
		state = value;
		timestamp_ms = SystemTiming::get_timestamp_ms();
	}

	TransportProtocolManager::TransportProtocolSession::TransportProtocolSession(TransportProtocolSession &&other) noexcept :
	  state(other.state),
	  sessionMessage(std::move(other.sessionMessage)),
	  timestamp_ms(other.timestamp_ms),
	  lastPacketNumber(other.lastPacketNumber),
	  packetCount(other.packetCount),
	  processedPacketsThisSession(other.processedPacketsThisSession),
	  clearToSendPacketMax(other.clearToSendPacketMax),
	  sessionCompleteCallback(std::move(other.sessionCompleteCallback)),
	  parent(other.parent),
	  sessionDirection(other.sessionDirection)
	{
	}

	TransportProtocolManager::TransportProtocolSession &TransportProtocolManager::TransportProtocolSession::operator=(TransportProtocolSession &&other) noexcept
	{
		sessionMessage = std::move(other.sessionMessage);
		sessionDirection = other.sessionDirection;
		state = other.state;
		packetCount = other.packetCount;
		lastPacketNumber = other.lastPacketNumber;
		processedPacketsThisSession = other.processedPacketsThisSession;
		clearToSendPacketMax = other.clearToSendPacketMax;
		timestamp_ms = other.timestamp_ms;
		sessionCompleteCallback = other.sessionCompleteCallback;
		parent = other.parent;
		return *this;
	}

	TransportProtocolManager::TransportProtocolManager(CANLibBadge<CANNetworkManager>)
	{
	}

	bool TransportProtocolManager::initialize(CANLibBadge<CANNetworkManager>)
	{
		if (!initialized)
		{
			initialized = true;
			CANNetworkManager::CANNetwork.add_protocol_parameter_group_number_callback(static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolConnectionManagement), process_message, this);
			CANNetworkManager::CANNetwork.add_protocol_parameter_group_number_callback(static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolDataTransfer), process_message, this);
		}
		return initialized;
	}

	void TransportProtocolManager::process_broadcast_announce_message(const std::shared_ptr<ControlFunction> source,
	                                                                  std::uint32_t pgn,
	                                                                  std::uint16_t totalMessageSize,
	                                                                  std::uint8_t totalNumberOfPackets)
	{
		// The standard defines that we may not send aborts for messages with a global destination, we can only ignore them if we need to
		if (activeSessions.size() >= CANNetworkManager::CANNetwork.get_configuration().get_max_number_transport_protocol_sessions())
		{
			// TODO: consider using maximum memory instead of maximum number of sessions
			CANStackLogger::warn("[TP]: Ignoring Broadcast Announcement Message (BAM) for %#06X, configured maximum number of sessions reached.", pgn);
		}
		else
		{
			if (auto session = get_session(source, nullptr))
			{
				CANStackLogger::warn("[TP]: Received Broadcast Announcement Message (BAM) while a session already existed for this source, overwriting for %#06X...", pgn);
				close_session(*session, false);
			}

			auto data = std::make_unique<CANTransportDataVector>();
			data->reserve(totalMessageSize);

			TransportProtocolSession session(TransportProtocolSession::Direction::Receive,
			                                 { pgn,
			                                   source,
			                                   nullptr, // Global destination
			                                   std::move(data) });
			session.packetCount = totalNumberOfPackets;
			session.set_state(StateMachineState::RxDataSession);
			activeSessions.push_back(std::move(session));

			CANStackLogger::debug("[TP]: New rx broadcast message session for %#06X. Source: %hu", pgn, source->get_address());
		}
	}

	void TransportProtocolManager::process_request_to_send(const std::shared_ptr<ControlFunction> source,
	                                                       const std::shared_ptr<ControlFunction> destination,
	                                                       std::uint32_t pgn,
	                                                       std::uint16_t totalMessageSize,
	                                                       std::uint8_t totalNumberOfPackets,
	                                                       std::uint8_t clearToSendPacketMax)
	{
		if (activeSessions.size() >= CANNetworkManager::CANNetwork.get_configuration().get_max_number_transport_protocol_sessions())
		{
			// TODO: consider using maximum memory instead of maximum number of sessions
			CANStackLogger::warn("[TP]: Replying with abort to Request To Send (RTS) for %#06X, configured maximum number of sessions reached.", pgn);
			send_abort(std::static_pointer_cast<InternalControlFunction>(destination), source, pgn, ConnectionAbortReason::AlreadyInCMSession);
		}
		else
		{
			if (auto session = get_session(source, destination))
			{
				if (session->sessionMessage.get_pgn() != pgn)
				{
					CANStackLogger::error("[TP]: Received Request To Send (RTS) while a session already existed for this source and destination, aborting for %#06X...", pgn);
					abort_session(*session, ConnectionAbortReason::AlreadyInCMSession);
				}
				else
				{
					CANStackLogger::warn("[TP]: Received Request To Send (RTS) while a session already existed for this source and destination and pgn, overwriting for %#06X...", pgn);
					close_session(*session, false);
				}
			}

			auto data = std::make_unique<CANTransportDataVector>();
			data->reserve(totalMessageSize);

			TransportProtocolSession session(TransportProtocolSession::Direction::Receive,
			                                 { pgn,
			                                   source,
			                                   destination,
			                                   std::move(data) });

			session.packetCount = totalNumberOfPackets;
			session.clearToSendPacketMax = clearToSendPacketMax;
			session.set_state(StateMachineState::ClearToSend);
			activeSessions.push_back(std::move(session));
		}
	}

	void TransportProtocolManager::process_clear_to_send(const std::shared_ptr<ControlFunction> source,
	                                                     const std::shared_ptr<ControlFunction> destination,
	                                                     std::uint32_t pgn,
	                                                     std::uint8_t packetsToBeSent,
	                                                     std::uint8_t nextPacketNumber)
	{
		if (auto session = get_session(source, destination))
		{
			if (session->sessionMessage.get_pgn() != pgn)
			{
				CANStackLogger::error("[TP]: Received a Clear To Send (CTS) message for %#06X while a session already existed for this source and destination, sending abort for both...", pgn);
				abort_session(*session, ConnectionAbortReason::AnyOtherError);
				send_abort(std::static_pointer_cast<InternalControlFunction>(destination), source, pgn, ConnectionAbortReason::AnyOtherError);
			}
			else if (nextPacketNumber != (session->lastPacketNumber + 1))
			{
				CANStackLogger::error("[TP]: Received a Clear To Send (CTS) message for %#06X with a bad sequence number, aborting...", pgn);
				abort_session(*session, ConnectionAbortReason::BadSequenceNumber);
			}
			else if (StateMachineState::WaitForClearToSend != session->state)
			{
				// The session exists, but we're not in the right state to receive a CTS, so we must abort
				CANStackLogger::warn("[TP]: Received a Clear To Send (CTS) message for %#06X, but not expecting one, aborting session.", pgn);
				abort_session(*session, ConnectionAbortReason::ClearToSendReceivedWhileTransferInProgress);
			}
			else
			{
				session->packetCount = packetsToBeSent;
				session->timestamp_ms = SystemTiming::get_timestamp_ms();

				// If 0 was sent as the packet number, they want us to wait.
				// Just sit here in this state until we get a non-zero packet count
				if (0 != packetsToBeSent)
				{
					session->lastPacketNumber = 0;
					session->state = StateMachineState::TxDataSession;
				}
			}
		}
		else
		{
			// We got a CTS but no session exists. Aborting clears up the situation faster than waiting for them to timeout
			CANStackLogger::warn("[TP]: Received Clear To Send (CTS) for %#06X while no session existed for this source and destination, sending abort.", pgn);
			send_abort(std::static_pointer_cast<InternalControlFunction>(destination), source, pgn, ConnectionAbortReason::AnyOtherError);
		}
	}

	void TransportProtocolManager::process_end_of_session_acknowledgement(const std::shared_ptr<ControlFunction> source,
	                                                                      const std::shared_ptr<ControlFunction> destination,
	                                                                      std::uint32_t pgn)
	{
		if (auto session = get_session(source, destination))
		{
			if (StateMachineState::WaitForEndOfMessageAcknowledge == session->state)
			{
				// We completed our Tx session!
				session->state = StateMachineState::None;
				close_session(*session, true);
			}
			else
			{
				// The session exists, but we're not in the right state to receive an EOM, by the standard we must ignore it
				CANStackLogger::warn("[TP]: Received an End Of Message Acknowledgement message for %#06X, but not expecting one, ignoring.", pgn);
			}
		}
		else
		{
			CANStackLogger::warn("[TP]: Received End Of Message Acknowledgement for %#06X while no session existed for this source and destination, sending abort.", pgn);
			send_abort(std::static_pointer_cast<InternalControlFunction>(destination), source, pgn, ConnectionAbortReason::AnyOtherError);
		}
	}

	void TransportProtocolManager::process_abort(const std::shared_ptr<ControlFunction> source,
	                                             const std::shared_ptr<ControlFunction> destination,
	                                             std::uint32_t pgn,
	                                             TransportProtocolManager::ConnectionAbortReason reason)
	{
		bool foundSession = false;

		if (auto session = get_session(source, destination))
		{
			if (session->sessionMessage.get_pgn() == pgn)
			{
				foundSession = true;
				CANStackLogger::error("[TP]: Received an abort (reason=%hu) for an rx session for PGN %#06X", static_cast<uint8_t>(reason), pgn);
				close_session(*session, false);
			}
		}
		if (auto session = get_session(destination, source))
		{
			if (session->sessionMessage.get_pgn() == pgn)
			{
				foundSession = true;
				CANStackLogger::error("[TP]: Received an abort (reason=%hu) for a tx session for PGN %#06X", static_cast<uint8_t>(reason), pgn);
				close_session(*session, false);
			}
		}

		if (!foundSession)
		{
			CANStackLogger::warn("[TP]: Received an abort (reason=%hu) with no matching session for PGN %#06X", static_cast<uint8_t>(reason), pgn);
		}
	}

	void TransportProtocolManager::process_connection_management_message(const CANMessage &message)
	{
		const auto pgn = message.get_uint24_at(5);

		switch (message.get_uint8_at(0))
		{
			case BROADCAST_ANNOUNCE_MESSAGE_MULTIPLEXOR:
			{
				if (message.is_destination_global())
				{
					const auto totalMessageSize = message.get_uint16_at(1);
					const auto totalNumberOfPackets = message.get_uint8_at(3);
					process_broadcast_announce_message(message.get_source_control_function(),
					                                   pgn,
					                                   totalMessageSize,
					                                   totalNumberOfPackets);
				}
				else
				{
					CANStackLogger::warn("[TP]: Received a Broadcast Announcement Message (BAM) with a non-global destination, ignoring");
				}
			}
			break;

			case REQUEST_TO_SEND_MULTIPLEXOR:
			{
				if (message.is_destination_global())
				{
					CANStackLogger::warn("[TP]: Received a Request to Send (RTS) message with a global destination, ignoring");
				}
				else
				{
					const auto totalMessageSize = message.get_uint16_at(1);
					const auto totalNumberOfPackets = message.get_uint8_at(3);
					const auto clearToSendPacketMax = message.get_uint8_at(4);
					process_request_to_send(message.get_source_control_function(),
					                        message.get_destination_control_function(),
					                        pgn,
					                        totalMessageSize,
					                        totalNumberOfPackets,
					                        clearToSendPacketMax);
				}
			}
			break;

			case CLEAR_TO_SEND_MULTIPLEXOR:
			{
				if (message.is_destination_global())
				{
					CANStackLogger::warn("[TP]: Received a Clear to Send (CTS) message with a global destination, ignoring");
				}
				else
				{
					const auto packetsToBeSent = message.get_uint8_at(1);
					const auto nextPacketNumber = message.get_uint8_at(2);
					process_clear_to_send(message.get_source_control_function(),
					                      message.get_destination_control_function(),
					                      pgn,
					                      packetsToBeSent,
					                      nextPacketNumber);
				}
			}
			break;

			case END_OF_MESSAGE_ACKNOWLEDGE_MULTIPLEXOR:
			{
				if (message.is_destination_global())
				{
					CANStackLogger::warn("[TP]: Received an End of Message Acknowledge message with a global destination, ignoring");
				}
				else
				{
					process_end_of_session_acknowledgement(message.get_source_control_function(),
					                                       message.get_destination_control_function(),
					                                       pgn);
				}
			}
			break;

			case CONNECTION_ABORT_MULTIPLEXOR:
			{
				if (message.is_destination_global())
				{
					CANStackLogger::warn("[TP]: Received an Abort message with a global destination, ignoring");
				}
				else
				{
					const auto reason = static_cast<ConnectionAbortReason>(message.get_uint8_at(1));
					process_abort(message.get_source_control_function(),
					              message.get_destination_control_function(),
					              pgn,
					              reason);
				}
			}
			break;

			default:
			{
				CANStackLogger::CAN_stack_log(CANStackLogger::LoggingLevel::Warning, "[TP]: Bad Mux in Transport Protocol Connection Management message");
			}
			break;
		}
	}

	void TransportProtocolManager::process_data_transfer_message(const CANMessage &message)
	{
		auto source = message.get_source_control_function();
		auto destination = message.is_destination_global() ? nullptr : message.get_destination_control_function();

		if (auto session = get_session(source, destination))
		{
			if (StateMachineState::RxDataSession != session->state)
			{
				CANStackLogger::warn("[TP]: Received a Data Transfer message from %hu while not expecting one, sending abort", source->get_address());
				abort_session(*session, ConnectionAbortReason::UnexpectedDataTransferPacketReceived);
			}
			else if (message.get_uint8_at(SEQUENCE_NUMBER_DATA_INDEX) == session->lastPacketNumber)
			{
				CANStackLogger::error("[TP]: Aborting rx session for %#06X due to duplicate sequence number", session->sessionMessage.get_pgn());
				abort_session(*session, ConnectionAbortReason::DuplicateSequenceNumber);
			}
			else if (message.get_uint8_at(SEQUENCE_NUMBER_DATA_INDEX) == (session->lastPacketNumber + 1))
			{
				// Correct sequence number, copy the data
				for (std::uint8_t i = 0; (i < PROTOCOL_BYTES_PER_FRAME) && (static_cast<std::uint32_t>((PROTOCOL_BYTES_PER_FRAME * session->lastPacketNumber) + i) < session->get_message_data_length()); i++)
				{
					std::uint16_t currentDataIndex = (PROTOCOL_BYTES_PER_FRAME * session->lastPacketNumber) + i;
					session->sessionMessage.get_data().set_byte(currentDataIndex, message.get_uint8_at(1 + i));
				}
				session->lastPacketNumber++;
				session->processedPacketsThisSession++;
				session->timestamp_ms = SystemTiming::get_timestamp_ms();
				if ((session->lastPacketNumber * PROTOCOL_BYTES_PER_FRAME) >= session->get_message_data_length())
				{
					// Send End of Message Acknowledgement for sessions with specific destination only
					if (!message.is_destination_global())
					{
						send_end_of_session_acknowledgement(*session);
					}
					CANNetworkManager::CANNetwork.process_any_control_function_pgn_callbacks(session->sessionMessage.construct_message());
					CANNetworkManager::CANNetwork.protocol_message_callback(session->sessionMessage.construct_message());
					close_session(*session, true);
				}
			}
			else
			{
				CANStackLogger::error("[TP]: Aborting rx session for %#06X due to bad sequence number", session->sessionMessage.get_pgn());
				abort_session(*session, ConnectionAbortReason::BadSequenceNumber);
			}
		}
		else if (!message.is_destination_global())
		{
			CANStackLogger::warn("[TP]: Received a Data Transfer message from %hu with no matching session, ignoring...", source->get_address());
		}
	}

	void TransportProtocolManager::process_message(const CANMessage &message)
	{
		// TODO: Allow sniffing of messages to all addresses, not just the ones we normally listen to (#297)
		if (message.has_valid_source_control_function() && (message.has_valid_destination_control_function() || message.is_destination_global()))
		{
			switch (message.get_identifier().get_parameter_group_number())
			{
				case static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolConnectionManagement):
				{
					if (CAN_DATA_LENGTH == message.get_data_length())
					{
						process_connection_management_message(message);
					}
					else
					{
						CANStackLogger::warn("[TP]: Received a Connection Management message of invalid length %hu", message.get_data_length());
					}
				}
				break;

				case static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolDataTransfer):
				{
					if (PROTOCOL_BYTES_PER_FRAME == message.get_data_length())
					{
						process_data_transfer_message(message);
					}
					else
					{
						CANStackLogger::warn("[TP]: Received a Data Transfer message of invalid length %hu", message.get_data_length());
					}
				}
				break;

				default:
					break;
			}
		}
	}

	void TransportProtocolManager::process_message(const CANMessage &message, void *parent)
	{
		if (nullptr != parent)
		{
			reinterpret_cast<TransportProtocolManager *>(parent)->process_message(message);
		}
	}

	bool TransportProtocolManager::protocol_transmit_message(std::uint32_t parameterGroupNumber,
	                                                         std::unique_ptr<CANTransportData> data,
	                                                         std::shared_ptr<ControlFunction> source,
	                                                         std::shared_ptr<ControlFunction> destination,
	                                                         TransmitCompleteCallback sessionCompleteCallback,
	                                                         void *parentPointer)
	{
		// Return false early if we can't send the message
		if ((nullptr == data) || (data->size() <= CAN_DATA_LENGTH) || (data->size() > MAX_PROTOCOL_DATA_LENGTH))
		{
			// Invalid message length
			return false;
		}
		else if ((nullptr == source) || (!source->get_address_valid()) || has_session(source, destination))
		{
			return false;
		}
		TransportProtocolSession session(TransportProtocolSession::Direction::Transmit,
		                                 { parameterGroupNumber,
		                                   source,
		                                   destination,
		                                   std::move(data) });

		session.packetCount = static_cast<std::uint8_t>(session.get_message_data_length() / PROTOCOL_BYTES_PER_FRAME);
		if (0 != (session.get_message_data_length() % PROTOCOL_BYTES_PER_FRAME))
		{
			session.packetCount++;
		}
		session.lastPacketNumber = 0;
		session.processedPacketsThisSession = 0;
		session.sessionCompleteCallback = sessionCompleteCallback;
		session.parent = parentPointer;

		if (nullptr != destination)
		{
			// Destination specific message
			session.set_state(StateMachineState::RequestToSend);
		}
		else
		{
			// Broadcast message
			session.set_state(StateMachineState::BroadcastAnnounce);
		}
		activeSessions.push_back(std::move(session));
		return true;
	}

	void TransportProtocolManager::update(CANLibBadge<CANNetworkManager>)
	{
		for (auto &session : activeSessions)
		{
			if (!session.sessionMessage.can_continue())
			{
				CANStackLogger::warn("[TP]: Closing active session as it is unable to continue");
				abort_session(session, ConnectionAbortReason::AnyOtherError);
			}
			else
			{
				update_state_machine(session);
			}
		}
	}

	void TransportProtocolManager::send_data_transfer_packets(TransportProtocolSession &session)
	{
		std::array<std::uint8_t, CAN_DATA_LENGTH> buffer;
		std::uint32_t framesSentThisUpdate = 0;

		// Try and send packets
		const auto &data = session.sessionMessage.get_data();
		for (std::uint8_t i = session.lastPacketNumber; i < session.packetCount; i++)
		{
			buffer[0] = (session.processedPacketsThisSession + 1);

			for (std::uint8_t j = 0; j < PROTOCOL_BYTES_PER_FRAME; j++)
			{
				std::uint32_t index = (j + (PROTOCOL_BYTES_PER_FRAME * session.processedPacketsThisSession));
				if (index < session.get_message_data_length())
				{
					buffer[1 + j] = data.get_byte(index);
				}
				else
				{
					buffer[1 + j] = 0xFF;
				}
			}

			if (CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolDataTransfer),
			                                                   buffer.data(),
			                                                   buffer.size(),
			                                                   std::static_pointer_cast<InternalControlFunction>(session.sessionMessage.get_source().lock()),
			                                                   session.sessionMessage.get_destination().lock(),
			                                                   CANIdentifier::CANPriority::PriorityLowest7))
			{
				framesSentThisUpdate++;
				session.lastPacketNumber++;
				session.processedPacketsThisSession++;
				session.timestamp_ms = SystemTiming::get_timestamp_ms();

				if (session.sessionMessage.is_destination_global())
				{
					// Need to wait for the frame delay time before continuing BAM session
					break;
				}
				else if (framesSentThisUpdate >= CANNetworkManager::CANNetwork.get_configuration().get_max_number_of_network_manager_protocol_frames_per_update())
				{
					break; // Throttle the session
				}
			}
			else
			{
				// Process more next time protocol is updated
				break;
			}
		}

		if (session.lastPacketNumber == session.packetCount)
		{
			if (session.get_message_data_length() <= (PROTOCOL_BYTES_PER_FRAME * session.processedPacketsThisSession))
			{
				if (session.sessionMessage.is_destination_global())
				{
					// Broadcast tx message is complete
					close_session(session, true);
				}
				else
				{
					session.set_state(StateMachineState::WaitForEndOfMessageAcknowledge);
				}
			}
			else
			{
				session.set_state(StateMachineState::WaitForClearToSend);
			}
		}
	}

	void TransportProtocolManager::update_state_machine(TransportProtocolSession &session)
	{
		switch (session.state)
		{
			case StateMachineState::None:
				break;

			case StateMachineState::ClearToSend:
			{
				if (send_clear_to_send(session))
				{
					session.set_state(StateMachineState::RxDataSession);
				}
			}
			break;

			case StateMachineState::WaitForClearToSend:
			case StateMachineState::WaitForEndOfMessageAcknowledge:
			{
				if (SystemTiming::time_expired_ms(session.timestamp_ms, T2_T3_TIMEOUT_MS))
				{
					CANStackLogger::error("[TP]: Timeout tx session for %#06X", session.sessionMessage.get_pgn());
					abort_session(session, ConnectionAbortReason::Timeout);
				}
			}
			break;

			case StateMachineState::RequestToSend:
			{
				if (send_request_to_send(session))
				{
					session.set_state(StateMachineState::WaitForClearToSend);
				}
			}
			break;

			case StateMachineState::BroadcastAnnounce:
			{
				if (send_broadcast_announce_message(session))
				{
					session.set_state(StateMachineState::TxDataSession);
				}
			}
			break;

			case StateMachineState::TxDataSession:
			{
				if (session.sessionMessage.is_destination_global() && (!SystemTiming::time_expired_ms(session.timestamp_ms, CANNetworkManager::CANNetwork.get_configuration().get_minimum_time_between_transport_protocol_bam_frames())))
				{
					// Need to wait before sending the next data frame of the broadcast session
				}
				else
				{
					send_data_transfer_packets(session);
				}
			}
			break;

			case StateMachineState::RxDataSession:
			{
				if (session.sessionMessage.is_destination_global())
				{
					// Broadcast message timeout check
					if (SystemTiming::time_expired_ms(session.timestamp_ms, T1_TIMEOUT_MS))
					{
						CANStackLogger::warn("[TP]: Broadcast rx session timeout");
						close_session(session, false);
					}
				}
				else
				{
					// CM TP Timeout check
					if (SystemTiming::time_expired_ms(session.timestamp_ms, MESSAGE_TR_TIMEOUT_MS))
					{
						CANStackLogger::error("[TP]: Destination specific rx session timeout");
						abort_session(session, ConnectionAbortReason::Timeout);
					}
				}
			}
			break;
		}
	}

	bool TransportProtocolManager::abort_session(const TransportProtocolSession &session, ConnectionAbortReason reason)
	{
		bool retVal = false;
		std::shared_ptr<InternalControlFunction> myControlFunction;
		std::shared_ptr<ControlFunction> partnerControlFunction;
		if (TransportProtocolSession::Direction::Transmit == session.sessionDirection)
		{
			myControlFunction = CANNetworkManager::CANNetwork.get_internal_control_function(session.sessionMessage.get_source().lock());
			partnerControlFunction = session.sessionMessage.get_destination().lock();
		}
		else
		{
			myControlFunction = CANNetworkManager::CANNetwork.get_internal_control_function(session.sessionMessage.get_destination().lock());
			partnerControlFunction = session.sessionMessage.get_source().lock();
		}

		if ((nullptr != myControlFunction) && (nullptr != partnerControlFunction))
		{
			retVal = send_abort(myControlFunction, partnerControlFunction, session.sessionMessage.get_pgn(), reason);
		}
		close_session(session, false);
		return retVal;
	}

	bool TransportProtocolManager::send_abort(std::shared_ptr<InternalControlFunction> sender,
	                                          std::shared_ptr<ControlFunction> receiver,
	                                          std::uint32_t parameterGroupNumber,
	                                          ConnectionAbortReason reason) const
	{
		const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer{
			CONNECTION_ABORT_MULTIPLEXOR,
			static_cast<std::uint8_t>(reason),
			0xFF,
			0xFF,
			0xFF,
			static_cast<std::uint8_t>(parameterGroupNumber & 0xFF),
			static_cast<std::uint8_t>((parameterGroupNumber >> 8) & 0xFF),
			static_cast<std::uint8_t>((parameterGroupNumber >> 16) & 0xFF)
		};
		return CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolConnectionManagement),
		                                                      buffer.data(),
		                                                      buffer.size(),
		                                                      sender,
		                                                      receiver);
	}

	void TransportProtocolManager::close_session(const TransportProtocolSession &session, bool successful)
	{
		if ((nullptr != session.sessionCompleteCallback) && (TransportProtocolSession::Direction::Transmit == session.sessionDirection))
		{
			if (auto source = session.sessionMessage.get_source().lock())
			{
				session.sessionCompleteCallback(session.sessionMessage.get_pgn(),
				                                session.get_message_data_length(),
				                                std::static_pointer_cast<InternalControlFunction>(source),
				                                session.sessionMessage.get_destination().lock(),
				                                successful,
				                                session.parent);
			}
		}

		auto sessionLocation = std::find(activeSessions.begin(), activeSessions.end(), session);
		if (activeSessions.end() != sessionLocation)
		{
			activeSessions.erase(sessionLocation);
			CANStackLogger::debug("[TP]: Session Closed");
		}
	}

	bool TransportProtocolManager::send_broadcast_announce_message(const TransportProtocolSession &session) const
	{
		bool retVal = false;
		if (auto source = session.sessionMessage.get_source().lock())
		{
			const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer{
				BROADCAST_ANNOUNCE_MESSAGE_MULTIPLEXOR,
				static_cast<std::uint8_t>(session.get_message_data_length() & 0xFF),
				static_cast<std::uint8_t>((session.get_message_data_length() >> 8) & 0xFF),
				session.packetCount,
				0xFF,
				static_cast<std::uint8_t>(session.sessionMessage.get_pgn() & 0xFF),
				static_cast<std::uint8_t>((session.sessionMessage.get_pgn() >> 8) & 0xFF),
				static_cast<std::uint8_t>((session.sessionMessage.get_pgn() >> 16) & 0xFF)
			};
			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolConnectionManagement),
			                                                        buffer.data(),
			                                                        buffer.size(),
			                                                        std::static_pointer_cast<InternalControlFunction>(source),
			                                                        nullptr);
		}
		return retVal;
	}

	bool TransportProtocolManager::send_clear_to_send(const TransportProtocolSession &session) const
	{
		bool retVal = false;
		if (auto ourControlFunction = session.sessionMessage.get_destination().lock())
		{
			std::uint8_t packetsRemaining = (session.packetCount - session.processedPacketsThisSession);
			std::uint8_t packetsThisSegment = (session.clearToSendPacketMax < packetsRemaining) ? session.clearToSendPacketMax : packetsRemaining;

			const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer{
				CLEAR_TO_SEND_MULTIPLEXOR,
				packetsThisSegment,
				static_cast<std::uint8_t>(session.processedPacketsThisSession + 1),
				0xFF,
				0xFF,
				static_cast<std::uint8_t>(session.sessionMessage.get_pgn() & 0xFF),
				static_cast<std::uint8_t>((session.sessionMessage.get_pgn() >> 8) & 0xFF),
				static_cast<std::uint8_t>((session.sessionMessage.get_pgn() >> 16) & 0xFF)
			};
			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolConnectionManagement),
			                                                        buffer.data(),
			                                                        buffer.size(),
			                                                        std::static_pointer_cast<InternalControlFunction>(ourControlFunction),
			                                                        session.sessionMessage.get_source().lock());
		}
		return retVal;
	}

	bool TransportProtocolManager::send_request_to_send(const TransportProtocolSession &session) const
	{
		bool retVal = false;
		if (auto source = session.sessionMessage.get_source().lock())
		{
			const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer{
				REQUEST_TO_SEND_MULTIPLEXOR,
				static_cast<std::uint8_t>(session.get_message_data_length() & 0xFF),
				static_cast<std::uint8_t>((session.get_message_data_length() >> 8) & 0xFF),
				session.packetCount,
				0xFF,
				static_cast<std::uint8_t>(session.sessionMessage.get_pgn() & 0xFF),
				static_cast<std::uint8_t>((session.sessionMessage.get_pgn() >> 8) & 0xFF),
				static_cast<std::uint8_t>((session.sessionMessage.get_pgn() >> 16) & 0xFF)
			};
			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolConnectionManagement),
			                                                        buffer.data(),
			                                                        buffer.size(),
			                                                        std::static_pointer_cast<InternalControlFunction>(source),
			                                                        session.sessionMessage.get_destination().lock());
		}
		return retVal;
	}

	bool TransportProtocolManager::send_end_of_session_acknowledgement(const TransportProtocolSession &session) const
	{
		bool retVal = false;
		if (auto ourControlFunction = session.sessionMessage.get_destination().lock())
		{
			std::uint32_t messageLength = session.get_message_data_length();
			std::uint32_t pgn = session.sessionMessage.get_pgn();
			const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer{
				END_OF_MESSAGE_ACKNOWLEDGE_MULTIPLEXOR,
				static_cast<std::uint8_t>(messageLength & 0xFF),
				static_cast<std::uint8_t>((messageLength >> 8) & 0xFF),
				session.packetCount,
				0xFF,
				static_cast<std::uint8_t>(pgn & 0xFF),
				static_cast<std::uint8_t>((pgn >> 8) & 0xFF),
				static_cast<std::uint8_t>((pgn >> 16) & 0xFF),
			};

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::TransportProtocolConnectionManagement),
			                                                        buffer.data(),
			                                                        buffer.size(),
			                                                        std::static_pointer_cast<InternalControlFunction>(ourControlFunction),
			                                                        session.sessionMessage.get_source().lock());
		}
		else
		{
			CANStackLogger::CAN_stack_log(CANStackLogger::LoggingLevel::Warning, "[TP]: Attempted to send EOM to null session");
		}
		return retVal;
	}

	bool isobus::TransportProtocolManager::has_session(std::shared_ptr<ControlFunction> source, std::shared_ptr<ControlFunction> destination)
	{
		return std::any_of(activeSessions.begin(), activeSessions.end(), [&](const TransportProtocolSession &session) {
			return ((session.sessionMessage.get_source().lock() == source) &&
			        (session.sessionMessage.get_destination().lock() == destination));
		});
	}

	TransportProtocolManager::TransportProtocolSession *TransportProtocolManager::get_session(std::shared_ptr<ControlFunction> source,
	                                                                                          std::shared_ptr<ControlFunction> destination)
	{
		return &*std::find_if(activeSessions.begin(), activeSessions.end(), [&](const TransportProtocolSession &session) {
			return ((session.sessionMessage.get_source().lock() == source) &&
			        (session.sessionMessage.get_destination().lock() == destination));
		});
	}
}

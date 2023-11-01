//================================================================================================
/// @file can_transport_message.hpp
///
/// @brief An class that represents a CAN message of arbitrary length being transported.
/// @author Daan Steenbergen
///
/// @copyright 2023 OpenAgriculture
//================================================================================================

#include "isobus/isobus/can_transport_message.hpp"

namespace isobus
{
	std::size_t CANTransportDataVector::size() const
	{
		return vector::size();
	}

	std::uint8_t CANTransportDataVector::get_byte(std::size_t index) const
	{
		return vector::at(index);
	}

	void CANTransportDataVector::set_byte(std::size_t index, std::uint8_t value)
	{
		vector::at(index) = value;
	}

	DataSpan<uint8_t> CANTransportDataVector::data()
	{
		return DataSpan<uint8_t>(vector::data(), vector::size());
	}

	CANTransportDataView::CANTransportDataView(std::uint8_t *ptr, std::size_t len) :
	  DataSpan<std::uint8_t>(ptr, len)
	{
	}

	std::size_t CANTransportDataView::size() const
	{
		return DataSpan::size();
	}

	std::uint8_t CANTransportDataView::get_byte(std::size_t index) const
	{
		return DataSpan::operator[](index);
	}

	void CANTransportDataView::set_byte(std::size_t index, std::uint8_t value)
	{
		DataSpan::operator[](index) = value;
	}

	DataSpan<uint8_t> CANTransportDataView::data()
	{
		return DataSpan<uint8_t>(DataSpan::begin(), DataSpan::size());
	}

	CANTransportMessage::CANTransportMessage(std::uint32_t id,
	                                         std::shared_ptr<ControlFunction> source,
	                                         std::shared_ptr<ControlFunction> destination,
	                                         std::unique_ptr<CANTransportData> transport_data) :
	  identifier(id),
	  source(source),
	  destination(destination),
	  global_destination(nullptr == destination),
	  data(std::move(transport_data))
	{
	}

	CANTransportMessage::CANTransportMessage(CANTransportMessage &&other) noexcept :
	  identifier(other.identifier),
	  source(other.source),
	  destination(other.destination),
	  global_destination(other.global_destination),
	  data(std::move(other.data))
	{
		other.source.reset();
		other.destination.reset();
	}

	CANTransportMessage &CANTransportMessage::operator=(CANTransportMessage &&other) noexcept
	{
		identifier = other.identifier;
		source = other.source;
		destination = other.destination;
		global_destination = other.global_destination;
		data = std::move(other.data);
		other.source.reset();
		other.destination.reset();
		return *this;
	}

	CANTransportData &CANTransportMessage::get_data() const
	{
		return *data;
	}

	std::uint32_t CANTransportMessage::get_pgn() const
	{
		return identifier;
	}

	std::weak_ptr<ControlFunction> CANTransportMessage::get_source() const
	{
		return source;
	}

	std::weak_ptr<ControlFunction> CANTransportMessage::get_destination() const
	{
		return destination;
	}

	bool CANTransportMessage::is_destination_global() const
	{
		return global_destination;
	}

	CANMessage CANTransportMessage::construct_message() const
	{
		CANMessage message(0);
		message.set_identifier(CANIdentifier(CANIdentifier::Type::Extended, identifier, CANIdentifier::PriorityDefault6, destination.lock()->get_address(), source.lock()->get_address()));
		message.set_source_control_function(source.lock());
		message.set_destination_control_function(destination.lock());
		message.set_data(data->data().begin(), static_cast<std::uint32_t>(data->size()));
		return message;
	}

	bool CANTransportMessage::can_continue() const
	{
		return !(source.expired() || (!global_destination && destination.expired()));
	}

}
// namespace isobus
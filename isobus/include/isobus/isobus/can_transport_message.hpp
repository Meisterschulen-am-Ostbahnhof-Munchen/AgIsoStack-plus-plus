//================================================================================================
/// @file can_transport_message.hpp
///
/// @brief An interface class that represents data payload of a CAN message of arbitrary length.
/// @author Daan Steenbergen
///
/// @copyright 2023 OpenAgriculture
//================================================================================================

#ifndef CAN_TRANSPORT_MESSAGE_HPP
#define CAN_TRANSPORT_MESSAGE_HPP

#include "isobus/isobus/can_control_function.hpp"
#include "isobus/isobus/can_message.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace isobus
{
	//================================================================================================
	/// @class DataSpan
	///
	/// @brief A class that represents a span of data of arbitrary length.
	//================================================================================================
	template<typename T>
	class DataSpan
	{
	public:
		/// @brief Construct a new DataSpan object of a writeable array.
		/// @param ptr pointer to the buffer to use.
		/// @param len The length of the buffer.
		DataSpan(T *ptr, std::size_t len) :
		  ptr(ptr),
		  len(len),
		  writeable(true)
		{
		}

		/// @brief Construct a new DataSpan object of a read-only array.
		/// @param ptr pointer to the buffer to use.
		/// @param len The length of the buffer.
		DataSpan(const T *ptr, std::size_t len) :
		  ptr(ptr),
		  len(len),
		  writeable(false)
		{
		}

		/// @brief Get the element at the given index.
		/// @param index The index of the element to get.
		/// @return The element at the given index.
		T &operator[](std::size_t index)
		{
			return ptr[index];
		}

		/// @brief Get the element at the given index.
		/// @param index The index of the element to get.
		/// @return The element at the given index.
		T const &operator[](std::size_t index) const
		{
			return ptr[index];
		}

		/// @brief Get the size of the data span.
		/// @return The size of the data span.
		std::size_t size() const
		{
			return len;
		}

		/// @brief Get the begin iterator.
		/// @return The begin iterator.
		T *begin()
		{
			return ptr;
		}

		/// @brief Get the end iterator.
		/// @return The end iterator.
		T *end()
		{
			return ptr + len;
		}

	private:
		T *ptr;
		std::size_t len;
		bool writeable;
	};

	//================================================================================================
	/// @class CANTransportData
	///
	/// @brief A interface class that represents data payload of a CAN message of arbitrary length.
	//================================================================================================
	class CANTransportData
	{
	public:
		/// @brief Default destructor.
		virtual ~CANTransportData() = default;

		/// @brief Get the size of the data.
		virtual std::size_t size() const = 0;

		/// @brief Get the byte at the given index.
		/// @param index The index of the byte to get.
		/// @return The byte at the given index.
		virtual std::uint8_t get_byte(std::size_t index) const = 0;

		/// @brief Set the byte at the given index.
		/// @param index The index of the byte to set.
		/// @param value The value to set the byte to.
		virtual void set_byte(std::size_t index, std::uint8_t value) = 0;

		/// @brief Get the data span.
		/// @return The data span.
		virtual DataSpan<uint8_t> data() = 0;
	};

	//================================================================================================
	/// @class CANTransportDataVector
	///
	/// @brief A class that represents data of a CAN message by holding a vector of bytes.
	//================================================================================================
	class CANTransportDataVector : public CANTransportData
	  , public std::vector<std::uint8_t>
	{
	public:
		/// @brief Get the size of the data.
		/// @return The size of the data.
		std::size_t size() const override;

		/// @brief Get the byte at the given index.
		/// @param index The index of the byte to get.
		/// @return The byte at the given index.
		std::uint8_t get_byte(std::size_t index) const override;

		/// @brief Set the byte at the given index.
		/// @param index The index of the byte to set.
		/// @param value The value to set the byte to.
		void set_byte(std::size_t index, std::uint8_t value) override;

		/// @brief Get the data span.
		/// @return The data span.
		DataSpan<uint8_t> data() override;
	};

	//================================================================================================
	/// @class CANTransportDataView
	///
	/// @brief A class that represents data of a CAN message by holding a view of an array of bytes.
	/// The view is not owned by this class, it is simply holding a pointer to the array of bytes.
	//================================================================================================
	class CANTransportDataView : public CANTransportData
	  , public DataSpan<std::uint8_t>
	{
	public:
		/// @brief Construct a new CANTransportDataView object.
		/// @param ptr The pointer to the array of bytes.
		/// @param len The length of the array of bytes.
		CANTransportDataView(std::uint8_t *ptr, std::size_t len);

		/// @brief Get the size of the data.
		/// @return The size of the data.
		std::size_t size() const override;

		/// @brief Get the byte at the given index.
		/// @param index The index of the byte to get.
		/// @return The byte at the given index.
		std::uint8_t get_byte(std::size_t index) const override;

		/// @brief Set the byte at the given index.
		/// @param index The index of the byte to set.
		/// @param value The value to set the byte to.
		void set_byte(std::size_t index, std::uint8_t value) override;

		/// @brief Get the data span.
		/// @return The data span.
		DataSpan<uint8_t> data() override;
	};

	// //================================================================================================
	// /// @class CANTransportDataCallback
	// ///
	// /// @brief A class that represents data of a CAN message by using a callback function.
	// //================================================================================================
	// class CANTransportDataCallback : public CANTransportData
	// {
	// public:
	// 	/// @brief Construct a new CANTransportDataCallback object.
	// 	/// @param size_callback The callback function to use for getting the size of the data.
	// 	/// @param get_byte_callback The callback function to use for getting a byte of the data.
	// 	/// @param set_byte_callback The callback function to use for setting a byte of the data.
	// 	CANTransportDataCallback(std::size_t (*size_callback)(),
	// 	                         std::uint8_t (*get_byte_callback)(int),
	// 	                         void (*set_byte_callback)(int, std::uint8_t)) :
	// 	  size_callback(size_callback),
	// 	  get_byte_callback(get_byte_callback),
	// 	  set_byte_callback(set_byte_callback)
	// 	{
	// 	}

	// 	/// @brief Get the size of the data.
	// 	/// @return The size of the data.
	// 	std::size_t size() const override
	// 	{
	// 		return size_callback();
	// 	}

	// 	/// @brief Get the byte at the given index.
	// 	/// @param index The index of the byte to get.
	// 	/// @return The byte at the given index.
	// 	std::uint8_t get_byte(int index) const override
	// 	{
	// 		return get_byte_callback(index);
	// 	}

	// 	/// @brief Set the byte at the given index.
	// 	/// @param index The index of the byte to set.
	// 	/// @param value The value to set the byte to.
	// 	void set_byte(int index, std::uint8_t value) override
	// 	{
	// 		set_byte_callback(index, value);
	// 	}

	// 	/// @brief Get the data span.
	// 	/// @return The data span.
	// 	DataSpan<uint8_t> data() override
	// 	{
	// 		return DataSpan<uint8_t>(nullptr, 0);
	// 	}

	//================================================================================================
	/// @class CANTransportMessage
	///
	/// @brief A class that represents a CAN message of arbitrary length being transported.
	//================================================================================================
	class CANTransportMessage
	{
	public:
		/// @brief Construct a new CANTransportMessage object.
		/// @param id The CAN message ID.
		/// @param source The source control function.
		/// @param destination The destination control function.
		/// @param transport_data The data payload of the CAN message.
		CANTransportMessage(std::uint32_t id,
		                    std::shared_ptr<ControlFunction> source,
		                    std::shared_ptr<ControlFunction> destination,
		                    std::unique_ptr<CANTransportData> transport_data);

		/// @brief Destructor.
		~CANTransportMessage() = default;

		/// @brief Move constructor.
		/// @param other The other CANTransportMessage object.
		CANTransportMessage(CANTransportMessage &&other) noexcept;

		/// @brief Move assignment operator.
		/// @param other The other CANTransportMessage object.
		CANTransportMessage &operator=(CANTransportMessage &&other) noexcept;

		/// @brief Get the data payload of the CAN message.
		/// @return The data payload of the CAN message.
		CANTransportData &get_data() const;

		/// @brief Get the Parameter Group Number (PGN) of the transported message.
		/// @return The pgn of the transported message.
		std::uint32_t get_pgn() const;

		/// @brief Get the source control function.
		/// @return The source control function.
		std::weak_ptr<ControlFunction> get_source() const;

		/// @brief Get the destination control function.
		/// @return The destination control function.
		std::weak_ptr<ControlFunction> get_destination() const;

		bool is_destination_global() const;

		/// @brief Construct a CANMessage from this object.
		/// @return The newly created CANMessage.
		CANMessage construct_message() const;

		/// @brief Check if the message can continue to be transported.
		/// @return true if the message can continue to be transported.
		bool can_continue() const;

	private:
		std::uint32_t identifier;
		std::weak_ptr<ControlFunction> source;
		std::weak_ptr<ControlFunction> destination;
		bool global_destination;
		std::unique_ptr<CANTransportData> data;
	};

} // namespace isobus

#endif // CAN_TRANSPORT_MESSAGE_HPP

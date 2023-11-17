//================================================================================================
/// @file isobus_virtual_terminal_server.cpp
///
/// @brief Implements portions of an abstract VT server.
/// @author Adrian Del Grosso
///
/// @copyright 2023 Adrian Del Grosso
//================================================================================================
#include "isobus/isobus/isobus_virtual_terminal_server.hpp"
#include "isobus/isobus/can_general_parameter_group_numbers.hpp"
#include "isobus/isobus/can_message.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/utility/system_timing.hpp"

namespace isobus
{
	VirtualTerminalServer::VirtualTerminalServer(std::shared_ptr<InternalControlFunction> controlFunctionToUse) :
	  languageCommandInterface(controlFunctionToUse, true),
	  serverInternalControlFunction(controlFunctionToUse)
	{
	}

	VirtualTerminalServer ::~VirtualTerminalServer()
	{
		if (initialized)
		{
			CANNetworkManager::CANNetwork.remove_any_control_function_parameter_group_number_callback(static_cast<std::uint32_t>(CANLibParameterGroupNumber::ECUtoVirtualTerminal),
			                                                                                          process_rx_message,
			                                                                                          this);
		}
	}

	void VirtualTerminalServer::initialize()
	{
		if (!initialized)
		{
			CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(static_cast<std::uint32_t>(CANLibParameterGroupNumber::ECUtoVirtualTerminal),
			                                                                                       process_rx_message,
			                                                                                       this);
		}
	}

	bool VirtualTerminalServer::get_initialized() const
	{
		return initialized;
	}

	std::shared_ptr<VirtualTerminalServerManagedWorkingSet> VirtualTerminalServer::get_active_working_set() const
	{
		return activeWorkingSet;
	}

	VirtualTerminalBase::GraphicMode VirtualTerminalServer::get_graphic_mode() const
	{
		return VirtualTerminalBase::GraphicMode::TwoHundredFiftySixColour;
	}

	std::uint8_t VirtualTerminalServer::get_powerup_time() const
	{
		return 0xFF;
	}

	std::uint8_t VirtualTerminalServer::get_supported_small_fonts_bitfield() const
	{
		return 0xFF;
	}

	std::uint8_t VirtualTerminalServer::get_supported_large_fonts_bitfield() const
	{
		return 0xFF;
	}

	EventDispatcher<std::shared_ptr<VirtualTerminalServerManagedWorkingSet>> &VirtualTerminalServer::get_on_repaint_event_dispatcher()
	{
		return onRepaintEventDispatcher;
	}

	EventDispatcher<std::shared_ptr<VirtualTerminalServerManagedWorkingSet>, std::uint16_t, std::uint16_t> &VirtualTerminalServer::get_on_change_active_mask_event_dispatcher()
	{
		return onChangeActiveMaskEventDispatcher;
	}

	EventDispatcher<std::shared_ptr<VirtualTerminalServerManagedWorkingSet>, std::uint16_t, bool> &VirtualTerminalServer::get_on_hide_show_object_event_dispatcher()
	{
		return onHideShowObjectEventDispatcher;
	}

	EventDispatcher<std::shared_ptr<VirtualTerminalServerManagedWorkingSet>, std::uint16_t, bool> &VirtualTerminalServer::get_on_enable_disable_object_event_dispatcher()
	{
		return onEnableDisableObjectEventDispatcher;
	}

	EventDispatcher<std::shared_ptr<VirtualTerminalServerManagedWorkingSet>, std::uint16_t, std::uint32_t> &VirtualTerminalServer::get_on_change_numeric_value_event_dispatcher()
	{
		return onChangeNumericValueEventDispatcher;
	}

	EventDispatcher<std::shared_ptr<VirtualTerminalServerManagedWorkingSet>, std::uint16_t, std::uint16_t, std::int8_t, std::int8_t> &VirtualTerminalServer::get_on_change_child_location_event_dispatcher()
	{
		return onChangeChildLocationEventDispatcher;
	}

	EventDispatcher<std::shared_ptr<VirtualTerminalServerManagedWorkingSet>, std::uint16_t, std::string> &VirtualTerminalServer::get_on_change_string_value_event_dispatcher()
	{
		return onChangeStringValueEventDispatcher;
	}

	EventDispatcher<std::shared_ptr<VirtualTerminalServerManagedWorkingSet>, std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t> &VirtualTerminalServer::get_on_change_child_position_event_dispatcher()
	{
		return onChangeChildPositionEventDispatcher;
	}

	LanguageCommandInterface &VirtualTerminalServer::get_language_command_interface()
	{
		return languageCommandInterface;
	}

	bool VirtualTerminalServer::check_if_source_is_managed(const CANMessage &message)
	{
		// Check if we're managing this CF
		bool retVal = false;

		// This is the static callback for the instance.
		// See if we need to set up a new managed working set.
		for (auto &cf : managedWorkingSetList)
		{
			if (cf->get_control_function() == message.get_source_control_function())
			{
				// Found a match
				retVal = true;
				break;
			}
		}

		if (!retVal)
		{
			if ((message.get_data()[0] == static_cast<std::uint8_t>(Function::WorkingSetMaintenanceMessage)) &&
			    (message.get_data()[1] & 0x01)) // Init bit is set
			{
				// This CF is probably trying to initiate communication with us.
				managedWorkingSetList.emplace_back(std::move(std::make_shared<VirtualTerminalServerManagedWorkingSet>(message.get_source_control_function())));
				auto &data = message.get_data();

				CANStackLogger::info("[VT Server]: Client %u initiated working set maintenance messages with version %u", managedWorkingSetList.back()->get_control_function()->get_address(), data[2]);
				if (data[2] > get_vt_version_byte(get_version()))
				{
					CANStackLogger::warn("[VT Server]: Client %u version %u is not supported", managedWorkingSetList.back()->get_control_function()->get_address(), data[2]);
				}
				managedWorkingSetList.back()->set_working_set_maintenance_message_timestamp_ms(SystemTiming::get_timestamp_ms());
			}
			else
			{
				// Whomever this is has probably timed out. Send them a NACK
				CANStackLogger::warn("[VT Server]: Received a non-status message from a client at address %u, but they are not connected to this VT.", message.get_identifier().get_source_address());
				send_acknowledgement(AcknowledgementType::Negative, static_cast<std::uint32_t>(CANLibParameterGroupNumber::ECUtoVirtualTerminal), serverInternalControlFunction, message.get_source_control_function());
			}
		}
		return retVal;
	}

	std::uint8_t VirtualTerminalServer::get_vt_version_byte(VTVersion version)
	{
		std::uint8_t retVal = 2;

		switch (version)
		{
			case VTVersion::Version3:
			{
				retVal = 3;
			}
			break;

			case VTVersion::Version4:
			{
				retVal = 4;
			}
			break;

			case VTVersion::Version5:
			{
				retVal = 5;
			}
			break;

			case VTVersion::Version6:
			{
				retVal = 6;
			}
			break;

			default:
			{
				// Report version 2
			}
			break;
		}
		return retVal;
	}

	void VirtualTerminalServer::process_rx_message(const CANMessage &message, void *parent)
	{
		auto parentServer = static_cast<VirtualTerminalServer *>(parent);
		if ((nullptr != message.get_source_control_function()) &&
		    (nullptr != parentServer) &&
		    ((CAN_DATA_LENGTH <= message.get_data_length()) ||
		     ((message.get_data_length() > 5) && (static_cast<std::uint8_t>(Function::ChangeStringValueCommand) == message.get_uint8_at(0)))) && // Technically this message can be 6 bytes
		    (parentServer->check_if_source_is_managed(message)))
		{
			for (auto cf : parentServer->managedWorkingSetList)
			{
				if (cf->get_control_function() == message.get_source_control_function())
				{
					auto &data = message.get_data();

					switch (message.get_identifier().get_parameter_group_number())
					{
						case static_cast<std::uint32_t>(CANLibParameterGroupNumber::ECUtoVirtualTerminal):
						{
							switch (static_cast<Function>(data[0]))
							{
								case Function::ObjectPoolTransferMessage:
								{
									auto tempPool = data; // Make a copy of the data (ouch)
									tempPool.erase(tempPool.begin()); // Strip off the mux byte (double ouch, good thing this is rare)
									CANStackLogger::info("[VT Server]: An ecu at address %u transferred %u bytes of object pool data to us.", message.get_identifier().get_source_address(), static_cast<std::uint32_t>(tempPool.size()));
									cf->add_iop_raw_data(tempPool);
								}
								break;

								case Function::GetMemoryMessage:
								{
									std::uint32_t requiredMemory = (data[2] | (static_cast<std::uint32_t>(data[3]) << 8) | (static_cast<std::uint32_t>(data[4]) << 16) | (static_cast<std::uint32_t>(data[5]) << 24));
									bool isEnoughMemory = parentServer->get_is_enough_memory(requiredMemory);
									CANStackLogger::info("[VT Server]: An ecu requested %u bytes of memory.", requiredMemory);

									if (!isEnoughMemory)
									{
										CANStackLogger::warn("[VT Server]: Callback indicated there is NOT enough memory.", requiredMemory);
									}
									else
									{
										CANStackLogger::debug("[VT Server]: Callback indicated there may be enough memory, but since there is overhead associated to object storage it is impossible to be sure.", requiredMemory);
									}

									std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = { 0 };
									buffer[0] = static_cast<std::uint8_t>(Function::GetMemoryMessage);
									buffer[1] = static_cast<std::uint8_t>(parentServer->get_version());
									buffer[2] = static_cast<std::uint8_t>(!isEnoughMemory);
									buffer[3] = 0xFF; // Reserved
									buffer[4] = 0xFF; // Reserved
									buffer[5] = 0xFF; // Reserved
									buffer[6] = 0xFF; // Reserved
									buffer[7] = 0xFF; // Reserved
									CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
									                                               buffer.data(),
									                                               CAN_DATA_LENGTH,
									                                               parentServer->serverInternalControlFunction,
									                                               message.get_source_control_function(),
									                                               CANIdentifier::PriorityLowest7);
								}
								break;

								case Function::GetNumberOfSoftKeysMessage:
								{
									std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = { 0 };
									buffer[0] = static_cast<std::uint8_t>(Function::GetNumberOfSoftKeysMessage);
									buffer[1] = parentServer->get_number_of_navigation_soft_keys(); // No navigation softkeys
									buffer[2] = 0xFF; // Reserved
									buffer[3] = 0xFF; // Reserved
									buffer[4] = parentServer->get_soft_key_descriptor_x_pixel_width(); // Pixel width of X softkey descriptor
									buffer[5] = parentServer->get_soft_key_descriptor_y_pixel_width(); // Pixel width of Y softkey descriptor
									buffer[6] = parentServer->get_number_of_possible_virtual_soft_keys_in_soft_key_mask(); // Number of possible virtual Soft Keys in a Soft Key Mask
									buffer[7] = parentServer->get_number_of_physical_soft_keys(); // No physical softkeys

									CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
									                                               buffer.data(),
									                                               CAN_DATA_LENGTH,
									                                               parentServer->serverInternalControlFunction,
									                                               message.get_source_control_function(),
									                                               CANIdentifier::PriorityLowest7);
								}
								break;

								case Function::GetTextFontDataMessage:
								{
									std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = { 0 };
									buffer[0] = static_cast<std::uint8_t>(Function::GetTextFontDataMessage);
									buffer[1] = 0xFF; // Reserved
									buffer[2] = 0xFF; // Reserved
									buffer[3] = 0xFF; // Reserved
									buffer[4] = 0xFF; // Reserved
									buffer[5] = parentServer->get_supported_small_fonts_bitfield(); // Say we support all small fonts
									buffer[6] = parentServer->get_supported_large_fonts_bitfield(); // Say we support all large fonts
									buffer[7] = 0x8F; // Support normal, bold, italic, proportional
									CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
									                                               buffer.data(),
									                                               CAN_DATA_LENGTH,
									                                               parentServer->serverInternalControlFunction,
									                                               message.get_source_control_function(),
									                                               CANIdentifier::PriorityLowest7);
								}
								break;

								case Function::GetHardwareMessage:
								{
									std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = { 0 };
									buffer[0] = static_cast<std::uint8_t>(Function::GetHardwareMessage);
									buffer[1] = parentServer->get_powerup_time();
									buffer[2] = static_cast<std::uint8_t>(parentServer->get_graphic_mode()); // 256 Colour Mode by default
									buffer[3] = 0x0F; // Support pointing event message
									buffer[4] = (parentServer->get_data_mask_area_size_x_pixels() & 0xFF); // X Pixels LSB
									buffer[5] = (parentServer->get_data_mask_area_size_x_pixels() >> 8); // X Pixels MSB
									buffer[6] = (parentServer->get_data_mask_area_size_y_pixels() & 0xFF); // Y Pixels LSB
									buffer[7] = (parentServer->get_data_mask_area_size_y_pixels() >> 8); // Y Pixels MSB
									CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
									                                               buffer.data(),
									                                               CAN_DATA_LENGTH,
									                                               parentServer->serverInternalControlFunction,
									                                               message.get_source_control_function(),
									                                               CANIdentifier::PriorityLowest7);
								}
								break;

								case Function::GetSupportedWidecharsMessage:
								{
									std::vector<std::uint8_t> wideCharRangeArray;
									std::uint8_t numberOfRanges = 0;
									std::uint8_t codePlane = data.at(1);
									std::uint16_t firstWideCharInInquiryRange = static_cast<std::uint16_t>(data.at(2)) | (static_cast<std::uint16_t>(data.at(3)) << 8);
									std::uint16_t lastWideCharInInquiryRange = static_cast<std::uint16_t>(data.at(4)) | (static_cast<std::uint16_t>(data.at(5)) << 8);
									auto errorCode = parentServer->get_supported_wide_chars(codePlane, firstWideCharInInquiryRange, lastWideCharInInquiryRange, numberOfRanges, wideCharRangeArray);

									std::vector<std::uint8_t> buffer;
									buffer.push_back(static_cast<std::uint8_t>(Function::GetSupportedWidecharsMessage));
									buffer.push_back(codePlane);
									buffer.push_back(static_cast<std::uint8_t>(firstWideCharInInquiryRange & 0xFF));
									buffer.push_back(static_cast<std::uint8_t>((firstWideCharInInquiryRange >> 8) & 0xFF));
									buffer.push_back(static_cast<std::uint8_t>(lastWideCharInInquiryRange & 0xFF));
									buffer.push_back(static_cast<std::uint8_t>((lastWideCharInInquiryRange >> 8) & 0xFF));
									buffer.push_back(static_cast<std::uint8_t>(errorCode));
									buffer.push_back(numberOfRanges);

									for (const auto &range : wideCharRangeArray)
									{
										buffer.push_back(range);
									}
									CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
									                                               buffer.data(),
									                                               static_cast<std::uint32_t>(buffer.size()),
									                                               parentServer->serverInternalControlFunction,
									                                               message.get_source_control_function(),
									                                               CANIdentifier::PriorityLowest7);
								}
								break;

								case Function::GetVersionsMessage:
								{
									auto versions = parentServer->get_versions(message.get_source_control_function()->get_NAME());

									std::vector<std::uint8_t> buffer;
									buffer.push_back(static_cast<std::uint32_t>(Function::GetVersionsResponse));

									CANStackLogger::debug("[VT Server]: Client %u requests stored versions", message.get_source_control_function()->get_address());

									if (versions.size() > 255)
									{
										CANStackLogger::warn("[VT Server]: get_versions returned too many versions! This client should really delete some.");
									}

									buffer.push_back(static_cast<std::uint8_t>(versions.size() & 0xFF));

									for (const auto &version : versions)
									{
										for (const auto &versionByte : version)
										{
											buffer.push_back(versionByte);
										}
									}

									while (buffer.size() < CAN_DATA_LENGTH)
									{
										buffer.push_back(0xFF);
									}
									CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
									                                               buffer.data(),
									                                               static_cast<std::uint32_t>(buffer.size()),
									                                               parentServer->serverInternalControlFunction,
									                                               message.get_source_control_function(),
									                                               CANIdentifier::PriorityLowest7);
								}
								break;

								case Function::LoadVersionCommand:
								{
									constexpr std::uint8_t VERSION_LABEL_LENGTH = 7;
									std::uint8_t errorCodes = 0x01; // Version label incorrect
									std::vector<std::uint8_t> versionLabel;

									versionLabel.reserve(VERSION_LABEL_LENGTH);

									for (std::uint_fast8_t i = 0; i < VERSION_LABEL_LENGTH; i++)
									{
										versionLabel.push_back(data[i + 1]);
									}

									auto loadedVersion = parentServer->load_version(versionLabel, message.get_source_control_function()->get_NAME());
									if (!loadedVersion.empty())
									{
										cf->add_iop_raw_data(loadedVersion);
										errorCodes = 0;
									}

									if (cf->get_any_object_pools())
									{
										cf->start_parsing_thread();
										CANStackLogger::debug("[VT Server]: Starting parsing thread for loaded pool data.");
									}
									std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = { 0 };
									buffer[0] = static_cast<std::uint8_t>(Function::LoadVersionCommand);
									buffer[1] = 0xFF; // Reserved
									buffer[2] = 0xFF; // Reserved
									buffer[3] = 0xFF; // Reserved
									buffer[4] = 0xFF; // Reserved
									buffer[5] = errorCodes;
									buffer[6] = 0xFF; // Reserved
									buffer[7] = 0xFF; // Reserved
									CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
									                                               buffer.data(),
									                                               CAN_DATA_LENGTH,
									                                               parentServer->serverInternalControlFunction,
									                                               message.get_source_control_function(),
									                                               CANIdentifier::PriorityLowest7);
								}
								break;

								case Function::StoreVersionCommand:
								{
									if (cf->get_any_object_pools())
									{
										constexpr std::uint8_t VERSION_LABEL_LENGTH = 7;
										std::string cfName = std::to_string(cf->get_control_function()->get_NAME().get_full_name());
										std::vector<std::uint8_t> versionLabel;
										bool allPoolsSaved = true;
										versionLabel.reserve(VERSION_LABEL_LENGTH);

										for (std::uint_fast8_t i = 0; i < VERSION_LABEL_LENGTH; i++)
										{
											versionLabel.push_back(static_cast<char>(data[i + 1]));
										}

										for (std::size_t i = 0; i < cf->get_number_iop_files(); i++)
										{
											bool didSave = parentServer->save_version(cf->get_iop_raw_data(i), versionLabel, message.get_source_control_function()->get_NAME());

											if (didSave)
											{
												CANStackLogger::info("[VT Server]: Object pool %u for NAME %s was stored", i, cfName);
											}
											else
											{
												CANStackLogger::warn("[VT Server]: Object pool %u for NAME %s could not be stored.", i, cfName);
												allPoolsSaved = false;
												break;
											}
										}

										std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = { 0 };
										buffer[0] = static_cast<std::uint8_t>(Function::StoreVersionCommand);
										buffer[1] = 0xFF; // Reserved
										buffer[2] = 0xFF; // Reserved
										buffer[3] = 0xFF; // Reserved
										buffer[4] = 0xFF; // Reserved
										if (allPoolsSaved)
										{
											buffer[5] = 0; // No error
										}
										else
										{
											buffer[5] = 0x04; // Any other error
										}
										buffer[6] = 0xFF; // Reserved
										buffer[7] = 0xFF; // Reserved
										CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
										                                               buffer.data(),
										                                               CAN_DATA_LENGTH,
										                                               parentServer->serverInternalControlFunction,
										                                               message.get_source_control_function(),
										                                               CANIdentifier::PriorityLowest7);
									}
									else
									{
										// Whomever this is is being bad, send them a NACK
										parentServer->send_acknowledgement(AcknowledgementType::Negative, static_cast<std::uint32_t>(CANLibParameterGroupNumber::ECUtoVirtualTerminal), parentServer->serverInternalControlFunction, cf->get_control_function());
									}
								}
								break;

								case Function::EndOfObjectPoolMessage:
								{
									if (cf->get_any_object_pools())
									{
										cf->start_parsing_thread();
									}
									else
									{
										CANStackLogger::warn("[VT Server]: End of object pool message ignored - no object pools are loaded for the source control function");
									}
								}
								break;

								case Function::WorkingSetMaintenanceMessage:
								{
									if (0 != cf->get_working_set_maintenance_message_timestamp_ms())
									{
										cf->set_working_set_maintenance_message_timestamp_ms(SystemTiming::get_timestamp_ms());
									}
								}
								break;

								case Function::ChangeNumericValueCommand:
								{
									std::uint32_t value = (static_cast<std::uint32_t>(data[4]) | (static_cast<std::uint32_t>(data[5]) << 8) | (static_cast<std::uint32_t>(data[6]) << 16) | (static_cast<std::uint32_t>(data[7]) << 24));
									auto objectId = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto lTargetObject = cf->get_object_by_id(objectId);
									bool logSuccess = true;

									if (nullptr != lTargetObject)
									{
										switch (lTargetObject->get_object_type())
										{
											case VirtualTerminalObjectType::InputBoolean:
											{
												std::static_pointer_cast<InputBoolean>(lTargetObject)->set_value(value);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::InputNumber:
											{
												std::static_pointer_cast<InputNumber>(lTargetObject)->set_value(value);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::InputList:
											{
												std::static_pointer_cast<InputList>(lTargetObject)->set_value(value);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::OutputNumber:
											{
												std::static_pointer_cast<OutputNumber>(lTargetObject)->set_value(value);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::OutputList:
											{
												std::static_pointer_cast<OutputList>(lTargetObject)->set_value(value);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::OutputMeter:
											{
												std::static_pointer_cast<OutputMeter>(lTargetObject)->set_value(value);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::OutputLinearBarGraph:
											{
												std::static_pointer_cast<OutputLinearBarGraph>(lTargetObject)->set_value(value);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::OutputArchedBarGraph:
											{
												std::static_pointer_cast<OutputArchedBarGraph>(lTargetObject)->set_value(value);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::NumberVariable:
											{
												std::static_pointer_cast<NumberVariable>(lTargetObject)->set_value(value);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::ObjectPointer:
											{
												std::static_pointer_cast<ObjectPointer>(lTargetObject)->pop_child();
												std::static_pointer_cast<ObjectPointer>(lTargetObject)->add_child(value, 0, 0);
												parentServer->onChangeNumericValueEventDispatcher.call(cf, objectId, value);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
											}
											break;

											case VirtualTerminalObjectType::ExternalObjectPointer:
											{
												std::uint16_t externalReferenceNAMEObjectIdD = (static_cast<std::uint16_t>(data[4]) | (static_cast<std::uint16_t>(data[5]) << 8));
												std::uint16_t referencedObjectID = (static_cast<std::uint16_t>(data[6]) | (static_cast<std::uint16_t>(data[7]) << 8));
												std::static_pointer_cast<ExternalObjectPointer>(lTargetObject)->set_external_reference_name_id(externalReferenceNAMEObjectIdD);
												std::static_pointer_cast<ExternalObjectPointer>(lTargetObject)->set_external_object_id(referencedObjectID);
												parentServer->send_change_numeric_value_response(objectId, 0, value, cf->get_control_function());
												// Todo: event dispatcher
											}
											break;

											case VirtualTerminalObjectType::Animation:
											{
												//Todo std::static_pointer_cast<Animation>(lTargetObject)->set_value(value);
												// parentServer->onChangeNumericValueEventDispatcher.call(objectId, value);
												parentServer->send_change_numeric_value_response(objectId, (1 << static_cast<std::uint8_t>(ChangeNumericValueErrorBit::AnyOtherError)), value, cf->get_control_function());
												CANStackLogger::warn("[VT Server]: Client %u change numeric value for animation not implemented yet", cf->get_control_function()->get_address());
												logSuccess = false;
											}
											break;

											default:
											{
												parentServer->send_change_numeric_value_response(objectId, (1 << static_cast<std::uint8_t>(ChangeNumericValueErrorBit::InvalidObjectID)), value, cf->get_control_function());
												CANStackLogger::warn("[VT Server]: Client %u change numeric value invalid object type. ID: %u", cf->get_control_function()->get_address(), objectId);
												logSuccess = false;
											}
											break;
										}

										if (logSuccess)
										{
											CANStackLogger::debug("[VT Server]: Client %u change numeric value command: change object ID %u to be %u", cf->get_control_function()->get_address(), objectId, value);
										}
									}
									else
									{
										parentServer->send_change_numeric_value_response(objectId, (1 << static_cast<std::uint8_t>(ChangeNumericValueErrorBit::InvalidObjectID)), value, cf->get_control_function());
										CANStackLogger::warn("[VT Server]: Client %u change numeric value invalid object ID of %u", cf->get_control_function()->get_address(), objectId);
									}
								}
								break;

								case Function::HideShowObjectCommand:
								{
									std::uint16_t objectId = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto lTargetObject = cf->get_object_by_id(objectId);

									if ((nullptr != lTargetObject) && (VirtualTerminalObjectType::Container == lTargetObject->get_object_type()))
									{
										std::static_pointer_cast<Container>(lTargetObject)->set_hidden(0 == data[3]);
										parentServer->send_hide_show_object_response(objectId, 0, (0 != data[3]), cf->get_control_function());
										parentServer->onHideShowObjectEventDispatcher.call(cf, objectId, (0 == data[3]));

										if (0 == data[3])
										{
											CANStackLogger::debug("[VT Server]: Client %u hide object command %u", cf->get_control_function()->get_address(), objectId);
										}
										else
										{
											CANStackLogger::debug("[VT Server]: Client %u show object command %u", cf->get_control_function()->get_address(), objectId);
										}
									}
									else
									{
										parentServer->send_hide_show_object_response(objectId, (1 << static_cast<std::uint8_t>(HideShowObjectErrorBit::InvalidObjectID)), (0 != data[3]), cf->get_control_function());
										CANStackLogger::warn("[VT Server]: Client %u hide/show object command failed. It can only affect containers! ID: %u", cf->get_control_function()->get_address(), objectId);
									}
								}
								break;

								case Function::EnableDisableObjectCommand:
								{
									std::uint16_t objectId = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto lTargetObject = cf->get_object_by_id(objectId);

									if (nullptr != lTargetObject)
									{
										if (data[3] <= 1)
										{
											switch (lTargetObject->get_object_type())
											{
												case VirtualTerminalObjectType::InputBoolean:
												{
													std::static_pointer_cast<InputBoolean>(lTargetObject)->set_enabled((0 != data[3]));
													parentServer->send_enable_disable_object_response(objectId, 0, (0 != data[3]), cf->get_control_function());
													parentServer->onEnableDisableObjectEventDispatcher.call(cf, objectId, (0 != data[3]));
												}
												break;

												case VirtualTerminalObjectType::InputList:
												{
													std::static_pointer_cast<InputList>(lTargetObject)->set_option(InputList::Options::Enabled, (0 != data[3]));
													parentServer->send_enable_disable_object_response(objectId, 0, (0 != data[3]), cf->get_control_function());
													parentServer->onEnableDisableObjectEventDispatcher.call(cf, objectId, (0 != data[3]));
												}
												break;

												case VirtualTerminalObjectType::InputString:
												{
													std::static_pointer_cast<InputString>(lTargetObject)->set_enabled((0 != data[3]));
													parentServer->send_enable_disable_object_response(objectId, 0, (0 != data[3]), cf->get_control_function());
													parentServer->onEnableDisableObjectEventDispatcher.call(cf, objectId, (0 != data[3]));
												}
												break;

												case VirtualTerminalObjectType::InputNumber:
												{
													std::static_pointer_cast<InputNumber>(lTargetObject)->set_option2(InputNumber::Options2::Enabled, (0 != data[3]));
													parentServer->send_enable_disable_object_response(objectId, 0, (0 != data[3]), cf->get_control_function());
													parentServer->onEnableDisableObjectEventDispatcher.call(cf, objectId, (0 != data[3]));
												}
												break;

												case VirtualTerminalObjectType::Button:
												{
													std::static_pointer_cast<Button>(lTargetObject)->set_option(Button::Options::Disabled, (0 == data[3]));
													parentServer->send_enable_disable_object_response(objectId, 0, (0 != data[3]), cf->get_control_function());
													parentServer->onEnableDisableObjectEventDispatcher.call(cf, objectId, (0 != data[3]));
												}
												break;

												default:
												{
													parentServer->send_enable_disable_object_response(objectId, (1 << static_cast<std::uint8_t>(EnableDisableObjectErrorBit::InvalidObjectID)), (0 != data[3]), cf->get_control_function());
												}
												break;
											}
										}
										else
										{
											parentServer->send_enable_disable_object_response(objectId, (1 << static_cast<std::uint8_t>(EnableDisableObjectErrorBit::InvalidEnableDisableCommandValue)), (0 != data[3]), cf->get_control_function());
										}
									}
									else
									{
										parentServer->send_enable_disable_object_response(objectId, (1 << static_cast<std::uint8_t>(EnableDisableObjectErrorBit::InvalidObjectID)), (0 != data[3]), cf->get_control_function());
									}
								}
								break;

								case Function::ChangeChildLocationCommand:
								{
									auto parentObjectId = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto objectID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[3]) | (static_cast<std::uint16_t>(data[4]) << 8));
									auto parentObject = cf->get_object_by_id(parentObjectId);

									if (nullptr != parentObject)
									{
										auto lTargetObject = cf->get_object_by_id(objectID);

										if (nullptr != lTargetObject)
										{
											std::int8_t xRelativeChange = static_cast<std::int8_t>(static_cast<std::int16_t>(data[5]) - 127);
											std::int8_t yRelativeChange = static_cast<std::int8_t>(static_cast<std::int16_t>(data[6]) - 127);
											bool anyObjectMatched = parentObject->offset_all_children_x_with_id(objectID, xRelativeChange, yRelativeChange);

											parentServer->onChangeChildLocationEventDispatcher.call(cf, parentObjectId, objectID, xRelativeChange, yRelativeChange);

											if (anyObjectMatched)
											{
												parentServer->send_change_child_location_response(parentObjectId, objectID, 0, cf->get_control_function());
												CANStackLogger::debug("[VT Server]: Client %u change child location command. Parent: %u, Target: %u, X-Offset: %d, Y-Offset: %d", cf->get_control_function()->get_address(), parentObjectId, objectID, xRelativeChange, yRelativeChange);
											}
											else
											{
												parentServer->send_change_child_location_response(parentObjectId, objectID, (1 << static_cast<std::uint8_t>(ChangeChildLocationorPositionErrorBit::TargetObjectDoesNotExistOrIsNotApplicable)), cf->get_control_function());
												CANStackLogger::warn("[VT Server]: Client %u change child location failed because the target object with ID %u isn't applicable", cf->get_control_function()->get_address(), objectID);
											}
										}
										else
										{
											parentServer->send_change_child_location_response(parentObjectId, objectID, (1 << static_cast<std::uint8_t>(ChangeChildLocationorPositionErrorBit::TargetObjectDoesNotExistOrIsNotApplicable)), cf->get_control_function());
											CANStackLogger::warn("[VT Server]: Client %u change child location failed because the target object with ID %u doesn't exist", cf->get_control_function()->get_address(), objectID);
										}
									}
									else
									{
										parentServer->send_change_child_location_response(parentObjectId, objectID, (1 << static_cast<std::uint8_t>(ChangeChildLocationorPositionErrorBit::ParentObjectDoesntExistOrIsNotAParentOfSpecifiedObject)), cf->get_control_function());
										CANStackLogger::warn("[VT Server]: Client %u change child location failed because the parent object with ID %u doesn't exist", cf->get_control_function()->get_address(), parentObject);
									}
								}
								break;

								case Function::ChangeActiveMaskCommand:
								{
									auto workingSetObjectId = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto newActiveMaskObjectId = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[3]) | (static_cast<std::uint16_t>(data[4]) << 8));
									auto workingSetObject = cf->get_object_by_id(workingSetObjectId);

									if (nullptr != workingSetObject)
									{
										if (nullptr != cf->get_object_by_id(newActiveMaskObjectId))
										{
											std::static_pointer_cast<WorkingSet>(workingSetObject)->set_active_mask(newActiveMaskObjectId);
											parentServer->send_change_active_mask_response(newActiveMaskObjectId, 0, cf->get_control_function());
											parentServer->onChangeActiveMaskEventDispatcher.call(cf, workingSetObjectId, newActiveMaskObjectId);
											CANStackLogger::debug("[VT Server]: Client %u changed active mask to object %u for working set object %u", cf->get_control_function()->get_address(), newActiveMaskObjectId, workingSetObjectId);
										}
										else
										{
											parentServer->send_change_active_mask_response(newActiveMaskObjectId, (1 << static_cast<std::uint8_t>(ChangeActiveMaskErrorBit::InvalidMaskObjectID)), cf->get_control_function());
											CANStackLogger::warn("[VT Server]: Client %u change active mask failed because the new mask object ID %u was not valid.", cf->get_control_function()->get_address(), newActiveMaskObjectId);
										}
									}
									else
									{
										parentServer->send_change_active_mask_response(newActiveMaskObjectId, (1 << static_cast<std::uint8_t>(ChangeActiveMaskErrorBit::InvalidWorkingSetObjectID)), cf->get_control_function());
										CANStackLogger::warn("[VT Server]: Client %u change active mask failed because the working set object ID %u was not valid.", cf->get_control_function()->get_address(), workingSetObjectId);
									}
								}
								break;

								case Function::GetSupportedObjectsMessage:
								{
									parentServer->send_supported_objects(message.get_source_control_function());
									CANStackLogger::debug("[VT Server]: Sent supported object list to client %u", cf->get_control_function()->get_address());
								}
								break;

								case Function::ChangeStringValueCommand:
								{
									std::uint16_t objectIdToChange = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									std::uint16_t numberOfBytesInString = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[3]) | (static_cast<std::uint16_t>(data[4]) << 8));
									auto stringObject = cf->get_object_by_id(objectIdToChange);

									if (message.get_data_length() >= (numberOfBytesInString + 5))
									{
										if (nullptr != stringObject)
										{
											std::string newStringValue;

											for (std::uint32_t i = 0; i < numberOfBytesInString; i++)
											{
												newStringValue.push_back(static_cast<char>(data.at(5 + i)));
											}

											switch (stringObject->get_object_type())
											{
												case VirtualTerminalObjectType::StringVariable:
												{
													std::static_pointer_cast<StringVariable>(stringObject)->set_value(newStringValue);
													parentServer->send_change_string_value_response(objectIdToChange, 0, message.get_source_control_function());
													parentServer->onRepaintEventDispatcher.call(cf);
													CANStackLogger::debug("[VT Server]: Client %u change string value command for string variable object %u. Value: " + newStringValue, cf->get_control_function()->get_address(), objectIdToChange);
												}
												break;

												case VirtualTerminalObjectType::OutputString:
												{
													std::static_pointer_cast<OutputString>(stringObject)->set_value(newStringValue);
													parentServer->send_change_string_value_response(objectIdToChange, 0, message.get_source_control_function());
													parentServer->onRepaintEventDispatcher.call(cf);
													CANStackLogger::debug("[VT Server]: Client %u change string value command for output string object %u. Value: " + newStringValue, cf->get_control_function()->get_address(), objectIdToChange);
												}
												break;

												case VirtualTerminalObjectType::InputString:
												{
													std::static_pointer_cast<InputString>(stringObject)->set_value(newStringValue);
													parentServer->send_change_string_value_response(objectIdToChange, 0, message.get_source_control_function());
													parentServer->onRepaintEventDispatcher.call(cf);
													CANStackLogger::debug("[VT Server]: Client %u change string value command for input string object %u. Value: " + newStringValue, cf->get_control_function()->get_address(), objectIdToChange);
												}
												break;

												default:
												{
													parentServer->send_change_string_value_response(objectIdToChange, (1 << static_cast<std::uint8_t>(ChangeStringValueErrorBit::InvalidObjectID)), message.get_source_control_function());
													CANStackLogger::warn("[VT Server]: Client %u change string value command for object %u failed because the object ID was for an object that isn't a string.", cf->get_control_function()->get_address(), objectIdToChange);
												}
												break;
											}
										}
										else
										{
											parentServer->send_change_string_value_response(objectIdToChange, (1 << static_cast<std::uint8_t>(ChangeStringValueErrorBit::InvalidObjectID)), message.get_source_control_function());
											CANStackLogger::warn("[VT Server]: Client %u change string value command for object %u failed because the object ID was invalid.", cf->get_control_function()->get_address(), objectIdToChange);
										}
									}
									else
									{
										parentServer->send_change_string_value_response(objectIdToChange, (1 << static_cast<std::uint8_t>(ChangeStringValueErrorBit::AnyOtherError)), message.get_source_control_function());
										CANStackLogger::warn("[VT Server]: Client %u change string value command for object %u failed because data length is not valid when compared to the amount sent.", cf->get_control_function()->get_address(), objectIdToChange);
									}
								}
								break;

								case Function::ChangeFillAttributesCommand:
								{
									std::uint16_t objectIdToChange = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									std::uint16_t fillPatternID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[5]) | (static_cast<std::uint16_t>(data[6]) << 8));
									auto object = cf->get_object_by_id(objectIdToChange);
									auto fillPatternObject = cf->get_object_by_id(fillPatternID);

									if ((nullptr != object) && (VirtualTerminalObjectType::FillAttributes == object->get_object_type()))
									{
										auto fillObject = std::static_pointer_cast<FillAttributes>(object);

										if (((nullptr != fillPatternObject) && (VirtualTerminalObjectType::PictureGraphic == fillPatternObject->get_object_type())) || (NULL_OBJECT_ID == fillPatternID))
										{
											if (data[3] <= static_cast<std::uint8_t>(FillAttributes::FillType::FillWithPatternGivenByFillPatternAttribute))
											{
												fillObject->set_fill_pattern(fillPatternID);
												fillObject->set_type(static_cast<FillAttributes::FillType>(data[3]));
												fillObject->set_background_color(data[4]);
												parentServer->send_change_fill_attributes_response(objectIdToChange, 0, message.get_source_control_function());
												parentServer->onRepaintEventDispatcher.call(cf);
												CANStackLogger::debug("[VT Server]: Client %u change fill attributes command for object %u", cf->get_control_function()->get_address(), objectIdToChange);
											}
											else
											{
												parentServer->send_change_fill_attributes_response(objectIdToChange, (1 << static_cast<std::uint8_t>(ChangeFillAttributesErrorBit::InvalidType)), message.get_source_control_function());
												CANStackLogger::warn("[VT Server]: Client %u change fill attributes of object %u invalid fill object type. Must be a picture graphic.", cf->get_control_function()->get_address(), objectIdToChange);
											}
										}
										else
										{
											parentServer->send_change_fill_attributes_response(objectIdToChange, (1 << static_cast<std::uint8_t>(ChangeFillAttributesErrorBit::InvalidPatternObjectID)), message.get_source_control_function());
											CANStackLogger::warn("[VT Server]: Client %u change fill attributes invalid pattern object ID of %u for object %u", cf->get_control_function()->get_address(), fillPatternID, objectIdToChange);
										}
									}
									else
									{
										parentServer->send_change_fill_attributes_response(objectIdToChange, (1 << static_cast<std::uint8_t>(ChangeFillAttributesErrorBit::InvalidObjectID)), message.get_source_control_function());
										CANStackLogger::warn("[VT Server]: Client %u change fill attributes invalid object ID of %u", cf->get_control_function()->get_address(), objectIdToChange);
									}
								}
								break;

								case Function::ChangeChildPositionCommand:
								{
									auto parentObjectId = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto objectID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[3]) | (static_cast<std::uint16_t>(data[4]) << 8));
									if (message.get_data_length() > CAN_DATA_LENGTH) // Must be at least 9 bytes
									{
										std::uint16_t newXPosition = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[5]) | (static_cast<std::uint16_t>(data[6]) << 8));
										std::uint16_t newYPosition = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[7]) | (static_cast<std::uint16_t>(data[8]) << 8));
										auto parentObject = cf->get_object_by_id(parentObjectId);
										auto targetObject = cf->get_object_by_id(objectID);

										if (nullptr != parentObject)
										{
											if (nullptr != targetObject)
											{
												switch (parentObject->get_object_type())
												{
													case VirtualTerminalObjectType::Button:
													case VirtualTerminalObjectType::Container:
													case VirtualTerminalObjectType::AlarmMask:
													case VirtualTerminalObjectType::DataMask:
													case VirtualTerminalObjectType::Key:
													case VirtualTerminalObjectType::WorkingSet:
													case VirtualTerminalObjectType::AuxiliaryInputType2:
													case VirtualTerminalObjectType::WindowMask:
													{
														bool wasFound = false;

														// If a parent object includes the child object multiple times, then each instance will be moved
														for (std::uint16_t i = 0; i < parentObject->get_number_children(); i++)
														{
															if (objectID == parentObject->get_child_id(i))
															{
																wasFound = true;
																parentObject->set_child_x(i, newXPosition);
																parentObject->set_child_y(i, newYPosition);
																parentServer->onChangeChildPositionEventDispatcher.call(cf, parentObjectId, objectID, newXPosition, newYPosition);
															}
														}

														if (wasFound)
														{
															CANStackLogger::debug("[VT Server]: Client %u changed child position: object %u of parent object %u, x: %u, y: %u", cf->get_control_function()->get_address(), objectID, parentObjectId, newXPosition, newYPosition);
															parentServer->send_change_child_position_response(parentObjectId, objectID, 0, message.get_source_control_function());
														}
														else
														{
															CANStackLogger::warn("[VT Server]: Client %u change child position error. Target object does not exist or is not applicable: object %u of parent object %u, x: %u, y: %u", cf->get_control_function()->get_address(), objectID, parentObjectId, newXPosition, newYPosition);
															parentServer->send_change_child_position_response(parentObjectId, objectID, (1 << static_cast<std::uint8_t>(ChangeChildLocationorPositionErrorBit::TargetObjectDoesNotExistOrIsNotApplicable)), message.get_source_control_function());
														}
													}
													break;

													default:
													{
														CANStackLogger::warn("[VT Server]: Client %u change child position error. Parent object type cannot be targeted by this command: object %u of parent object %u, x: %u, y: %u", cf->get_control_function()->get_address(), objectID, parentObjectId, newXPosition, newYPosition);
														parentServer->send_change_child_position_response(parentObjectId, objectID, (1 << static_cast<std::uint8_t>(ChangeChildLocationorPositionErrorBit::AnyOtherError)), message.get_source_control_function());
													}
													break;
												}
											}
											else
											{
												CANStackLogger::warn("[VT Server]: Client %u change child position error. Target object does not exist or is not applicable: object %u of parent object %u, x: %u, y: %u", cf->get_control_function()->get_address(), objectID, parentObjectId, newXPosition, newYPosition);
												parentServer->send_change_child_position_response(parentObjectId, objectID, (1 << static_cast<std::uint8_t>(ChangeChildLocationorPositionErrorBit::TargetObjectDoesNotExistOrIsNotApplicable)), message.get_source_control_function());
											}
										}
										else
										{
											CANStackLogger::warn("[VT Server]: Client %u change child position error. Parent object does not exist or is not applicable: object %u of parent object %u, x: %u, y: %u", cf->get_control_function()->get_address(), objectID, parentObjectId, newXPosition, newYPosition);
											parentServer->send_change_child_position_response(parentObjectId, objectID, (1 << static_cast<std::uint8_t>(ChangeChildLocationorPositionErrorBit::ParentObjectDoesntExistOrIsNotAParentOfSpecifiedObject)), message.get_source_control_function());
										}
									}
									else
									{
										CANStackLogger::warn("[VT Server]: Client %u change child position error. DLC must be 9 bytes for the message to be valid.");
										parentServer->send_change_child_position_response(parentObjectId, objectID, (1 << static_cast<std::uint8_t>(ChangeChildLocationorPositionErrorBit::AnyOtherError)), message.get_source_control_function());
									}
								}
								break;

								case Function::ChangeAttributeCommand:
								{
									auto objectID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto targetObject = cf->get_object_by_id(objectID);
									std::uint8_t attributeID = data[3];
									std::uint32_t attributeData = static_cast<std::uint32_t>(static_cast<std::uint32_t>(data[4]) | (static_cast<std::uint32_t>(data[5]) << 8) | (static_cast<std::uint32_t>(data[6]) << 16) | (static_cast<std::uint32_t>(data[7]) << 24));
									VTObject::AttributeError errorCode = VTObject::AttributeError::AnyOtherError;

									if ((NULL_OBJECT_ID != objectID) && (nullptr != targetObject))
									{
										if (targetObject->set_attribute(attributeID, attributeData, errorCode)) // 0 Is always the read-only "type" attribute
										{
											parentServer->send_change_attribute_response(objectID, 0, data.at(3), message.get_source_control_function());
											CANStackLogger::debug("[VT Server]: Client %u changed object %u attribute %u to %u", cf->get_control_function()->get_address(), objectID, attributeID, attributeData);
											parentServer->onRepaintEventDispatcher.call(cf);
										}
										else
										{
											parentServer->send_change_attribute_response(objectID, (1 << static_cast<std::uint8_t>(errorCode)), data.at(3), message.get_source_control_function());
											CANStackLogger::warn("[VT Server]: Client %u change object %u attribute %u to %ul error %u", cf->get_control_function()->get_address(), objectID, attributeID, attributeData, static_cast<std::uint8_t>(errorCode));
										}
									}
									else
									{
										parentServer->send_change_attribute_response(objectID, (1 << static_cast<std::uint8_t>(VTObject::AttributeError::InvalidObjectID)), data.at(3), message.get_source_control_function());
										CANStackLogger::warn("[VT Server]: Client %u change attribute %u invalid object ID of %u", cf->get_control_function()->get_address(), attributeID, objectID);
									}
								}
								break;

								case Function::ChangeSizeCommand:
								{
									auto objectID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto newWidth = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[3]) | (static_cast<std::uint16_t>(data[4]) << 8));
									auto newHeight = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[5]) | (static_cast<std::uint16_t>(data[6]) << 8));
									auto targetObject = cf->get_object_by_id(objectID);

									if (nullptr != targetObject)
									{
										bool success = false;

										switch (targetObject->get_object_type())
										{
											case VirtualTerminalObjectType::OutputMeter:
											{
												if (newWidth == newHeight) // Output meter must be square!
												{
													targetObject->set_width(newWidth);
													targetObject->set_height(newHeight);
													success = true;
													CANStackLogger::debug("[VT Server]: Client %u change size command: Object: %u, Width: %u, Height: %u", cf->get_control_function()->get_address(), objectID, newWidth, newHeight);
													parentServer->onRepaintEventDispatcher.call(cf);
												}
												else
												{
													CANStackLogger::warn("[VT Server]: Client %u change size command: invalid new size. Meter must be square! Object: %u", cf->get_control_function()->get_address(), objectID);
													parentServer->send_change_size_response(objectID, (1 << (static_cast<std::uint8_t>(ChangeSizeErrorBit::AnyOtherError))), message.get_source_control_function());
												}
											}
											break;

											case VirtualTerminalObjectType::Animation:
											case VirtualTerminalObjectType::OutputArchedBarGraph:
											case VirtualTerminalObjectType::OutputPolygon:
											case VirtualTerminalObjectType::OutputEllipse:
											case VirtualTerminalObjectType::OutputRectangle:
											case VirtualTerminalObjectType::OutputLine:
											case VirtualTerminalObjectType::OutputNumber:
											case VirtualTerminalObjectType::OutputList:
											case VirtualTerminalObjectType::InputList:
											case VirtualTerminalObjectType::Button:
											case VirtualTerminalObjectType::Container:
											{
												targetObject->set_width(newWidth);
												targetObject->set_height(newHeight);
												success = true;
												CANStackLogger::debug("[VT Server]: Client %u change size command: Object: %u, Width: %u, Height: %u", cf->get_control_function()->get_address(), objectID, newWidth, newHeight);
												parentServer->onRepaintEventDispatcher.call(cf);
											}
											break;

											default:
											{
												CANStackLogger::warn("[VT Server]: Client %u change size command: invalid object type for object %u", cf->get_control_function()->get_address(), objectID);
												parentServer->send_change_size_response(objectID, (1 << (static_cast<std::uint8_t>(ChangeSizeErrorBit::AnyOtherError))), message.get_source_control_function());
											}
											break;
										}

										if (success)
										{
											parentServer->send_change_size_response(objectID, 0, message.get_source_control_function());
										}
									}
									else
									{
										CANStackLogger::warn("[VT Server]: Client %u change size command: invalid object ID of %u", cf->get_control_function()->get_address(), objectID);
										parentServer->send_change_size_response(objectID, (1 << (static_cast<std::uint8_t>(ChangeSizeErrorBit::InvalidObjectID))), message.get_source_control_function());
									}
								}
								break;

								case Function::ChangeListItemCommand:
								{
									auto objectID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto newObjectID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[4]) | (static_cast<std::uint16_t>(data[5]) << 8));
									auto listIndex = data[3];
									auto targetObject = cf->get_object_by_id(objectID);
									auto newObject = cf->get_object_by_id(newObjectID);

									if (nullptr != targetObject)
									{
										if ((NULL_OBJECT_ID == newObjectID) || (nullptr != newObject))
										{
											switch (targetObject->get_object_type())
											{
												case VirtualTerminalObjectType::InputList:
												{
													if (std::static_pointer_cast<InputList>(targetObject)->change_list_item(listIndex, newObjectID))
													{
														parentServer->send_change_list_item_response(objectID, newObjectID, 0, listIndex, message.get_source_control_function());
														CANStackLogger::debug("[VT Server]: Client %u change list item command: Object ID: %u, New Object ID: %u, Index: %u", cf->get_control_function()->get_address(), objectID, newObjectID, listIndex);
														parentServer->onRepaintEventDispatcher.call(cf);
													}
													else
													{
														parentServer->send_change_list_item_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeListItemErrorBit::AnyOtherError)), listIndex, message.get_source_control_function());
														CANStackLogger::warn("[VT Server]: Client %u change list item command failed. Object ID: %u, New Object ID: %u, Index: %u", cf->get_control_function()->get_address(), objectID, newObjectID, listIndex);
													}
												}
												break;

												case VirtualTerminalObjectType::Animation:
												case VirtualTerminalObjectType::ExternalObjectDefinition:
												{
													// @todo
													parentServer->send_change_list_item_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeListItemErrorBit::AnyOtherError)), listIndex, message.get_source_control_function());
													CANStackLogger::warn("[VT Server]: Client %u change list item command: TODO object type", cf->get_control_function()->get_address());
												}
												break;

												case VirtualTerminalObjectType::OutputList:
												{
													if (std::static_pointer_cast<OutputList>(targetObject)->change_list_item(listIndex, newObjectID))
													{
														parentServer->send_change_list_item_response(objectID, newObjectID, 0, listIndex, message.get_source_control_function());
														CANStackLogger::debug("[VT Server]: Client %u change list item command: Object ID: %u, New Object ID: %u, Index: %u", cf->get_control_function()->get_address(), objectID, newObjectID, listIndex);
														parentServer->onRepaintEventDispatcher.call(cf);
													}
													else
													{
														parentServer->send_change_list_item_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeListItemErrorBit::AnyOtherError)), listIndex, message.get_source_control_function());
														CANStackLogger::warn("[VT Server]: Client %u change list item command failed. Object ID: %u, New Object ID: %u, Index: %u", cf->get_control_function()->get_address(), objectID, newObjectID, listIndex);
													}
												}
												break;

												default:
												{
													CANStackLogger::warn("[VT Server]: Client %u change list item command: invalid object type. Object: %u", cf->get_control_function()->get_address(), objectID);
													parentServer->send_change_list_item_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeListItemErrorBit::AnyOtherError)), listIndex, message.get_source_control_function());
												}
												break;
											}
										}
										else
										{
											CANStackLogger::warn("[VT Server]: Client %u change list item command: invalid new object ID of %u", cf->get_control_function()->get_address(), newObjectID);
											parentServer->send_change_list_item_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeListItemErrorBit::InvalidNewListItemObjectID)), listIndex, message.get_source_control_function());
										}
									}
									else
									{
										CANStackLogger::warn("[VT Server]: Client %u change list item command: invalid object ID of %u", cf->get_control_function()->get_address(), objectID);
										parentServer->send_change_list_item_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeListItemErrorBit::InvalidObjectID)), listIndex, message.get_source_control_function());
									}
								}
								break;

								case Function::ChangeFontAttributesCommand:
								{
									auto objectID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto targetObject = cf->get_object_by_id(objectID);
									std::uint8_t fontColour = data[3];
									std::uint8_t fontSize = data[4];
									std::uint8_t fontType = data[5];
									std::uint8_t fontStyle = data[6];

									if ((nullptr != targetObject) &&
									    (VirtualTerminalObjectType::FontAttributes == targetObject->get_object_type()))
									{
										if (fontSize <= static_cast<std::uint8_t>(FontAttributes::FontSize::Size128x192))
										{
											auto font = std::static_pointer_cast<FontAttributes>(targetObject);
											font->set_background_color(fontColour);
											font->set_size(static_cast<FontAttributes::FontSize>(fontSize));
											font->set_type(static_cast<FontAttributes::FontType>(fontType));
											font->set_style(fontStyle);
											CANStackLogger::debug("[VT Server]: Client %u change font attributes command: ObjectID: %u", cf->get_control_function()->get_address(), fontSize, objectID);
											parentServer->send_change_font_attributes_response(objectID, 0, message.get_source_control_function());
											parentServer->onRepaintEventDispatcher.call(cf);
										}
										else
										{
											CANStackLogger::warn("[VT Server]: Client %u change font attributes command: invalid font size %u. ObjectID: %u", cf->get_control_function()->get_address(), fontSize, objectID);
											parentServer->send_change_font_attributes_response(objectID, (1 << static_cast<std::uint8_t>(ChangeFontAttributesErrorBit::InvalidSize)), message.get_source_control_function());
										}
									}
									else
									{
										CANStackLogger::warn("[VT Server]: Client %u change font attributes command: invalid object ID of %u", cf->get_control_function()->get_address(), objectID);
										parentServer->send_change_font_attributes_response(objectID, (1 << static_cast<std::uint8_t>(ChangeFontAttributesErrorBit::InvalidObjectID)), message.get_source_control_function());
									}
								}
								break;

								case Function::ChangeSoftKeyMaskCommand:
								{
									auto objectID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) | (static_cast<std::uint16_t>(data[2]) << 8));
									auto newObjectID = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[3]) | (static_cast<std::uint16_t>(data[4]) << 8));
									auto targetObject = cf->get_object_by_id(objectID);
									auto newObject = cf->get_object_by_id(newObjectID);

									if (nullptr != targetObject)
									{
										if ((NULL_OBJECT_ID == newObjectID) || (nullptr != newObject))
										{
											switch (targetObject->get_object_type())
											{
												case VirtualTerminalObjectType::AlarmMask:
												{
													if (std::static_pointer_cast<AlarmMask>(targetObject)->change_soft_key_mask(newObjectID))
													{
														CANStackLogger::debug("[VT Server]: Client %u change soft key mask command: alarm mask object %u to %u", cf->get_control_function()->get_address(), objectID, newObjectID);
														parentServer->send_change_soft_key_mask_response(objectID, newObjectID, 0, message.get_source_control_function());
													}
													else
													{
														CANStackLogger::warn("[VT Server]: Client %u change soft key mask command: failed to set mask for alarm mask object %u to %u", cf->get_control_function()->get_address(), objectID, newObjectID);
														parentServer->send_change_soft_key_mask_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeSoftKeyMaskErrorBit::AnyOtherError)), message.get_source_control_function());
													}
												}
												break;

												case VirtualTerminalObjectType::DataMask:
												{
													if (std::static_pointer_cast<DataMask>(targetObject)->change_soft_key_mask(newObjectID))
													{
														CANStackLogger::debug("[VT Server]: Client %u change soft key mask command: data mask object %u to %u", cf->get_control_function()->get_address(), objectID, newObjectID);
														parentServer->send_change_soft_key_mask_response(objectID, newObjectID, 0, message.get_source_control_function());
													}
													else
													{
														CANStackLogger::warn("[VT Server]: Client %u change soft key mask command: failed to set mask for data mask object %u to %u", cf->get_control_function()->get_address(), objectID, newObjectID);
														parentServer->send_change_soft_key_mask_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeSoftKeyMaskErrorBit::AnyOtherError)), message.get_source_control_function());
													}
												}
												break;

												default:
												{
													CANStackLogger::warn("[VT Server]: Client %u change soft key mask command: invalid object type for object %u", cf->get_control_function()->get_address(), objectID);
													parentServer->send_change_soft_key_mask_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeSoftKeyMaskErrorBit::AnyOtherError)), message.get_source_control_function());
												}
												break;
											}
										}
										else
										{
											CANStackLogger::warn("[VT Server]: Client %u change soft key mask command: invalid soft key object ID of %u", cf->get_control_function()->get_address(), newObjectID);
											parentServer->send_change_soft_key_mask_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeSoftKeyMaskErrorBit::InvalidSoftKeyMaskObjectID)), message.get_source_control_function());
										}
									}
									else
									{
										CANStackLogger::warn("[VT Server]: Client %u change soft key mask command: invalid data mask or alarm mask object ID of %u", cf->get_control_function()->get_address(), objectID);
										parentServer->send_change_soft_key_mask_response(objectID, newObjectID, (1 << static_cast<std::uint8_t>(ChangeSoftKeyMaskErrorBit::InvalidDataOrAlarmMaskObjectID)), message.get_source_control_function());
									}
								}
								break;

								default:
								{
									CANStackLogger::warn("[VT Server]: Unimplemented Command!");
								}
								break;
							}
						}
						break;

						default:
							break;
					}
					break;
				}
			}
		}
	}

	bool VirtualTerminalServer::send_acknowledgement(AcknowledgementType type, std::uint32_t parameterGroupNumber, std::shared_ptr<InternalControlFunction> source, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if ((nullptr != source) && (nullptr != destination))
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer;

			buffer[0] = static_cast<std::uint8_t>(type);
			buffer[1] = 0xFF;
			buffer[2] = 0xFF;
			buffer[3] = 0xFF;
			buffer[4] = destination->get_address();
			buffer[5] = static_cast<std::uint8_t>(parameterGroupNumber & 0xFF);
			buffer[6] = static_cast<std::uint8_t>((parameterGroupNumber >> 8) & 0xFF);
			buffer[7] = static_cast<std::uint8_t>((parameterGroupNumber >> 16) & 0xFF);

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::Acknowledge),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        source,
			                                                        nullptr,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_active_mask_response(std::uint16_t newMaskObjectID, std::uint8_t errorBitfield, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = {
				static_cast<std::uint8_t>(Function::ChangeActiveMaskCommand),
				static_cast<std::uint8_t>(newMaskObjectID & 0xFF),
				static_cast<std::uint8_t>((newMaskObjectID >> 8) & 0xFF),
				errorBitfield,
				0xFF,
				0xFF,
				0xFF,
				0xFF
			};

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_attribute_response(std::uint16_t objectID, std::uint8_t errorBitfield, std::uint8_t attributeID, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = {
				static_cast<std::uint8_t>(Function::ChangeAttributeCommand),
				static_cast<std::uint8_t>(objectID & 0xFF),
				static_cast<std::uint8_t>((objectID >> 8) & 0xFF),
				attributeID,
				errorBitfield,
				0xFF,
				0xFF,
				0xFF
			};

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_child_location_response(std::uint16_t parentObjectID, std::uint16_t objectID, std::uint8_t errorBitfield, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer;

			buffer[0] = static_cast<std::uint8_t>(Function::ChangeChildLocationCommand);
			buffer[1] = static_cast<std::uint8_t>(parentObjectID & 0xFF);
			buffer[2] = static_cast<std::uint8_t>(parentObjectID >> 8);
			buffer[3] = static_cast<std::uint8_t>(objectID & 0xFF);
			buffer[4] = static_cast<std::uint8_t>(objectID >> 8);
			buffer[5] = errorBitfield;
			buffer[6] = 0xFF;
			buffer[7] = 0xFF;

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_child_position_response(std::uint16_t parentObjectID, std::uint16_t objectID, std::uint8_t errorBitfield, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer{
				buffer[0] = static_cast<std::uint8_t>(Function::ChangeChildPositionCommand),
				buffer[1] = static_cast<std::uint8_t>(parentObjectID & 0xFF),
				buffer[2] = static_cast<std::uint8_t>(parentObjectID >> 8),
				buffer[3] = static_cast<std::uint8_t>(objectID & 0xFF),
				buffer[4] = static_cast<std::uint8_t>(objectID >> 8),
				buffer[5] = errorBitfield,
				buffer[6] = 0xFF,
				buffer[7] = 0xFF
			};

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_fill_attributes_response(std::uint16_t objectID, std::uint8_t errorBitfield, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = {
				static_cast<std::uint8_t>(Function::ChangeFillAttributesCommand),
				static_cast<std::uint8_t>(objectID & 0xFF),
				static_cast<std::uint8_t>(objectID >> 8),
				errorBitfield,
				0xFF,
				0xFF,
				0xFF,
				0xFF
			};
			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_font_attributes_response(std::uint16_t objectID, std::uint8_t errorBitfield, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = {
				static_cast<std::uint8_t>(Function::ChangeFontAttributesCommand),
				static_cast<std::uint8_t>(objectID & 0xFF),
				static_cast<std::uint8_t>(objectID >> 8),
				errorBitfield,
				0xFF,
				0xFF,
				0xFF,
				0xFF
			};
			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_list_item_response(std::uint16_t objectID, std::uint16_t newObjectID, std::uint8_t errorBitfield, std::uint8_t listIndex, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = {
				static_cast<std::uint8_t>(Function::ChangeListItemCommand),
				static_cast<std::uint8_t>(objectID & 0xFF),
				static_cast<std::uint8_t>(objectID >> 8),
				listIndex,
				static_cast<std::uint8_t>(newObjectID & 0xFF),
				static_cast<std::uint8_t>(newObjectID >> 8),
				errorBitfield,
				0xFF
			};
			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_button_activation_message(KeyActivationCode activationCode, std::uint16_t objectId, std::uint16_t parentObjectId, std::uint8_t keyNumber, std::shared_ptr<ControlFunction> destination) const
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer;

			buffer[0] = static_cast<std::uint8_t>(Function::ButtonActivationMessage);
			buffer[1] = static_cast<std::uint8_t>(activationCode);
			buffer[2] = static_cast<std::uint8_t>(objectId & 0xFF);
			buffer[3] = static_cast<std::uint8_t>(objectId >> 8);
			buffer[4] = static_cast<std::uint8_t>(parentObjectId & 0xFF);
			buffer[5] = static_cast<std::uint8_t>(parentObjectId >> 8);
			buffer[6] = keyNumber;
			buffer[7] = 0xFF; // Reserved TODO: TAN

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_numeric_value_message(std::uint16_t objectId, std::uint32_t value, std::shared_ptr<ControlFunction> destination) const
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = {

				static_cast<std::uint8_t>(Function::VTChangeNumericValueMessage),
				static_cast<std::uint8_t>(objectId & 0xFF),
				static_cast<std::uint8_t>((objectId >> 8) & 0xFF),
				0xFF, // TODO: TAN, version 6
				static_cast<std::uint8_t>(value & 0xFF),
				static_cast<std::uint8_t>((value >> 8) & 0xFF),
				static_cast<std::uint8_t>((value >> 16) & 0xFF),
				static_cast<std::uint8_t>((value >> 24) & 0xFF)
			};

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_select_input_object_message(std::uint16_t objectId, bool isObjectSelected, bool isObjectOpenForInput, std::shared_ptr<ControlFunction> destination) const
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = {

				static_cast<std::uint8_t>(Function::VTSelectInputObjectMessage),
				static_cast<std::uint8_t>(objectId & 0xFF),
				static_cast<std::uint8_t>((objectId >> 8) & 0xFF),
				static_cast<std::uint8_t>(isObjectSelected),
				static_cast<std::uint8_t>(isObjectOpenForInput),
				0xFF,
				0xFF,
				0xFF // Reserved TODO: TAN
			};

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_soft_key_activation_message(KeyActivationCode activationCode, std::uint16_t objectId, std::uint16_t parentObjectId, std::uint8_t keyNumber, std::shared_ptr<ControlFunction> destination) const
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = {

				static_cast<std::uint8_t>(Function::SoftKeyActivationMessage),
				static_cast<std::uint8_t>(activationCode),
				static_cast<std::uint8_t>(objectId & 0xFF),
				static_cast<std::uint8_t>(objectId >> 8),
				static_cast<std::uint8_t>(parentObjectId & 0xFF),
				static_cast<std::uint8_t>(parentObjectId >> 8),
				keyNumber,
				0xFF // Reserved TODO: TAN
			};

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_numeric_value_response(std::uint16_t objectID, std::uint8_t errorBitfield, std::uint32_t value, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer;

			buffer[0] = static_cast<std::uint8_t>(Function::ChangeNumericValueCommand);
			buffer[1] = static_cast<std::uint8_t>(objectID & 0xFF);
			buffer[2] = static_cast<std::uint8_t>(objectID >> 8);
			buffer[3] = errorBitfield;
			buffer[4] = static_cast<std::uint8_t>(value & 0xFF);
			buffer[5] = static_cast<std::uint8_t>(value >> 8);
			buffer[6] = static_cast<std::uint8_t>(value >> 16);
			buffer[7] = static_cast<std::uint8_t>(value >> 24);

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_size_response(std::uint16_t objectID, std::uint8_t errorBitfield, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer{
				static_cast<std::uint8_t>(Function::ChangeSizeCommand),
				static_cast<std::uint8_t>(objectID & 0xFF),
				static_cast<std::uint8_t>(objectID >> 8),
				errorBitfield,
				0xFF,
				0xFF,
				0xFF,
				0xFF
			};

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_soft_key_mask_response(std::uint16_t objectID, std::uint16_t newObjectID, std::uint8_t errorBitfield, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer{
				static_cast<std::uint8_t>(Function::ChangeSoftKeyMaskCommand),
				static_cast<std::uint8_t>(objectID & 0xFF),
				static_cast<std::uint8_t>(objectID >> 8),
				static_cast<std::uint8_t>(newObjectID & 0xFF),
				static_cast<std::uint8_t>(newObjectID >> 8),
				errorBitfield,
				0xFF,
				0xFF
			};

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_change_string_value_response(std::uint16_t objectID, std::uint8_t errorBitfield, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			const std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = {
				static_cast<std::uint8_t>(Function::ChangeStringValueCommand),
				0xFF,
				0xFF,
				static_cast<std::uint8_t>(objectID & 0xFF),
				static_cast<std::uint8_t>(objectID >> 8),
				errorBitfield,
				0xFF,
				0xFF
			};
			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_enable_disable_object_response(std::uint16_t objectID, std::uint8_t errorBitfield, bool value, std::shared_ptr<ControlFunction> destination)
	{
		bool retVal = false;

		if (nullptr != destination)
		{
			std::array<std::uint8_t, CAN_DATA_LENGTH> buffer;

			buffer[0] = static_cast<std::uint8_t>(Function::EnableDisableObjectCommand);
			buffer[1] = static_cast<std::uint8_t>(objectID & 0xFF);
			buffer[2] = static_cast<std::uint8_t>(objectID >> 8);
			buffer[3] = value;
			buffer[4] = errorBitfield;
			buffer[5] = 0xFF;
			buffer[6] = 0xFF;
			buffer[7] = 0xFF;

			retVal = CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
			                                                        buffer.data(),
			                                                        CAN_DATA_LENGTH,
			                                                        serverInternalControlFunction,
			                                                        destination,
			                                                        CANIdentifier::PriorityLowest7);
		}
		return retVal;
	}

	bool VirtualTerminalServer::send_end_of_object_pool_response(bool success,
	                                                             std::uint16_t parentIDOfFaultingObject,
	                                                             std::uint16_t faultingObjectID,
	                                                             std::uint8_t errorCodes,
	                                                             std::shared_ptr<ControlFunction> destination)
	{
		std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = { 0 };

		buffer[0] = static_cast<std::uint8_t>(Function::EndOfObjectPoolMessage);
		buffer[1] = (success ? 0x00 : 0x01); // Error in object pool is 0x01, no error is 0x00
		buffer[2] = (parentIDOfFaultingObject & 0xFF);
		buffer[3] = (parentIDOfFaultingObject >> 8);
		buffer[4] = (faultingObjectID & 0xFF);
		buffer[5] = (faultingObjectID >> 8);
		buffer[6] = errorCodes;
		buffer[7] = 0xFF; // Reserved

		return CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
		                                                      buffer.data(),
		                                                      CAN_DATA_LENGTH,
		                                                      serverInternalControlFunction,
		                                                      destination,
		                                                      CANIdentifier::PriorityLowest7);
	}

	bool VirtualTerminalServer::send_hide_show_object_response(std::uint16_t objectID, std::uint8_t errorBitfield, bool value, std::shared_ptr<ControlFunction> destination)
	{
		std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = { 0 };

		buffer[0] = static_cast<std::uint8_t>(Function::HideShowObjectCommand);
		buffer[1] = (objectID & 0xFF);
		buffer[2] = ((objectID >> 8) & 0xFF);
		buffer[3] = static_cast<std::uint8_t>(value);
		buffer[4] = errorBitfield;
		buffer[5] = 0xFF; // Reserved
		buffer[6] = 0xFF; // Reserved
		buffer[7] = 0xFF; // Reserved

		return CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
		                                                      buffer.data(),
		                                                      CAN_DATA_LENGTH,
		                                                      serverInternalControlFunction,
		                                                      destination,
		                                                      CANIdentifier::PriorityLowest7);
	}

	bool VirtualTerminalServer::send_status_message()
	{
		std::array<std::uint8_t, CAN_DATA_LENGTH> buffer = { 0 };

		buffer[0] = static_cast<std::uint8_t>(Function::VTStatusMessage);
		buffer[1] = activeWorkingSetMasterAddress;
		buffer[2] = (activeWorkingSetDataMaskObjectID & 0xFF);
		buffer[3] = ((activeWorkingSetDataMaskObjectID >> 8) & 0xFF);
		buffer[4] = (activeWorkingSetSoftkeyMaskObjectID & 0xFF);
		buffer[5] = ((activeWorkingSetSoftkeyMaskObjectID >> 8) & 0xFF);
		buffer[6] = busyCodesBitfield;
		buffer[7] = currentCommandFunctionCode;
		return CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
		                                                      buffer.data(),
		                                                      CAN_DATA_LENGTH,
		                                                      serverInternalControlFunction,
		                                                      nullptr,
		                                                      CANIdentifier::PriorityLowest7);
	}

	bool VirtualTerminalServer::send_supported_objects(std::shared_ptr<ControlFunction> destination) const
	{
		auto supportedObjects = get_supported_objects();
		std::vector<std::uint8_t> buffer = { static_cast<std::uint8_t>(Function::GetSupportedObjectsMessage),
			                                   static_cast<std::uint8_t>(supportedObjects.size()) };

		for (const auto &supportedObject : supportedObjects)
		{
			buffer.push_back(supportedObject);
		}
		return CANNetworkManager::CANNetwork.send_can_message(static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU),
		                                                      buffer.data(),
		                                                      CAN_DATA_LENGTH,
		                                                      serverInternalControlFunction,
		                                                      destination,
		                                                      CANIdentifier::PriorityLowest7);
	}

	void VirtualTerminalServer::update()
	{
		if ((isobus::SystemTiming::time_expired_ms(statusMessageTimestamp_ms, 1000)) &&
		    (send_status_message()))
		{
			statusMessageTimestamp_ms = isobus::SystemTiming::get_timestamp_ms();
		}

		for (auto &ws : managedWorkingSetList)
		{
			if (VirtualTerminalServerManagedWorkingSet::ObjectPoolProcessingThreadState::Success == ws->get_object_pool_processing_state())
			{
				ws->join_parsing_thread();
				send_end_of_object_pool_response(true, NULL_OBJECT_ID, NULL_OBJECT_ID, 0, ws->get_control_function());
				if (isobus::NULL_CAN_ADDRESS == activeWorkingSetMasterAddress)
				{
					activeWorkingSetMasterAddress = ws->get_control_function()->get_address();
					activeWorkingSetDataMaskObjectID = std::static_pointer_cast<WorkingSet>(ws->get_working_set_object())->get_active_mask();
				}
			}
			else if (VirtualTerminalServerManagedWorkingSet::ObjectPoolProcessingThreadState::Fail == ws->get_object_pool_processing_state())
			{
				ws->join_parsing_thread();
				///  @todo Get the parent object ID of the faulting object
				send_end_of_object_pool_response(true, NULL_OBJECT_ID, ws->get_object_pool_faulting_object_id(), 0, ws->get_control_function());
			}
		}
	}
}

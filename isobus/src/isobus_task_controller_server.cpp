//================================================================================================
/// @file isobus_task_controller_server.cpp
///
/// @brief Implements portions of an abstract task controller server class.
/// You can consume this file and implement the pure virtual functions to create your own
/// task controller or data logger server.
/// @author Adrian Del Grosso
///
/// @copyright 2023 Adrian Del Grosso
//================================================================================================
#include "isobus/isobus/isobus_task_controller_server.hpp"

namespace isobus
{
	TaskControllerServer::TaskControllerServer(std::shared_ptr<InternalControlFunction> internalControlFunction,
	                                           std::uint8_t numberBoomsSupported,
	                                           std::uint8_t numberSectionsSupported,
	                                           std::uint8_t numberChannelsSupportedForPositionBasedControl,
	                                           std::uint8_t optionsBitfield) :
	  languageCommandInterface(internalControlFunction, true),
	  numberBoomsSupportedToReport(numberBoomsSupported),
	  numberSectionsSupportedToReport(numberSectionsSupported),
	  numberChannelsSupportedForPositionBasedControlToReport(numberChannelsSupportedForPositionBasedControl),
	  optionsBitfieldToReport(optionsBitfield)
	{
	}

	LanguageCommandInterface &TaskControllerServer::get_language_command_interface()
	{
		return languageCommandInterface;
	}
}

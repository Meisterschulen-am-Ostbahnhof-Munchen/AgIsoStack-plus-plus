//================================================================================================
/// @file isobus_virtual_terminal_objects.cpp
///
/// @brief Implements VT server object pool objects.
/// @author Adrian Del Grosso
///
/// @copyright 2023 Adrian Del Grosso
//================================================================================================
#include "isobus/isobus/isobus_virtual_terminal_objects.hpp"

namespace isobus
{
	VTColourTable::VTColourTable()
	{
		// The table can be altered at runtime. Init here to VT standard
		colourTable[0] = VTColourVector(0.0f, 0.0f, 0.0f); // Black
		colourTable[1] = VTColourVector(1.0f, 1.0f, 1.0f); // White
		colourTable[2] = VTColourVector(0.0f, (153.0f / 255.0f), 0.0f); // Green
		colourTable[3] = VTColourVector(0.0f, (153.0f / 255.0f), (153.0f / 255.0f)); // Teal
		colourTable[4] = VTColourVector((153.0f / 255.0f), 0.0f, 0.0f); // Maroon
		colourTable[5] = VTColourVector((153.0f / 255.0f), 0.0f, (153.0f / 255.0f)); // Purple
		colourTable[6] = VTColourVector((153.0f / 255.0f), (153.0f / 255.0f), 0.0f); // Olive
		colourTable[7] = VTColourVector((204.0f / 255.0f), (204.0f / 255.0f), (204.0f / 255.0f)); // Silver
		colourTable[8] = VTColourVector((153.0f / 255.0f), (153.0f / 255.0f), (153.0f / 255.0f)); // Grey
		colourTable[9] = VTColourVector(0.0f, 0.0f, 1.0f); // Blue
		colourTable[10] = VTColourVector(0.0f, 1.0f, 0.0f); // Lime
		colourTable[11] = VTColourVector(0.0f, 1.0f, 1.0f); // Cyan
		colourTable[12] = VTColourVector(1.0f, 0.0f, 0.0f); // Red
		colourTable[13] = VTColourVector(1.0f, 0.0f, 1.0f); // Magenta
		colourTable[14] = VTColourVector(1.0f, 1.0f, 0.0f); // Yellow
		colourTable[15] = VTColourVector(0.0f, 0.0f, (153.0f / 255.0f)); // Navy

		// This section of the table increases with a pattern
		for (std::uint8_t i = 16; i <= 231; i++)
		{
			std::uint8_t index = i - 16;

			std::uint32_t redCounter = (index / 36);
			std::uint32_t greenCounter = ((index / 6) % 6);
			std::uint32_t blueCounter = (index % 6);

			colourTable[i] = VTColourVector((51.0f * (redCounter) / 255.0f), ((51.0f * (greenCounter)) / 255.0f), ((51.0f * blueCounter) / 255.0f));
		}

		// The rest are proprietary. Init to white for now.
		for (std::uint16_t i = 232; i < VT_COLOUR_TABLE_SIZE; i++)
		{
			colourTable[i] = VTColourVector(1.0f, 1.0f, 1.0f);
		}
	}

	VTColourVector VTColourTable::get_colour(std::uint8_t colorIndex) const
	{
		return colourTable.at(colorIndex);
	}

	void VTColourTable::set_colour(std::uint8_t colorIndex, VTColourVector newColour)
	{
		colourTable.at(colorIndex) = newColour;
	}

	VTObject::VTObject(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  colourTable(currentColourTable),
	  thisObjectPool(memberObjectPool)
	{
	}

	std::uint16_t VTObject::get_id() const
	{
		return objectID;
	}

	void VTObject::set_id(std::uint16_t value)
	{
		objectID = value;
	}

	std::uint16_t VTObject::get_width() const
	{
		return width;
	}

	void VTObject::set_width(std::uint16_t value)
	{
		width = value;
	}

	std::uint16_t VTObject::get_height() const
	{
		return height;
	}

	void VTObject::set_height(std::uint16_t value)
	{
		height = value;
	}

	std::uint8_t VTObject::get_background_color() const
	{
		return backgroundColor;
	}

	void VTObject::set_background_color(std::uint8_t value)
	{
		backgroundColor = value;
	}

	std::shared_ptr<VTObject> VTObject::get_object_by_id(std::uint16_t objectID) const
	{
		return thisObjectPool[objectID];
	}

	std::uint16_t VTObject::get_number_children() const
	{
		return static_cast<std::uint16_t>(children.size());
	}

	void VTObject::add_child(std::uint16_t objectID, std::int16_t relativeXLocation, std::int16_t relativeYLocation)
	{
		children.push_back(ChildObjectData(objectID, relativeXLocation, relativeYLocation));
	}

	std::uint16_t VTObject::get_child_id(std::uint16_t index) const
	{
		std::uint16_t retVal = NULL_OBJECT_ID;

		if (index < children.size())
		{
			retVal = children[index].id;
		}
		return retVal;
	}

	std::int16_t VTObject::get_child_x(std::uint16_t index) const
	{
		std::int16_t retVal = 0;

		if (index < children.size())
		{
			retVal = children[index].xLocation;
		}
		return retVal;
	}

	std::int16_t VTObject::get_child_y(std::uint16_t index) const
	{
		std::int16_t retVal = 0;

		if (index < children.size())
		{
			retVal = children[index].yLocation;
		}
		return retVal;
	}

	void VTObject::set_child_x(std::uint16_t index, std::int16_t xOffset)
	{
		if (index < children.size())
		{
			children.at(index).xLocation = xOffset;
		}
	}

	void VTObject::set_child_y(std::uint16_t index, std::int16_t yOffset)
	{
		if (index < children.size())
		{
			children.at(index).yLocation = yOffset;
		}
	}

	bool VTObject::offset_all_children_x_with_id(std::uint16_t childObjectID, std::int8_t xOffset, std::int8_t yOffset)
	{
		bool retVal = false;

		for (auto &child : children)
		{
			if (child.id == childObjectID)
			{
				child.xLocation += xOffset;
				child.yLocation += yOffset;
			}
		}
		return retVal;
	}

	void VTObject::remove_child(std::uint16_t objectID, std::int16_t relativeXLocation, std::int16_t relativeYLocation)
	{
		for (auto child = children.begin(); child != children.end(); child++)
		{
			if ((child->id == objectID) && (child->xLocation == relativeXLocation) && (child->yLocation == relativeYLocation))
			{
				children.erase(child);
				break;
			}
		}
	}

	void VTObject::pop_child()
	{
		if (!children.empty())
		{
			children.pop_back();
		}
	}

	VTObject::ChildObjectData::ChildObjectData() :
	  id(NULL_OBJECT_ID),
	  xLocation(0),
	  yLocation(0)
	{
	}

	VTObject::ChildObjectData::ChildObjectData(std::uint16_t objectId,
	                                           std::int16_t x,
	                                           std::int16_t y) :
	  id(objectId),
	  xLocation(x),
	  yLocation(y)
	{
	}

	WorkingSet::WorkingSet(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  activeMask(NULL_OBJECT_ID),
	  selectable(false)
	{
	}

	VirtualTerminalObjectType WorkingSet::get_object_type() const
	{
		return VirtualTerminalObjectType::WorkingSet;
	}

	std::uint32_t WorkingSet::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool WorkingSet::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::OutputList:
					case VirtualTerminalObjectType::Container:
					case VirtualTerminalObjectType::OutputString:
					case VirtualTerminalObjectType::OutputNumber:
					case VirtualTerminalObjectType::OutputLine:
					case VirtualTerminalObjectType::OutputRectangle:
					case VirtualTerminalObjectType::OutputEllipse:
					case VirtualTerminalObjectType::OutputPolygon:
					case VirtualTerminalObjectType::OutputMeter:
					case VirtualTerminalObjectType::OutputLinearBarGraph:
					case VirtualTerminalObjectType::OutputArchedBarGraph:
					case VirtualTerminalObjectType::GraphicsContext:
					case VirtualTerminalObjectType::PictureGraphic:
					case VirtualTerminalObjectType::ObjectPointer:
					{
						// Valid Objects
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	bool WorkingSet::get_selectable() const
	{
		return selectable;
	}

	void WorkingSet::set_selectable(bool value)
	{
		selectable = value;
	}

	std::uint16_t WorkingSet::get_active_mask() const
	{
		return activeMask;
	}

	void WorkingSet::set_active_mask(std::uint16_t value)
	{
		activeMask = value;
	}

	DataMask::DataMask(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable)
	{
	}

	VirtualTerminalObjectType DataMask::get_object_type() const
	{
		return VirtualTerminalObjectType::DataMask;
	}

	std::uint32_t DataMask::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool DataMask::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::WorkingSet:
					case VirtualTerminalObjectType::Button:
					case VirtualTerminalObjectType::InputBoolean:
					case VirtualTerminalObjectType::InputString:
					case VirtualTerminalObjectType::InputNumber:
					case VirtualTerminalObjectType::OutputString:
					case VirtualTerminalObjectType::InputList:
					case VirtualTerminalObjectType::OutputNumber:
					case VirtualTerminalObjectType::OutputList:
					case VirtualTerminalObjectType::OutputLine:
					case VirtualTerminalObjectType::OutputRectangle:
					case VirtualTerminalObjectType::OutputEllipse:
					case VirtualTerminalObjectType::OutputPolygon:
					case VirtualTerminalObjectType::OutputMeter:
					case VirtualTerminalObjectType::OutputLinearBarGraph:
					case VirtualTerminalObjectType::OutputArchedBarGraph:
					case VirtualTerminalObjectType::GraphicsContext:
					case VirtualTerminalObjectType::Animation:
					case VirtualTerminalObjectType::PictureGraphic:
					case VirtualTerminalObjectType::ObjectPointer:
					case VirtualTerminalObjectType::ExternalObjectPointer:
					case VirtualTerminalObjectType::AuxiliaryFunctionType2:
					case VirtualTerminalObjectType::AuxiliaryInputType2:
					case VirtualTerminalObjectType::AuxiliaryControlDesignatorType2:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}

		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	AlarmMask::AlarmMask(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  softKeyMask(0),
	  maskPriority(Priority::Low),
	  signalPriority(AcousticSignal::None)
	{
	}

	VirtualTerminalObjectType AlarmMask::get_object_type() const
	{
		return VirtualTerminalObjectType::AlarmMask;
	}

	std::uint32_t AlarmMask::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool AlarmMask::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::WorkingSet:
					case VirtualTerminalObjectType::Button:
					case VirtualTerminalObjectType::InputBoolean:
					case VirtualTerminalObjectType::InputString:
					case VirtualTerminalObjectType::InputNumber:
					case VirtualTerminalObjectType::OutputString:
					case VirtualTerminalObjectType::InputList:
					case VirtualTerminalObjectType::OutputNumber:
					case VirtualTerminalObjectType::OutputList:
					case VirtualTerminalObjectType::OutputLine:
					case VirtualTerminalObjectType::OutputRectangle:
					case VirtualTerminalObjectType::OutputEllipse:
					case VirtualTerminalObjectType::OutputPolygon:
					case VirtualTerminalObjectType::OutputMeter:
					case VirtualTerminalObjectType::OutputLinearBarGraph:
					case VirtualTerminalObjectType::OutputArchedBarGraph:
					case VirtualTerminalObjectType::GraphicsContext:
					case VirtualTerminalObjectType::Animation:
					case VirtualTerminalObjectType::PictureGraphic:
					case VirtualTerminalObjectType::ObjectPointer:
					case VirtualTerminalObjectType::ExternalObjectPointer:
					case VirtualTerminalObjectType::AuxiliaryFunctionType2:
					case VirtualTerminalObjectType::AuxiliaryInputType2:
					case VirtualTerminalObjectType::AuxiliaryControlDesignatorType2:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	AlarmMask::Priority AlarmMask::get_mask_priority() const
	{
		return maskPriority;
	}

	void AlarmMask::set_mask_priority(Priority value)
	{
		maskPriority = value;
	}

	AlarmMask::AcousticSignal AlarmMask::get_signal_priority() const
	{
		return signalPriority;
	}

	void AlarmMask::set_signal_priority(AcousticSignal value)
	{
		signalPriority = value;
	}

	Container::Container(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  hidden(false)
	{
	}

	VirtualTerminalObjectType Container::get_object_type() const
	{
		return VirtualTerminalObjectType::Container;
	}

	std::uint32_t Container::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool Container::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::WorkingSet:
					case VirtualTerminalObjectType::Container:
					case VirtualTerminalObjectType::Button:
					case VirtualTerminalObjectType::InputBoolean:
					case VirtualTerminalObjectType::InputString:
					case VirtualTerminalObjectType::InputNumber:
					case VirtualTerminalObjectType::InputList:
					case VirtualTerminalObjectType::OutputString:
					case VirtualTerminalObjectType::OutputNumber:
					case VirtualTerminalObjectType::OutputList:
					case VirtualTerminalObjectType::OutputLine:
					case VirtualTerminalObjectType::OutputRectangle:
					case VirtualTerminalObjectType::OutputEllipse:
					case VirtualTerminalObjectType::OutputPolygon:
					case VirtualTerminalObjectType::OutputMeter:
					case VirtualTerminalObjectType::GraphicsContext:
					case VirtualTerminalObjectType::OutputArchedBarGraph:
					case VirtualTerminalObjectType::OutputLinearBarGraph:
					case VirtualTerminalObjectType::Animation:
					case VirtualTerminalObjectType::PictureGraphic:
					case VirtualTerminalObjectType::ObjectPointer:
					case VirtualTerminalObjectType::ExternalObjectPointer:
					case VirtualTerminalObjectType::AuxiliaryFunctionType2:
					case VirtualTerminalObjectType::AuxiliaryInputType2:
					case VirtualTerminalObjectType::AuxiliaryControlDesignatorType2:
					{
						// Valid Child Object
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	bool Container::get_hidden() const
	{
		return hidden;
	}

	void Container::set_hidden(bool value)
	{
		hidden = value;
	}

	SoftKeyMask::SoftKeyMask(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable)
	{
	}

	VirtualTerminalObjectType SoftKeyMask::get_object_type() const
	{
		return VirtualTerminalObjectType::SoftKeyMask;
	}

	std::uint32_t SoftKeyMask::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool SoftKeyMask::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::ObjectPointer:
					case VirtualTerminalObjectType::ExternalObjectPointer:
					case VirtualTerminalObjectType::Key:
					{
						// Valid Child Object
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	Key::Key(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  keyCode(0)
	{
	}

	VirtualTerminalObjectType Key::get_object_type() const
	{
		return VirtualTerminalObjectType::Key;
	}

	std::uint32_t Key::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool Key::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::WorkingSet:
					case VirtualTerminalObjectType::Container:
					case VirtualTerminalObjectType::OutputString:
					case VirtualTerminalObjectType::OutputNumber:
					case VirtualTerminalObjectType::OutputList:
					case VirtualTerminalObjectType::OutputLine:
					case VirtualTerminalObjectType::OutputRectangle:
					case VirtualTerminalObjectType::OutputEllipse:
					case VirtualTerminalObjectType::OutputPolygon:
					case VirtualTerminalObjectType::OutputMeter:
					case VirtualTerminalObjectType::GraphicsContext:
					case VirtualTerminalObjectType::OutputArchedBarGraph:
					case VirtualTerminalObjectType::OutputLinearBarGraph:
					case VirtualTerminalObjectType::Animation:
					case VirtualTerminalObjectType::PictureGraphic:
					case VirtualTerminalObjectType::ObjectPointer:
					case VirtualTerminalObjectType::ExternalObjectPointer:
					{
						// Valid Child Object
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint8_t Key::get_key_code() const
	{
		return keyCode;
	}

	void Key::set_key_code(std::uint8_t value)
	{
		keyCode = value;
	}

	KeyGroup::KeyGroup(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  keyGroupIcon(NULL_OBJECT_ID)
	{
	}

	VirtualTerminalObjectType KeyGroup::get_object_type() const
	{
		return VirtualTerminalObjectType::KeyGroup;
	}

	std::uint32_t KeyGroup::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool KeyGroup::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::Key:
					case VirtualTerminalObjectType::ObjectPointer:
					{
						/// @todo search child object pointers for only keys or NULL ID
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint16_t KeyGroup::get_key_group_icon() const
	{
		return keyGroupIcon;
	}

	void KeyGroup::set_key_group_icon(std::uint16_t value)
	{
		keyGroupIcon = value;
	}

	bool KeyGroup::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & optionsBitfield));
	}

	void KeyGroup::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void KeyGroup::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	Button::Button(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  borderColour(0),
	  keyCode(0),
	  optionsBitfield(0)
	{
	}

	VirtualTerminalObjectType Button::get_object_type() const
	{
		return VirtualTerminalObjectType::Button;
	}

	std::uint32_t Button::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool Button::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::WorkingSet:
					case VirtualTerminalObjectType::OutputList:
					case VirtualTerminalObjectType::Container:
					case VirtualTerminalObjectType::OutputString:
					case VirtualTerminalObjectType::OutputNumber:
					case VirtualTerminalObjectType::OutputLine:
					case VirtualTerminalObjectType::OutputRectangle:
					case VirtualTerminalObjectType::OutputEllipse:
					case VirtualTerminalObjectType::OutputPolygon:
					case VirtualTerminalObjectType::OutputMeter:
					case VirtualTerminalObjectType::OutputLinearBarGraph:
					case VirtualTerminalObjectType::OutputArchedBarGraph:
					case VirtualTerminalObjectType::GraphicsContext:
					case VirtualTerminalObjectType::PictureGraphic:
					case VirtualTerminalObjectType::ObjectPointer:
					case VirtualTerminalObjectType::Animation:
					{
						// Valid Child Object
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint8_t Button::get_key_code() const
	{
		return keyCode;
	}

	void Button::set_key_code(std::uint8_t value)
	{
		keyCode = value;
	}

	std::uint8_t Button::get_border_colour() const
	{
		return borderColour;
	}

	void Button::set_border_colour(std::uint8_t value)
	{
		borderColour = value;
	}

	bool Button::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & optionsBitfield));
	}

	void Button::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void Button::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	InputBoolean::InputBoolean(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  value(0),
	  enabled(false)
	{
	}

	VirtualTerminalObjectType InputBoolean::get_object_type() const
	{
		return VirtualTerminalObjectType::InputBoolean;
	}

	std::uint32_t InputBoolean::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool InputBoolean::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::NumberVariable:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint8_t InputBoolean::get_value() const
	{
		return value;
	}

	void InputBoolean::set_value(std::uint8_t inputValue)
	{
		value = inputValue;
	}

	bool InputBoolean::get_enabled() const
	{
		return enabled;
	}

	void InputBoolean::set_enabled(bool value)
	{
		enabled = value;
	}

	InputString::InputString(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  optionsBitfield(0),
	  justificationBitfield(0),
	  length(0),
	  enabled(false)
	{
	}

	VirtualTerminalObjectType InputString::get_object_type() const
	{
		return VirtualTerminalObjectType::InputString;
	}

	std::uint32_t InputString::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool InputString::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::StringVariable:
					case VirtualTerminalObjectType::FontAttributes:
					case VirtualTerminalObjectType::InputAttributes:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	bool InputString::get_enabled() const
	{
		return enabled;
	}

	void InputString::set_enabled(bool value)
	{
		enabled = value;
	}

	bool InputString::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & optionsBitfield));
	}

	void InputString::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void InputString::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	InputString::HorizontalJustification InputString::get_horizontal_justification() const
	{
		return static_cast<HorizontalJustification>(justificationBitfield & 0x0F);
	}

	InputString::VerticalJustification InputString::get_vertical_justification() const
	{
		return static_cast<VerticalJustification>((justificationBitfield >> 4) & 0x0F);
	}

	void InputString::set_justification_bitfield(std::uint8_t value)
	{
		justificationBitfield = value;
	}

	InputNumber::InputNumber(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  scale(0.0f),
	  maximumValue(0),
	  minimumValue(0),
	  value(0),
	  offset(0),
	  numberOfDecimals(0),
	  options(0),
	  options2(0),
	  justificationBitfield(0),
	  format(false)
	{
	}

	VirtualTerminalObjectType InputNumber::get_object_type() const
	{
		return VirtualTerminalObjectType::InputNumber;
	}

	std::uint32_t InputNumber::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool InputNumber::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::NumberVariable:
					case VirtualTerminalObjectType::FontAttributes:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	InputNumber::HorizontalJustification InputNumber::get_horizontal_justification() const
	{
		return static_cast<HorizontalJustification>(justificationBitfield & 0x0F);
	}

	InputNumber::VerticalJustification InputNumber::get_vertical_justification() const
	{
		return static_cast<VerticalJustification>((justificationBitfield >> 4) & 0x0F);
	}

	void InputNumber::set_justification_bitfield(std::uint8_t value)
	{
		justificationBitfield = value;
	}

	float InputNumber::get_scale() const
	{
		return scale;
	}

	void InputNumber::set_scale(float value)
	{
		scale = value;
	}

	std::uint32_t InputNumber::get_maximum_value() const
	{
		return maximumValue;
	}

	void InputNumber::set_maximum_value(std::uint32_t value)
	{
		maximumValue = value;
	}

	std::uint32_t InputNumber::get_minimum_value() const
	{
		return minimumValue;
	}

	void InputNumber::set_minimum_value(std::uint32_t value)
	{
		minimumValue = value;
	}

	std::int32_t InputNumber::get_offset() const
	{
		return offset;
	}

	void InputNumber::set_offset(std::int32_t value)
	{
		offset = value;
	}

	std::uint8_t InputNumber::get_number_of_decimals() const
	{
		return numberOfDecimals;
	}

	void InputNumber::set_number_of_decimals(std::uint8_t value)
	{
		numberOfDecimals = value;
	}

	bool InputNumber::get_format() const
	{
		return format;
	}

	void InputNumber::set_format(bool value)
	{
		format = value;
	}

	bool InputNumber::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & options));
	}

	void InputNumber::set_options(std::uint8_t value)
	{
		options = value;
	}

	void InputNumber::set_option(Options option, bool value)
	{
		if (value)
		{
			options |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			options &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	bool InputNumber::get_option2(Options2 option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & options2));
	}

	void InputNumber::set_options2(std::uint8_t value)
	{
		options2 = value;
	}

	void InputNumber::set_option2(Options2 option, bool value)
	{
		if (value)
		{
			options2 |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			options2 &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	std::uint32_t InputNumber::get_value() const
	{
		return value;
	}

	void InputNumber::set_value(std::uint32_t inputValue)
	{
		value = inputValue;
	}

	InputList::InputList(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  numberOfListItems(0),
	  optionsBitfield(0),
	  value(0)
	{
	}

	VirtualTerminalObjectType InputList::get_object_type() const
	{
		return VirtualTerminalObjectType::InputList;
	}

	std::uint32_t InputList::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool InputList::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::NumberVariable:
					case VirtualTerminalObjectType::OutputString:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	bool InputList::get_option(Options option) const
	{
		return (0 != (optionsBitfield & (1 << static_cast<std::uint8_t>(option))));
	}

	void InputList::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void InputList::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	std::uint8_t InputList::get_value() const
	{
		return value;
	}

	void InputList::set_value(std::uint8_t inputValue)
	{
		value = inputValue;
	}

	OutputString::OutputString(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  optionsBitfield(0),
	  justificationBitfield(0),
	  length(0)
	{
	}

	VirtualTerminalObjectType OutputString::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputString;
	}

	std::uint32_t OutputString::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool OutputString::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::StringVariable:
					case VirtualTerminalObjectType::FontAttributes:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	bool OutputString::get_option(Options option) const
	{
		return (0 != (optionsBitfield & (1 << static_cast<std::uint8_t>(option))));
	}

	void OutputString::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void OutputString::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	OutputString::HorizontalJustification OutputString::get_horizontal_justification() const
	{
		return static_cast<HorizontalJustification>(justificationBitfield & 0x0F);
	}

	OutputString::VerticalJustification OutputString::get_vertical_justification() const
	{
		return static_cast<VerticalJustification>((justificationBitfield >> 4) & 0x0F);
	}

	void OutputString::set_justification_bitfield(std::uint8_t value)
	{
		justificationBitfield = value;
	}

	std::string OutputString::get_value() const
	{
		return stringValue;
	}

	void OutputString::set_value(std::string value)
	{
		stringValue = value;
	}

	OutputNumber::OutputNumber(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  scale(0.0f),
	  offset(0),
	  value(0),
	  numberOfDecimals(0),
	  optionsBitfield(0),
	  justificationBitfield(0),
	  format(false)
	{
	}

	VirtualTerminalObjectType OutputNumber::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputNumber;
	}

	std::uint32_t OutputNumber::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool OutputNumber::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::NumberVariable:
					case VirtualTerminalObjectType::FontAttributes:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	bool OutputNumber::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & optionsBitfield));
	}

	void OutputNumber::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void OutputNumber::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	OutputNumber::HorizontalJustification OutputNumber::get_horizontal_justification() const
	{
		return static_cast<HorizontalJustification>(justificationBitfield & 0x0F);
	}

	OutputNumber::VerticalJustification OutputNumber::get_vertical_justification() const
	{
		return static_cast<VerticalJustification>((justificationBitfield >> 4) & 0x0F);
	}

	void OutputNumber::set_justification_bitfield(std::uint8_t value)
	{
		justificationBitfield = value;
	}

	float OutputNumber::get_scale() const
	{
		return scale;
	}

	void OutputNumber::set_scale(float value)
	{
		scale = value;
	}

	std::int32_t OutputNumber::get_offset() const
	{
		return offset;
	}

	void OutputNumber::set_offset(std::int32_t value)
	{
		offset = value;
	}

	std::uint8_t OutputNumber::get_number_of_decimals() const
	{
		return numberOfDecimals;
	}

	void OutputNumber::set_number_of_decimals(std::uint8_t value)
	{
		numberOfDecimals = value;
	}

	bool OutputNumber::get_format() const
	{
		return format;
	}

	void OutputNumber::set_format(bool value)
	{
		format = value;
	}

	std::uint32_t OutputNumber::get_value() const
	{
		return value;
	}

	void OutputNumber::set_value(std::uint32_t inputValue)
	{
		value = inputValue;
	}

	OutputList::OutputList(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  numberOfListItems(0),
	  value(0)
	{
	}

	VirtualTerminalObjectType OutputList::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputList;
	}

	std::uint32_t OutputList::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool OutputList::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::NumberVariable:
					case VirtualTerminalObjectType::OutputString:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint8_t OutputList::get_number_of_list_items() const
	{
		return numberOfListItems;
	}

	std::uint8_t OutputList::get_value() const
	{
		return value;
	}

	void OutputList::set_value(std::uint8_t aValue)
	{
		value = aValue;
	}

	OutputLine::OutputLine(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  lineDirection(0)
	{
	}

	VirtualTerminalObjectType OutputLine::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputLine;
	}

	bool OutputLine::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::LineAttributes:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint32_t OutputLine::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	OutputLine::LineDirection OutputLine::get_line_direction() const
	{
		return static_cast<LineDirection>(lineDirection);
	}

	void OutputLine::set_line_direction(LineDirection value)
	{
		lineDirection = static_cast<std::uint8_t>(value);
	}

	OutputRectangle::OutputRectangle(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  lineSuppressionBitfield(0)
	{
	}

	VirtualTerminalObjectType OutputRectangle::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputRectangle;
	}

	std::uint32_t OutputRectangle::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool OutputRectangle::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::LineAttributes:
					case VirtualTerminalObjectType::FillAttributes:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint8_t OutputRectangle::get_line_suppression_bitfield() const
	{
		return lineSuppressionBitfield;
	}

	void OutputRectangle::set_line_suppression_bitfield(std::uint8_t value)
	{
		lineSuppressionBitfield = value;
	}

	OutputEllipse::OutputEllipse(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  ellipseType(0),
	  startAngle(0),
	  endAngle(0)
	{
	}

	VirtualTerminalObjectType OutputEllipse::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputEllipse;
	}

	std::uint32_t OutputEllipse::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool OutputEllipse::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::LineAttributes:
					case VirtualTerminalObjectType::FillAttributes:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	OutputEllipse::EllipseType OutputEllipse::get_ellipse_type() const
	{
		return static_cast<EllipseType>(ellipseType);
	}

	void OutputEllipse::set_ellipse_type(EllipseType value)
	{
		ellipseType = static_cast<std::uint8_t>(value);
	}

	std::uint8_t OutputEllipse::get_start_angle() const
	{
		return startAngle;
	}

	void OutputEllipse::set_start_angle(std::uint8_t value)
	{
		startAngle = value;
	}

	std::uint8_t OutputEllipse::get_end_angle() const
	{
		return endAngle;
	}

	void OutputEllipse::set_end_angle(std::uint8_t value)
	{
		endAngle = value;
	}

	OutputPolygon::OutputPolygon(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  polygonType(0)
	{
	}

	VirtualTerminalObjectType OutputPolygon::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputPolygon;
	}

	std::uint32_t OutputPolygon::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool OutputPolygon::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::LineAttributes:
					case VirtualTerminalObjectType::FillAttributes:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	void OutputPolygon::add_point(std::uint16_t x, std::uint16_t y)
	{
		pointList.push_back({ x, y });
	}

	std::uint8_t OutputPolygon::get_number_of_points() const
	{
		return static_cast<std::uint8_t>(pointList.size());
	}

	OutputPolygon::PolygonPoint OutputPolygon::get_point(std::uint8_t index)
	{
		PolygonPoint retVal = { 0, 0 };

		if (index < pointList.size())
		{
			retVal = pointList[index];
		}
		return retVal;
	}

	OutputPolygon::PolygonType OutputPolygon::get_type() const
	{
		return static_cast<PolygonType>(polygonType);
	}

	void OutputPolygon::set_type(PolygonType value)
	{
		polygonType = static_cast<std::uint8_t>(value);
	}

	OutputMeter::OutputMeter(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  minValue(0),
	  maxValue(0),
	  value(0),
	  needleColour(0),
	  borderColour(0),
	  arcAndTickColour(0),
	  optionsBitfield(0),
	  numberOfTicks(0),
	  startAngle(0),
	  endAngle(0)
	{
	}

	VirtualTerminalObjectType OutputMeter::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputMeter;
	}

	std::uint32_t OutputMeter::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool OutputMeter::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::NumberVariable:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint16_t OutputMeter::get_min_value() const
	{
		return minValue;
	}

	void OutputMeter::set_min_value(std::uint8_t value)
	{
		minValue = value;
	}

	std::uint16_t OutputMeter::get_max_value() const
	{
		return maxValue;
	}

	void OutputMeter::set_max_value(std::uint8_t value)
	{
		maxValue = value;
	}

	std::uint16_t OutputMeter::get_value() const
	{
		return value;
	}

	void OutputMeter::set_value(std::uint8_t aValue)
	{
		value = aValue;
	}

	std::uint8_t OutputMeter::get_needle_colour() const
	{
		return needleColour;
	}

	void OutputMeter::set_needle_colour(std::uint8_t value)
	{
		needleColour = value;
	}

	std::uint8_t OutputMeter::get_border_colour() const
	{
		return borderColour;
	}

	void OutputMeter::set_border_colour(std::uint8_t value)
	{
		borderColour = value;
	}

	std::uint8_t OutputMeter::get_arc_and_tick_colour() const
	{
		return arcAndTickColour;
	}

	void OutputMeter::set_arc_and_tick_colour(std::uint8_t value)
	{
		arcAndTickColour = value;
	}

	std::uint8_t OutputMeter::get_number_of_ticks() const
	{
		return numberOfTicks;
	}

	void OutputMeter::set_number_of_ticks(std::uint8_t value)
	{
		numberOfTicks = value;
	}

	bool OutputMeter::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & optionsBitfield));
	}

	void OutputMeter::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void OutputMeter::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	std::uint8_t OutputMeter::get_start_angle() const
	{
		return startAngle;
	}

	void OutputMeter::set_start_angle(std::uint8_t value)
	{
		startAngle = value;
	}

	std::uint8_t OutputMeter::get_end_angle() const
	{
		return endAngle;
	}

	void OutputMeter::set_end_angle(std::uint8_t value)
	{
		endAngle = value;
	}

	OutputLinearBarGraph::OutputLinearBarGraph(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  minValue(0),
	  maxValue(0),
	  targetValue(0),
	  targetValueReference(NULL_OBJECT_ID),
	  value(0),
	  numberOfTicks(0),
	  colour(0),
	  targetLineColour(0),
	  optionsBitfield(0)
	{
	}

	VirtualTerminalObjectType OutputLinearBarGraph::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputLinearBarGraph;
	}

	std::uint32_t OutputLinearBarGraph::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool OutputLinearBarGraph::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::NumberVariable:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint16_t OutputLinearBarGraph::get_min_value() const
	{
		return minValue;
	}

	void OutputLinearBarGraph::set_min_value(std::uint16_t value)
	{
		minValue = value;
	}

	std::uint16_t OutputLinearBarGraph::get_max_value() const
	{
		return maxValue;
	}

	void OutputLinearBarGraph::set_max_value(std::uint16_t value)
	{
		maxValue = value;
	}

	std::uint16_t OutputLinearBarGraph::get_value() const
	{
		return value;
	}

	void OutputLinearBarGraph::set_value(std::uint16_t aValue)
	{
		value = aValue;
	}

	std::uint16_t OutputLinearBarGraph::get_target_value() const
	{
		return targetValue;
	}

	void OutputLinearBarGraph::set_target_value(std::uint16_t value)
	{
		targetValue = value;
	}

	std::uint16_t OutputLinearBarGraph::get_target_value_reference() const
	{
		return targetValueReference;
	}

	void OutputLinearBarGraph::set_target_value_reference(std::uint16_t value)
	{
		targetValueReference = value;
	}

	std::uint8_t OutputLinearBarGraph::get_number_of_ticks() const
	{
		return numberOfTicks;
	}

	void OutputLinearBarGraph::set_number_of_ticks(std::uint8_t value)
	{
		numberOfTicks = value;
	}

	std::uint8_t OutputLinearBarGraph::get_colour() const
	{
		return colour;
	}

	void OutputLinearBarGraph::set_colour(std::uint8_t value)
	{
		colour = value;
	}

	std::uint8_t OutputLinearBarGraph::get_target_line_colour() const
	{
		return targetLineColour;
	}

	void OutputLinearBarGraph::set_target_line_colour(std::uint8_t value)
	{
		targetLineColour = value;
	}

	bool OutputLinearBarGraph::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & optionsBitfield));
	}

	void OutputLinearBarGraph::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void OutputLinearBarGraph::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	OutputArchedBarGraph::OutputArchedBarGraph(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  barGraphWidth(0),
	  minValue(0),
	  maxValue(0),
	  value(0),
	  targetValue(0),
	  targetValueReference(NULL_OBJECT_ID),
	  targetLineColour(0),
	  colour(0),
	  optionsBitfield(0),
	  startAngle(0),
	  endAngle(0)
	{
	}

	VirtualTerminalObjectType OutputArchedBarGraph::get_object_type() const
	{
		return VirtualTerminalObjectType::OutputArchedBarGraph;
	}

	std::uint32_t OutputArchedBarGraph::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool OutputArchedBarGraph::get_is_valid() const
	{
		bool anyWrongChildType = false;

		for (auto &child : children)
		{
			auto childObject = get_object_by_id(child.id);
			if (nullptr != childObject)
			{
				switch (childObject->get_object_type())
				{
					case VirtualTerminalObjectType::NumberVariable:
					{
					}
					break;

					default:
					{
						anyWrongChildType = true;
					}
					break;
				}
			}
		}
		return ((!anyWrongChildType) &&
		        (NULL_OBJECT_ID != objectID));
	}

	std::uint16_t OutputArchedBarGraph::get_bar_graph_width() const
	{
		return barGraphWidth;
	}

	void OutputArchedBarGraph::set_bar_graph_width(std::uint16_t value)
	{
		barGraphWidth = value;
	}

	std::uint16_t OutputArchedBarGraph::get_min_value() const
	{
		return minValue;
	}

	void OutputArchedBarGraph::set_min_value(std::uint16_t value)
	{
		minValue = value;
	}

	std::uint16_t OutputArchedBarGraph::get_max_value() const
	{
		return maxValue;
	}

	void OutputArchedBarGraph::set_max_value(std::uint16_t value)
	{
		maxValue = value;
	}

	std::uint16_t OutputArchedBarGraph::get_value() const
	{
		return value;
	}

	void OutputArchedBarGraph::set_value(std::uint16_t aValue)
	{
		value = aValue;
	}

	std::uint8_t OutputArchedBarGraph::get_target_line_colour() const
	{
		return targetLineColour;
	}

	void OutputArchedBarGraph::set_target_line_colour(std::uint8_t value)
	{
		targetLineColour = value;
	}

	std::uint8_t OutputArchedBarGraph::get_colour() const
	{
		return colour;
	}

	void OutputArchedBarGraph::set_colour(std::uint8_t value)
	{
		colour = value;
	}

	bool OutputArchedBarGraph::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & optionsBitfield));
	}

	void OutputArchedBarGraph::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void OutputArchedBarGraph::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	std::uint8_t OutputArchedBarGraph::get_start_angle() const
	{
		return startAngle;
	}

	void OutputArchedBarGraph::set_start_angle(std::uint8_t value)
	{
		startAngle = value;
	}

	std::uint8_t OutputArchedBarGraph::get_end_angle() const
	{
		return endAngle;
	}

	void OutputArchedBarGraph::set_end_angle(std::uint8_t value)
	{
		endAngle = value;
	}

	std::uint16_t OutputArchedBarGraph::get_target_value() const
	{
		return targetValue;
	}

	void OutputArchedBarGraph::set_target_value(std::uint16_t value)
	{
		targetValue = value;
	}

	std::uint16_t OutputArchedBarGraph::get_target_value_reference() const
	{
		return targetValueReference;
	}

	void OutputArchedBarGraph::set_target_value_reference(std::uint16_t value)
	{
		targetValueReference = value;
	}

	PictureGraphic::PictureGraphic(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  numberOfBytesInRawData(0),
	  actualWidth(0),
	  actualHeight(0),
	  formatByte(0),
	  optionsBitfield(0),
	  transparencyColour(0)
	{
	}

	VirtualTerminalObjectType PictureGraphic::get_object_type() const
	{
		return VirtualTerminalObjectType::PictureGraphic;
	}

	std::uint32_t PictureGraphic::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool PictureGraphic::get_is_valid() const
	{
		return true;
	}

	std::vector<std::uint8_t> &PictureGraphic::get_raw_data()
	{
		return rawData;
	}

	void PictureGraphic::set_raw_data(std::uint8_t *data, std::uint32_t size)
	{
		rawData.clear();
		rawData.resize(size);

		for (std::uint32_t i = 0; i < size; i++)
		{
			rawData[i] = data[i];
		}
	}

	void PictureGraphic::add_raw_data(std::uint8_t dataByte)
	{
		rawData.push_back(dataByte);
	}

	std::uint32_t PictureGraphic::get_number_of_bytes_in_raw_data() const
	{
		return numberOfBytesInRawData;
	}

	void PictureGraphic::set_number_of_bytes_in_raw_data(std::uint32_t value)
	{
		numberOfBytesInRawData = value;
		rawData.reserve(value);
	}

	std::uint16_t PictureGraphic::get_actual_width() const
	{
		return actualWidth;
	}

	void PictureGraphic::set_actual_width(std::uint16_t value)
	{
		actualWidth = value;
	}

	std::uint16_t PictureGraphic::get_actual_height() const
	{
		return actualHeight;
	}

	void PictureGraphic::set_actual_height(std::uint16_t value)
	{
		actualHeight = value;
	}

	PictureGraphic::Format PictureGraphic::get_format() const
	{
		return static_cast<Format>(formatByte);
	}

	void PictureGraphic::set_format(Format value)
	{
		formatByte = static_cast<std::uint8_t>(value);
	}

	bool PictureGraphic::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & optionsBitfield));
	}

	void PictureGraphic::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void PictureGraphic::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

	std::uint8_t PictureGraphic::get_transparency_colour() const
	{
		return transparencyColour;
	}

	void PictureGraphic::set_transparency_colour(std::uint8_t value)
	{
		transparencyColour = value;
	}

	NumberVariable::NumberVariable(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  value(0)
	{
	}

	VirtualTerminalObjectType NumberVariable::get_object_type() const
	{
		return VirtualTerminalObjectType::NumberVariable;
	}

	std::uint32_t NumberVariable::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool NumberVariable::get_is_valid() const
	{
		return true;
	}

	std::uint32_t NumberVariable::get_value() const
	{
		return value;
	}

	void NumberVariable::set_value(std::uint32_t aValue)
	{
		value = aValue;
	}

	StringVariable::StringVariable(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable)
	{
	}

	VirtualTerminalObjectType StringVariable::get_object_type() const
	{
		return VirtualTerminalObjectType::StringVariable;
	}

	std::uint32_t StringVariable::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool StringVariable::get_is_valid() const
	{
		return true;
	}

	std::string StringVariable::get_value()
	{
		return value;
	}

	void StringVariable::set_value(std::string aValue)
	{
		value = aValue;
	}

	FontAttributes::FontAttributes(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  colour(0),
	  size(0),
	  type(0),
	  style(0)
	{
	}

	VirtualTerminalObjectType FontAttributes::get_object_type() const
	{
		return VirtualTerminalObjectType::FontAttributes;
	}

	std::uint32_t FontAttributes::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool FontAttributes::get_is_valid() const
	{
		return true;
	}

	FontAttributes::FontType FontAttributes::get_type() const
	{
		return static_cast<FontType>(type);
	}

	void FontAttributes::set_type(FontType value)
	{
		type = static_cast<std::uint8_t>(value);
	}

	std::uint8_t FontAttributes::get_style() const
	{
		return style;
	}

	bool FontAttributes::get_style(FontStyleBits styleSetting)
	{
		return (style >> static_cast<std::uint8_t>(styleSetting)) & 0x01;
	}

	void FontAttributes::set_style(FontStyleBits bit, bool value)
	{
		style = (static_cast<std::uint8_t>(value) << static_cast<std::uint8_t>(bit));
	}

	void FontAttributes::set_style(std::uint8_t value)
	{
		style = value;
	}

	FontAttributes::FontSize FontAttributes::get_size() const
	{
		return static_cast<FontSize>(size);
	}

	void FontAttributes::set_size(FontSize value)
	{
		size = static_cast<std::uint8_t>(value);
	}

	std::uint8_t FontAttributes::get_colour() const
	{
		return colour;
	}

	void FontAttributes::set_colour(std::uint8_t value)
	{
		colour = value;
	}

	std::uint8_t FontAttributes::get_font_width_pixels() const
	{
		std::uint8_t retVal = 0;

		switch (static_cast<FontSize>(size))
		{
			case FontSize::Size6x8:
			{
				retVal = 6;
			}
			break;

			case FontSize::Size8x8:
			case FontSize::Size8x12:
			{
				retVal = 8;
			}
			break;

			case FontSize::Size12x16:
			{
				retVal = 12;
			}
			break;

			case FontSize::Size16x16:
			case FontSize::Size16x24:
			{
				retVal = 16;
			}
			break;

			case FontSize::Size24x32:
			{
				retVal = 24;
			}
			break;

			case FontSize::Size32x32:
			case FontSize::Size32x48:
			{
				retVal = 32;
			}
			break;

			case FontSize::Size48x64:
			{
				retVal = 48;
			}
			break;

			case FontSize::Size64x64:
			case FontSize::Size64x96:
			{
				retVal = 64;
			}
			break;

			case FontSize::Size96x128:
			{
				retVal = 96;
			}
			break;

			case FontSize::Size128x128:
			case FontSize::Size128x192:
			{
				retVal = 128;
			}
			break;

			default:
				break;
		}
		return retVal;
	}

	std::uint8_t FontAttributes::get_font_height_pixels() const
	{
		std::uint8_t retVal = 0;

		switch (static_cast<FontSize>(size))
		{
			case FontSize::Size6x8:
			case FontSize::Size8x8:
			{
				retVal = 8;
			}
			break;

			case FontSize::Size8x12:
			{
				retVal = 12;
			}
			break;

			case FontSize::Size12x16:
			case FontSize::Size16x16:
			{
				retVal = 16;
			}
			break;

			case FontSize::Size16x24:
			{
				retVal = 24;
			}
			break;

			case FontSize::Size24x32:
			case FontSize::Size32x32:
			{
				retVal = 32;
			}
			break;

			case FontSize::Size32x48:
			{
				retVal = 48;
			}
			break;

			case FontSize::Size48x64:
			case FontSize::Size64x64:
			{
				retVal = 64;
			}
			break;

			case FontSize::Size64x96:
			{
				retVal = 96;
			}
			break;

			case FontSize::Size96x128:
			case FontSize::Size128x128:
			{
				retVal = 128;
			}
			break;

			case FontSize::Size128x192:
			{
				retVal = 192;
			}
			break;

			default:
				break;
		}
		return retVal;
	}

	LineAttributes::LineAttributes(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  lineArtBitpattern(0)
	{
	}

	VirtualTerminalObjectType LineAttributes::get_object_type() const
	{
		return VirtualTerminalObjectType::LineAttributes;
	}

	std::uint32_t LineAttributes::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool LineAttributes::get_is_valid() const
	{
		return true;
	}

	std::uint16_t LineAttributes::get_line_art_bit_pattern() const
	{
		return lineArtBitpattern;
	}

	void LineAttributes::set_line_art_bit_pattern(std::uint16_t value)
	{
		lineArtBitpattern = value;
	}

	FillAttributes::FillAttributes(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  fillPattern(NULL_OBJECT_ID),
	  type(FillType::NoFill)
	{
	}

	VirtualTerminalObjectType FillAttributes::get_object_type() const
	{
		return VirtualTerminalObjectType::FillAttributes;
	}

	std::uint32_t FillAttributes::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool FillAttributes::get_is_valid() const
	{
		return true;
	}

	std::uint16_t FillAttributes::get_fill_pattern() const
	{
		return fillPattern;
	}

	void FillAttributes::set_fill_pattern(std::uint16_t value)
	{
		fillPattern = value;
	}

	FillAttributes::FillType FillAttributes::get_type() const
	{
		return type;
	}

	void FillAttributes::set_type(FillType value)
	{
		type = value;
	}

	InputAttributes::InputAttributes(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  validationType(0)
	{
	}

	VirtualTerminalObjectType InputAttributes::get_object_type() const
	{
		return VirtualTerminalObjectType::InputAttributes;
	}

	std::uint32_t InputAttributes::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool InputAttributes::get_is_valid() const
	{
		return true;
	}

	std::string InputAttributes::get_validation_string() const
	{
		return validationString;
	}

	void InputAttributes::set_validation_string(std::string value)
	{
		validationString = value;
	}

	std::uint8_t InputAttributes::get_validation_type() const
	{
		return validationType;
	}

	void InputAttributes::set_validation_type(std::uint8_t value)
	{
		validationType = value;
	}

	ExtendedInputAttributes::ExtendedInputAttributes(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable),
	  validationType(0)
	{
	}

	VirtualTerminalObjectType ExtendedInputAttributes::get_object_type() const
	{
		return VirtualTerminalObjectType::ExtendedInputAttributes;
	}

	std::uint32_t ExtendedInputAttributes::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool ExtendedInputAttributes::get_is_valid() const
	{
		return true;
	}

	std::uint8_t ExtendedInputAttributes::get_number_of_code_planes() const
	{
		return static_cast<std::uint8_t>(codePlanes.size());
	}

	void ExtendedInputAttributes::set_number_of_code_planes(std::uint8_t value)
	{
		codePlanes.resize(value);
	}

	std::uint8_t ExtendedInputAttributes::get_validation_type() const
	{
		return validationType;
	}

	void ExtendedInputAttributes::set_validation_type(std::uint8_t value)
	{
		validationType = value;
	}

	ObjectPointer::ObjectPointer(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable)
	{
	}

	VirtualTerminalObjectType ObjectPointer::get_object_type() const
	{
		return VirtualTerminalObjectType::ObjectPointer;
	}

	std::uint32_t ObjectPointer::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool ObjectPointer::get_is_valid() const
	{
		return true;
	}

	ExternalObjectPointer::ExternalObjectPointer(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable)
	{
	}

	VirtualTerminalObjectType ExternalObjectPointer::get_object_type() const
	{
		return VirtualTerminalObjectType::ExternalObjectPointer;
	}

	std::uint32_t ExternalObjectPointer::get_minumum_object_length() const
	{
		return 9;
	}

	bool ExternalObjectPointer::get_is_valid() const
	{
		return true;
	}

	std::uint16_t ExternalObjectPointer::get_default_object_id() const
	{
		return defaultObjectID;
	}

	void ExternalObjectPointer::set_default_object_id(std::uint16_t id)
	{
		defaultObjectID = id;
	}

	std::uint16_t ExternalObjectPointer::get_external_reference_name_id() const
	{
		return externalReferenceNAMEID;
	}

	void ExternalObjectPointer::set_external_reference_name_id(std::uint16_t id)
	{
		externalReferenceNAMEID = id;
	}

	std::uint16_t ExternalObjectPointer::get_external_object_id() const
	{
		return externalObjectID;
	}

	void ExternalObjectPointer::set_external_object_id(std::uint16_t id)
	{
		externalObjectID = id;
	}

	const std::array<std::uint8_t, 28> Macro::ALLOWED_COMMANDS_LOOKUP_TABLE = {
		static_cast<std::uint8_t>(Command::HideShowObject),
		static_cast<std::uint8_t>(Command::EnableDisableObject),
		static_cast<std::uint8_t>(Command::SelectInputObject),
		static_cast<std::uint8_t>(Command::ControlAudioSignal),
		static_cast<std::uint8_t>(Command::SetAudioVolume),
		static_cast<std::uint8_t>(Command::ChangeChildLocation),
		static_cast<std::uint8_t>(Command::ChangeSize),
		static_cast<std::uint8_t>(Command::ChangeBackgroundColour),
		static_cast<std::uint8_t>(Command::ChangeNumericValue),
		static_cast<std::uint8_t>(Command::ChangeEndPoint),
		static_cast<std::uint8_t>(Command::ChangeFontAttributes),
		static_cast<std::uint8_t>(Command::ChangeLineAttributes),
		static_cast<std::uint8_t>(Command::ChangeFillAttributes),
		static_cast<std::uint8_t>(Command::ChangeActiveMask),
		static_cast<std::uint8_t>(Command::ChangeSoftKeyMask),
		static_cast<std::uint8_t>(Command::ChangeAttribute),
		static_cast<std::uint8_t>(Command::ChangePriority),
		static_cast<std::uint8_t>(Command::ChangeListItem),
		static_cast<std::uint8_t>(Command::ChangeStringValue),
		static_cast<std::uint8_t>(Command::ChangeChildPosition),
		static_cast<std::uint8_t>(Command::ChangeObjectLabel),
		static_cast<std::uint8_t>(Command::ChangePolygonPoint),
		static_cast<std::uint8_t>(Command::LockUnlockMask),
		static_cast<std::uint8_t>(Command::ExecuteMacro),
		static_cast<std::uint8_t>(Command::ChangePolygonScale),
		static_cast<std::uint8_t>(Command::GraphicsContextCommand),
		static_cast<std::uint8_t>(Command::SelectColourMap),
		static_cast<std::uint8_t>(Command::ExecuteExtendedMacro)
	};

	Macro::Macro(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable)
	{
	}

	VirtualTerminalObjectType Macro::get_object_type() const
	{
		return VirtualTerminalObjectType::Macro;
	}

	std::uint32_t Macro::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool Macro::get_is_valid() const
	{
		bool retVal = true;

		if (commandPackets.empty())
		{
			retVal = true;
		}
		else
		{
			for (const auto &command : commandPackets)
			{
				bool currentCommandAllowed = false;

				for (const auto &allowedCommand : ALLOWED_COMMANDS_LOOKUP_TABLE)
				{
					if (command.at(0) == allowedCommand)
					{
						currentCommandAllowed = true;
						break;
					}
				}

				if (!currentCommandAllowed)
				{
					retVal = false;
					break;
				}
			}
		}
		return retVal;
	}

	bool Macro::add_command_packet(std::array<std::uint8_t, CAN_DATA_LENGTH> command)
	{
		bool retVal = false;

		if (commandPackets.size() < 255)
		{
			commandPackets.push_back(command);
			retVal = true;
		}
		return retVal;
	}

	std::uint8_t Macro::get_number_of_commands() const
	{
		return static_cast<std::uint8_t>(commandPackets.size());
	}

	bool Macro::get_command_packet(std::uint8_t index, std::array<std::uint8_t, CAN_DATA_LENGTH> &command)
	{
		bool retVal = false;

		if (index < commandPackets.size())
		{
			std::copy(std::begin(commandPackets.at(index)), std::end(commandPackets.at(index)), std::begin(command));
			retVal = true;
		}
		return retVal;
	}

	bool Macro::remove_command_packet(std::uint8_t index)
	{
		bool retVal = false;

		if (index < commandPackets.size())
		{
			auto eraseLocation = commandPackets.begin() + index;
			commandPackets.erase(eraseLocation);
			retVal = true;
		}
		return retVal;
	}

	ColourMap::ColourMap(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable)
	{
	}

	VirtualTerminalObjectType ColourMap::get_object_type() const
	{
		return VirtualTerminalObjectType::ColourMap;
	}

	std::uint32_t ColourMap::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool ColourMap::get_is_valid() const
	{
		return true;
	}

	WindowMask::WindowMask(std::map<std::uint16_t, std::shared_ptr<VTObject>> &memberObjectPool, VTColourTable &currentColourTable) :
	  VTObject(memberObjectPool, currentColourTable)
	{
	}

	VirtualTerminalObjectType WindowMask::get_object_type() const
	{
		return VirtualTerminalObjectType::WindowMask;
	}

	std::uint32_t WindowMask::get_minumum_object_length() const
	{
		return MIN_OBJECT_LENGTH;
	}

	bool WindowMask::get_is_valid() const
	{
		bool anyWrongChildType = false;

		if (WindowType::Freeform != get_window_type())
		{
			if (NULL_OBJECT_ID != title)
			{
				auto titleObject = get_object_by_id(title);

				if (nullptr != titleObject)
				{
					if ((VirtualTerminalObjectType::ObjectPointer != titleObject->get_object_type()) &&
					    (VirtualTerminalObjectType::OutputString != titleObject->get_object_type()))
					{
						anyWrongChildType = true;
					}
					else if (VirtualTerminalObjectType::ObjectPointer == titleObject->get_object_type())
					{
						if (0 == titleObject->get_number_children())
						{
							anyWrongChildType = true;
						}
						else
						{
							std::uint16_t titleObjectPointedTo = std::static_pointer_cast<ObjectPointer>(titleObject)->get_child_id(0);
							auto child = get_object_by_id(titleObjectPointedTo);

							if ((nullptr != child) && (VirtualTerminalObjectType::OutputString == child->get_object_type()))
							{
								// Valid
							}
							else
							{
								anyWrongChildType = true;
							}
						}
					}
					else
					{
						// Valid
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			else
			{
				anyWrongChildType = true;
			}

			if (NULL_OBJECT_ID != name)
			{
				auto nameObject = get_object_by_id(name);

				if (nullptr != nameObject)
				{
					if ((VirtualTerminalObjectType::ObjectPointer != nameObject->get_object_type()) &&
					    (VirtualTerminalObjectType::OutputString != nameObject->get_object_type()))
					{
						anyWrongChildType = true;
					}
					else if (VirtualTerminalObjectType::ObjectPointer == nameObject->get_object_type())
					{
						if (0 == nameObject->get_number_children())
						{
							anyWrongChildType = true;
						}
						else
						{
							std::uint16_t titleObjectPointedTo = std::static_pointer_cast<ObjectPointer>(nameObject)->get_child_id(0);
							auto child = get_object_by_id(titleObjectPointedTo);

							if ((nullptr != child) && (VirtualTerminalObjectType::OutputString == child->get_object_type()))
							{
								// Valid
							}
							else
							{
								anyWrongChildType = true;
							}
						}
					}
					else
					{
						// Valid
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			else
			{
				anyWrongChildType = true;
			}

			if (NULL_OBJECT_ID != icon)
			{
				auto nameObject = get_object_by_id(icon);

				if (nullptr != nameObject)
				{
					switch (nameObject->get_object_type())
					{
						case VirtualTerminalObjectType::OutputString:
						case VirtualTerminalObjectType::Container:
						case VirtualTerminalObjectType::OutputNumber:
						case VirtualTerminalObjectType::OutputList:
						case VirtualTerminalObjectType::OutputLine:
						case VirtualTerminalObjectType::OutputRectangle:
						case VirtualTerminalObjectType::OutputEllipse:
						case VirtualTerminalObjectType::OutputPolygon:
						case VirtualTerminalObjectType::OutputMeter:
						case VirtualTerminalObjectType::OutputLinearBarGraph:
						case VirtualTerminalObjectType::OutputArchedBarGraph:
						case VirtualTerminalObjectType::GraphicsContext:
						case VirtualTerminalObjectType::PictureGraphic:
						case VirtualTerminalObjectType::ObjectPointer:
						case VirtualTerminalObjectType::ScaledGraphic:
						{
							// Valid
						}
						break;

						default:
						{
							anyWrongChildType = true;
						}
						break;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			else
			{
				anyWrongChildType = true;
			}
		}
		else if (NULL_OBJECT_ID != title)
		{
			anyWrongChildType = true;
		}

		// Validate the actual child object references for each window type
		switch (static_cast<WindowType>(windowType))
		{
			case WindowType::Freeform:
			{
			}
			break;

			case WindowType::NumericOutputValueWithUnits1x1:
			case WindowType::NumericOutputValueWithUnits2x1:
			{
				if (2 == get_number_children())
				{
					auto outputNum = get_object_by_id(get_child_id(0));
					auto outputString = get_object_by_id(get_child_id(0));

					if ((nullptr == outputNum) ||
					    (nullptr == outputString) ||
					    (VirtualTerminalObjectType::OutputNumber != outputNum->get_object_type()) ||
					    (VirtualTerminalObjectType::OutputString != outputString->get_object_type()))
					{
						anyWrongChildType = true;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			break;

			case WindowType::NumericOutputValueNoUnits1x1:
			case WindowType::NumericOutputValueNoUnits2x1:
			{
				if (1 == get_number_children())
				{
					auto outputNum = get_object_by_id(get_child_id(0));

					if ((nullptr == outputNum) ||
					    (VirtualTerminalObjectType::OutputNumber != outputNum->get_object_type()))
					{
						anyWrongChildType = true;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			break;

			case WindowType::StringOutputValue1x1:
			case WindowType::StringOutputValue2x1:
			{
				if (1 == get_number_children())
				{
					auto outputString = get_object_by_id(get_child_id(0));

					if ((nullptr == outputString) ||
					    (VirtualTerminalObjectType::OutputString != outputString->get_object_type()))
					{
						anyWrongChildType = true;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			break;

			case WindowType::NumericInputValueWithUnits1x1:
			case WindowType::NumericInputValueWithUnits2x1:
			{
				if (2 == get_number_children())
				{
					auto inputNum = get_object_by_id(get_child_id(0));
					auto outputString = get_object_by_id(get_child_id(0));

					if ((nullptr == inputNum) ||
					    (nullptr == outputString) ||
					    (VirtualTerminalObjectType::InputNumber != inputNum->get_object_type()) ||
					    (VirtualTerminalObjectType::OutputString != outputString->get_object_type()))
					{
						anyWrongChildType = true;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			break;

			case WindowType::NumericInputValueNoUnits1x1:
			case WindowType::NumericInputValueNoUnits2x1:
			{
				if (1 == get_number_children())
				{
					auto inputNum = get_object_by_id(get_child_id(0));

					if ((nullptr == inputNum) ||
					    (VirtualTerminalObjectType::InputNumber != inputNum->get_object_type()))
					{
						anyWrongChildType = true;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			break;

			case WindowType::StringInputValue1x1:
			case WindowType::StringInputValue2x1:
			{
				if (1 == get_number_children())
				{
					auto inputStr = get_object_by_id(get_child_id(0));

					if ((nullptr == inputStr) ||
					    (VirtualTerminalObjectType::InputString != inputStr->get_object_type()))
					{
						anyWrongChildType = true;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			break;

			case WindowType::HorizontalLinearBarGraphNoUnits1x1:
			case WindowType::HorizontalLinearBarGraphNoUnits2x1:
			{
				if (1 == get_number_children())
				{
					auto outputBargraph = get_object_by_id(get_child_id(0));

					if ((nullptr == outputBargraph) ||
					    (VirtualTerminalObjectType::OutputLinearBarGraph != outputBargraph->get_object_type()))
					{
						anyWrongChildType = true;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			break;

			case WindowType::SingleButton1x1:
			case WindowType::SingleButton2x1:
			{
				if (1 == get_number_children())
				{
					auto button = get_object_by_id(get_child_id(0));

					if ((nullptr == button) ||
					    (VirtualTerminalObjectType::Button != button->get_object_type()))
					{
						anyWrongChildType = true;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			break;

			case WindowType::DoubleButton1x1:
			case WindowType::DoubleButton2x1:
			{
				if (2 == get_number_children())
				{
					auto button1 = get_object_by_id(get_child_id(0));
					auto button2 = get_object_by_id(get_child_id(0));

					if ((nullptr == button1) ||
					    (nullptr == button2) ||
					    (VirtualTerminalObjectType::Button != button1->get_object_type()) ||
					    (VirtualTerminalObjectType::Button != button2->get_object_type()))
					{
						anyWrongChildType = true;
					}
				}
				else
				{
					anyWrongChildType = true;
				}
			}
			break;

			default:
			{
				anyWrongChildType = true;
			}
			break;
		}
		return !anyWrongChildType;
	}

	std::uint16_t WindowMask::get_name_object_id() const
	{
		return name;
	}

	void WindowMask::set_name_object_id(std::uint16_t object)
	{
		name = object;
	}

	std::uint16_t WindowMask::get_title_object_id() const
	{
		return title;
	}

	void WindowMask::set_title_object_id(std::uint16_t object)
	{
		title = object;
	}

	std::uint16_t WindowMask::get_icon_object_id() const
	{
		return icon;
	}

	void WindowMask::set_icon_object_id(std::uint16_t object)
	{
		icon = object;
	}

	WindowMask::WindowType WindowMask::get_window_type() const
	{
		return static_cast<WindowType>(windowType);
	}

	void WindowMask::set_window_type(WindowType type)
	{
		if (static_cast<std::uint8_t>(type) <= static_cast<std::uint8_t>(WindowType::DoubleButton2x1))
		{
			windowType = static_cast<std::uint8_t>(type);
		}
	}

	bool WindowMask::get_option(Options option) const
	{
		return (0 != ((1 << static_cast<std::uint8_t>(option)) & optionsBitfield));
	}

	void WindowMask::set_options(std::uint8_t value)
	{
		optionsBitfield = value;
	}

	void WindowMask::set_option(Options option, bool value)
	{
		if (value)
		{
			optionsBitfield |= (1 << static_cast<std::uint8_t>(option));
		}
		else
		{
			optionsBitfield &= ~(1 << static_cast<std::uint8_t>(option));
		}
	}

} // namespace isobus

//
// Kernel Object
//
#pragma once
#include "Object.hpp"
#include <unordered_map>
#include <string_view>

namespace Chino
{
	class Directory : public Object
	{
	public:
		void AddItem(std::string_view name, Object& obj);
		ObjectPtr<Object> GetItem(std::string_view name);
	private:
		std::unordered_map<std::string, ObjectPtr<Object>> items_;
	};
}

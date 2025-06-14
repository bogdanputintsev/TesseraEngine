#pragma once
#include <functional>

#include "services/Service.h"

namespace parus
{
	

	class GraphicsLibrary : public Service
	{
	public:
		virtual ~GraphicsLibrary() = default;

		virtual void init() = 0;
		virtual void drawFrame() = 0;
		virtual void handleMinimization() = 0;
		[[nodiscard]] virtual std::vector<const char*> getRequiredExtensions() const = 0;
		virtual void clean() = 0;
	};

}


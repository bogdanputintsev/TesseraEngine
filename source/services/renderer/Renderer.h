#pragma once
#include "services/Service.h"

namespace parus
{

	class Renderer : public Service
	{
	public:
		virtual ~Renderer() = default;

		virtual void init() = 0;
		virtual void registerEvents() = 0;
		virtual void clean() = 0;
		virtual void drawFrame() = 0;
		virtual void deviceWaitIdle() = 0;

	};
}

#pragma once

#include "ApplicationInfo.h"


namespace parus
{
	
	class Application final
	{
	public:
		void init();
		void loop();
		void clean();

		[[nodiscard]] ApplicationInfo getApplicationInfo() const { return applicationInfo; }
	private:
		static void registerServices();
		void registerEvents();
		
		bool isRunning = false;

		ApplicationInfo applicationInfo{};
	};

}


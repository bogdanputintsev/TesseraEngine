#pragma once

#include "engine/logs/Logs.h"


/**
 * \brief This macro assures that the condition is true. Otherwise, it logs an error and throw an exception.
 * \param condition The boolean condition that will be checked.
 * \param msg This message will be printed if the condition fails.
 */
#define ASSERT(condition, msg)									\
    do															\
	{															\
        if (!(condition))										\
		{														\
            LOG_FATAL(msg);										\
			throw std::runtime_error("Assertion failed.");		\
        }														\
    } while (0)

/**
 * \brief This macro assures that the condition is true if application is in debug mode.
 * Otherwise, it logs an error and throw an exception.
 * \param condition The boolean condition that will be checked.
 * \param msg This message will be printed if the condition fails.
 */
#if IN_DEBUG_MODE												
	#define DEBUG_ASSERT(condition, msg) ASSERT(condition, msg)									
#else
	#define DEBUG_ASSERT(condition, msg)
#endif
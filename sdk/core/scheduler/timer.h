// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "plic.h"
#include "thread.h"
#include <platform-timer.hh>
#include <stdint.h>
#include <tick_macros.h>
#include <utils.hh>

namespace
{
	/**
	 * Concept for the interface to setting the system timer.
	 */
	template<typename T>
	concept IsTimer = requires(uint32_t cycles)
	{
		{T::init()};
		{T::setnext(cycles)};
	};

	static_assert(
	  IsTimer<TimerCore>,
	  "Platform's timer implementation does not meet the required interface");

	class Timer final : private TimerCore
	{
		inline static uint64_t lastTickTime = 0;
		public:
		static void interrupt_setup()
		{
			static_assert(TIMERCYCLES_PER_TICK <= UINT32_MAX,
			              "Cycles per tick can't be represented in 32 bits. "
			              "Double check your platform config");
			init();
			setnext(TIMERCYCLES_PER_TICK);
		}

		static void update()
		{

			if (Thread::waitingList == nullptr)
			{
				clear();
			}
			else
			{
				setnext(TIMERCYCLES_PER_TICK * (Thread::waitingList->expiryTime - Thread::ticksSinceBoot));
			}
		}

		static void expiretimers()
		{
			// TODO: Should be reading the timer's time, not the core's time.
			// They are currently the same value, but that's not guaranteed.
			uint64_t now = rdcycle64();
			uint32_t elapsed = now - lastTickTime;
			lastTickTime = now;
			Thread::ticksSinceBoot += std::max(1U, elapsed / TIMERCYCLES_PER_TICK);
			if (Thread::waitingList == nullptr)
			{
				return;
			}
			for (Thread *iter = Thread::waitingList;;)
			{
				if (iter->expiryTime <= Thread::ticksSinceBoot)
				{
					Thread *iterNext = iter->timerNext;

					iter->ready(Thread::WakeReason::Timer);
					iter = iterNext;
					if (Thread::waitingList == nullptr ||
					    iter == Thread::waitingList)
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
		}
	};
} // namespace

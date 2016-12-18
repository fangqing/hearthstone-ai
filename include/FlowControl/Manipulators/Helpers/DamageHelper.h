#pragma once

#include "State/State.h"

namespace FlowControl
{
	namespace Manipulators
	{
		namespace Helpers
		{
			class DamageHelper
			{
			public:
				DamageHelper(state::State & state, FlowContext & flow_context, state::CardRef card_ref, state::Cards::Card & card, int amount);
			};
		}
	}
}
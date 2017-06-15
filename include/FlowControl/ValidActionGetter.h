#pragma once

#include <vector>
#include "state/State.h"

namespace FlowControl
{
	class ValidActionGetter
	{
	public:
		ValidActionGetter(state::State const& state) : state_(state) {}

	public: // check valid actions
			// These functions MUST return valid for all actually available actions
			// These functions can return valid for ones actually are not valid actions

		std::vector<int> GetPlayableCards()
		{
			std::vector<int> playable_cards;
			auto const& hand = state_.GetCurrentPlayer().hand_;
			for (int i = 0; i < hand.Size(); ++i) {
				state::CardRef card_ref = hand.Get(i);
				playable_cards.push_back(i);
			}
			return playable_cards;
		}

		bool IsAttackable(state::CardRef card_ref) {
			state::Cards::Card const& card = state_.GetCardsManager().Get(card_ref);

			if (card.GetRawData().cant_attack) return false;
			if (card.GetRawData().freezed) return false;

			if (card.GetCardType() == state::kCardTypeMinion) {
				if (card.HasCharge() == false && card.GetRawData().just_played) return false;
			}

			if (card.GetRawData().num_attacks_this_turn >= card.GetMaxAttacksPerTurn()) return false;

			if (state_.GetCardAttackConsiderWeapon(card_ref) <= 0) return false;

			return true;
		}

		std::vector<state::CardRef> GetAttackers() {
			std::vector<state::CardRef> attackers;
			auto const& player = state_.GetBoard().Get(state_.GetCurrentPlayerId());
			auto op = [&](state::CardRef card_ref) { return IsAttackable(card_ref); };

			state::CardRef hero_ref = player.GetHeroRef();
			if (op(hero_ref)) attackers.push_back(hero_ref);

			player.minions_.ForEach([&](state::CardRef card_ref) {
				if (op(card_ref)) attackers.push_back(card_ref);
			});
			
			return attackers;
		}

		std::vector<state::CardRef> GetDefenders() {
			std::vector<state::CardRef> defenders;
			auto const& player = state_.GetBoard().Get(state_.GetCurrentPlayerId().Opposite());

			state::CardRef hero_ref = player.GetHeroRef();
			defenders.push_back(hero_ref);

			player.minions_.ForEach([&](state::CardRef card_ref) {
				defenders.push_back(card_ref);
			});

			return defenders;
		}

		bool HeroPowerUsable() {
			state::CardRef hero_power_ref = state_.GetCurrentPlayer().GetHeroPowerRef();
			if (!hero_power_ref.IsValid()) return false;
			if (!state_.GetCard(hero_power_ref).GetRawData().usable) return false;

			// TODO: need to consider the cost_health_instead flag
			int resource = state_.GetCurrentPlayer().GetResource().GetCurrent();
			if (state_.GetCard(hero_power_ref).GetCost() > resource) return false;
			
			return true;
		}
		
	private:
		state::State const& state_;
	};
}
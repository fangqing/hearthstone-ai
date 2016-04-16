#pragma once

#include "random/random-generator.h"
#include "hand.h"

namespace GameEngine {
	inline Hand::Hand(RandomGenerator & random_generator) :
		random_generator(random_generator)
	{
		this->cards.reserve(10);
		this->deck_cards.reserve(36);
	}

	inline Hand::Hand(RandomGenerator & random_generator, Hand const& rhs) :
		random_generator(random_generator), cards(rhs.cards), deck_cards(rhs.deck_cards), hidden_cards(rhs.hidden_cards)
	{
	}

	inline Hand::Hand(RandomGenerator & random_generator, Hand && rhs) :
		random_generator(random_generator),
		cards(std::move(rhs.cards)), deck_cards(std::move(rhs.deck_cards)), hidden_cards(std::move(rhs.hidden_cards))
	{
	}

	inline Hand & Hand::operator=(Hand && rhs)
	{
		this->cards = std::move(rhs.cards);
		this->deck_cards = std::move(rhs.deck_cards);
		this->hidden_cards = std::move(rhs.hidden_cards);
		return *this;
	}

	inline void Hand::AddDeterminedCard(Card const & card)
	{
		HandCard hand_card;
		hand_card.type = HandCard::TYPE_DETERMINED;
		hand_card.card = card;
		this->cards.push_back(hand_card);
	}

	inline bool Hand::HasCardToDraw() const
	{
		return this->deck_cards.empty();
	}

	inline void Hand::DrawOneCardToHand()
	{
		HandCard hand_card;
		hand_card.type = HandCard::TYPE_DRAW_FROM_HIDDEN_CARDS;
		hand_card.card = this->DrawOneCardFromDeck();
		this->cards.push_back(hand_card);
	}

	inline Card Hand::DrawOneCardAndDiscard()
	{
		Card card = this->DrawOneCardFromDeck();

		auto it = this->hidden_cards.find(card.id);
		if (it == this->hidden_cards.end()) throw std::runtime_error("consistency check failed: cannot find the draw card from hidden cards");
		this->hidden_cards.erase(it);

		return card;
	}

	inline void Hand::DiscardHandCard(Locator idx)
	{
		auto it = this->cards.begin() + idx;
		this->cards.erase(it);
		// TODO: trigger hooks
	}

	inline Card Hand::DrawOneCardFromDeck()
	{
		Card ret;
		size_t deck_count = this->deck_cards.size();

		if (UNLIKELY(deck_count == 0)) {
			throw std::runtime_error("no card to draw");
		}

		const int rand_idx = this->random_generator.GetRandom((int)deck_count);

		ret = this->deck_cards[rand_idx];

		this->deck_cards[rand_idx] = this->deck_cards.back();
		this->deck_cards.pop_back();

		return ret;
	}

	inline Card const & Hand::GetCard(Locator idx, bool & is_determined) const
	{
		auto const& card = *(this->cards.begin() + idx);
		is_determined = card.type == HandCard::TYPE_DETERMINED;
		return card.card;
	}

	inline Card const & Hand::GetCard(Locator idx) const
	{
		return (this->cards.begin() + idx)->card;
	}

	inline void Hand::RemoveCard(Locator idx)
	{
		std::vector<HandCard>::iterator it = this->cards.begin() + idx;
		this->cards.erase(it);
	}

	inline size_t Hand::GetCount() const
	{
		return this->cards.size();
	}

	inline void Hand::AddCardToDeck(Card const & card)
	{
		// Note: This operation should INVALIDATE all the hand cards with type TYPE_DRAW_FROM_HIDDEN_CARDS
		// Since the probability distribution differs due to a new card in deck
		// However, in a normal play, we didn't add a lot cards to deck
		// so we don't invalidate the existing hand cards

		this->deck_cards.push_back(card);
		this->hidden_cards.insert(card.id);
	}

	inline bool Hand::operator==(const Hand &rhs) const
	{
		// special logic to compare different kinds of hand cards
		if (this->cards.size() != rhs.cards.size()) return false;
		for (size_t i = 0; i < this->cards.size(); ++i) {
			auto const& lhs_card = this->cards[i];
			auto const& rhs_card = rhs.cards[i];

			if (lhs_card.type != rhs_card.type) return false;

			switch (lhs_card.type) {
			case HandCard::TYPE_DETERMINED:
				if (lhs_card.card != rhs_card.card) return false;
				break;

			case HandCard::TYPE_DRAW_FROM_HIDDEN_CARDS:
				// no need to compare card id
				break;

			default:
				throw std::runtime_error("invalid hand card type");
			}
		}

		// check hidden cards
		if (this->hidden_cards != rhs.hidden_cards) return false;

		// don't compare deck_cards; all the necessary decision information is already in hidden_cards

		return true;
	}

	inline bool Hand::operator!=(Hand const& rhs) const
	{
		return !(*this == rhs);
	}

	inline void Hand::ShuffleHiddenInformation(std::function<int()> random_generator)
	{
		// TODO: we swap hand card with deck card
		//       but there might be some enchantments applied on the hand card
		//       how to record this information?
		// Idea 1: record hand card enchantment with...
		//      (1) which turn the hand card starts to be held in hand?
		//      (2) enchantments records the turn it starts to take effect.

		// Do a random permutation for hand cards with 'TYPE_DRAW_FROM_HIDDEN_CARDS'
		std::vector<int> hidden_hand_card_map;
		hidden_hand_card_map.reserve(this->cards.size());
		for (size_t i = 0; i < this->cards.size(); ++i) {
			auto const& card = this->cards[i];
			if (card.type == HandCard::TYPE_DRAW_FROM_HIDDEN_CARDS) {
				hidden_hand_card_map.push_back(i);
			}
		}
		
		// do random permutation (only for hand cards; since deck cards will always be drawn randomly)
		for (int i = 0; i < hidden_hand_card_map.size(); ++i) {
			int r = random_generator() % (hidden_hand_card_map.size() + this->deck_cards.size() - i) + i;

			if (r < hidden_hand_card_map.size()) {
				// swap with another hand card
				std::iter_swap(this->cards.begin() + hidden_hand_card_map[i], this->cards.begin() + hidden_hand_card_map[r]);
				continue;
			}
			r -= hidden_hand_card_map.size();

			// swap with deck card
			std::swap(this->cards[hidden_hand_card_map[i]].card, this->deck_cards[r]);
		}
	}

	inline std::string Hand::GetDebugString() const
	{
		std::string result;

		result += "Deck: ";
		for (const auto &deck_card : this->deck_cards) {
			result += deck_card.GetDebugString() + " ";
		}
		result += "\n";

		result += "Hand: ";
		for (const auto &card : this->cards) {
			switch (card.type)
			{
			case HandCard::TYPE_DETERMINED:
				result += "[D]";
				break;
			case HandCard::TYPE_DRAW_FROM_HIDDEN_CARDS:
				result += "[H]";
				break;
			}
			result += card.card.GetDebugString() + " ";
		}

		return result;
	}

} // namespace GameEngine

namespace std {

	template <> struct hash<GameEngine::Hand> {
		typedef GameEngine::Hand argument_type;
		typedef std::size_t result_type;
		result_type operator()(const argument_type &s) const {
			result_type result = 0;

			for (auto card : s.cards) {
				GameEngine::hash_combine(result, card.type);

				switch (card.type)
				{
				case GameEngine::HandCard::TYPE_DETERMINED:
					GameEngine::hash_combine(result, card.card);
					break;

				case GameEngine::HandCard::TYPE_DRAW_FROM_HIDDEN_CARDS:
					// no need to check card id
					break;

				default:
					throw std::runtime_error("invalid hand card type");
				}
			}

			for (auto const& hidden_card : s.hidden_cards)
			{
				GameEngine::hash_combine(result, hidden_card);
			}

			// don't compare deck_cards; all the necessary decision information is already in hidden_cards

			return result;
		}
	};
}
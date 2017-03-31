#include <assert.h>
#include <iostream>

#include "FlowControl/FlowController.h"
#include "FlowControl/FlowController-impl.h"

class Test4_ActionParameterGetter : public FlowControl::IActionParameterGetter
{
public:
	int GetMinionPutLocation(int min, int max)
	{
		return next_minion_put_location;
	}

	state::CardRef GetSpecifiedTarget(state::State & state, state::CardRef card_ref, const state::Cards::Card & card, std::vector<state::CardRef> const& targets)
	{
		assert(targets.size() == next_specified_target_count);

		if (next_specified_target_idx < 0) return state::CardRef();
		else return targets[next_specified_target_idx];
	}

	int next_minion_put_location;

	int next_specified_target_count;
	int next_specified_target_idx;
};

class Test4_RandomGenerator : public state::IRandomGenerator
{
public:
	Test4_RandomGenerator() :called_times(0), next_rand(0) {}

	int Get(int exclusive_max)
	{
		++called_times;
		return next_rand;
	}

	size_t Get(size_t exclusive_max) { return (size_t)Get((int)exclusive_max); }

	int Get(int min, int max)
	{
		++called_times;
		return min + next_rand;
	}

public:
	int called_times;
	int next_rand;
};

static void CheckZoneAndPosition(const state::State & state, state::CardRef ref, state::PlayerIdentifier player, state::CardZone zone, int pos)
{
	auto & item = state.GetCardsManager().Get(ref);
	assert(item.GetPlayerIdentifier() == player);
	assert(item.GetZone() == zone);
	assert(item.GetZonePosition() == pos);
}

static state::Cards::Card CreateDeckCard(Cards::CardId id, state::State & state, state::PlayerIdentifier player)
{
	state::Cards::CardData raw_card = Cards::CardDispatcher::CreateInstance(id);
	raw_card.enchanted_states.player = player;
	raw_card.zone = state::kCardZoneNewlyCreated;
	raw_card.enchantment_handler.SetOriginalStates(raw_card.enchanted_states);

	return state::Cards::Card(raw_card);
}

static state::CardRef PushBackDeckCard(Cards::CardId id, FlowControl::FlowContext & flow_context, state::State & state, state::PlayerIdentifier player)
{
	int deck_count = (int)state.GetBoard().Get(player).deck_.Size();

	((Test4_RandomGenerator&)(flow_context.GetRandom())).next_rand = deck_count;
	((Test4_RandomGenerator&)(flow_context.GetRandom())).called_times = 0;

	auto ref = state.AddCard(CreateDeckCard(id, state, player));
	state.GetZoneChanger<state::kCardZoneNewlyCreated>(FlowControl::Manipulate(state, flow_context), ref)
		.ChangeTo<state::kCardZoneDeck>(player);

	if (deck_count > 0) assert(((Test4_RandomGenerator&)(flow_context.GetRandom())).called_times > 0);
	++deck_count;

	assert(state.GetBoard().Get(player).deck_.Size() == deck_count);
	assert(state.GetCardsManager().Get(ref).GetCardId() == id);
	assert(state.GetCardsManager().Get(ref).GetPlayerIdentifier() == player);
	assert(state.GetCardsManager().Get(ref).GetZone() == state::kCardZoneDeck);

	return ref;
}

static state::Cards::Card CreateHandCard(Cards::CardId id, state::State & state, state::PlayerIdentifier player)
{
	state::Cards::CardData raw_card = Cards::CardDispatcher::CreateInstance(id);

	raw_card.enchanted_states.player = player;
	raw_card.zone = state::kCardZoneNewlyCreated;
	raw_card.enchantment_handler.SetOriginalStates(raw_card.enchanted_states);

	return state::Cards::Card(raw_card);
}

static state::CardRef AddHandCard(Cards::CardId id, FlowControl::FlowContext & flow_context, state::State & state, state::PlayerIdentifier player)
{
	int hand_count = (int)state.GetBoard().Get(player).hand_.Size();

	auto ref = state.AddCard(CreateHandCard(id, state, player));
	state.GetZoneChanger<state::kCardZoneNewlyCreated>(FlowControl::Manipulate(state, flow_context), ref)
		.ChangeTo<state::kCardZoneHand>(player);

	assert(state.GetCardsManager().Get(ref).GetCardId() == id);
	assert(state.GetCardsManager().Get(ref).GetPlayerIdentifier() == player);
	if (hand_count == 10) {
		assert(state.GetBoard().Get(player).hand_.Size() == 10);
		assert(state.GetCardsManager().Get(ref).GetZone() == state::kCardZoneGraveyard);
	}
	else {
		++hand_count;
		assert(state.GetBoard().Get(player).hand_.Size() == hand_count);
		assert(state.GetBoard().Get(player).hand_.Get(hand_count - 1) == ref);
		assert(state.GetCardsManager().Get(ref).GetZone() == state::kCardZoneHand);
		assert(state.GetCardsManager().Get(ref).GetZonePosition() == (hand_count - 1));
	}

	return ref;
}

static void MakeHand(state::State & state, FlowControl::FlowContext & flow_context, state::PlayerIdentifier player)
{
	AddHandCard(Cards::ID_CS2_141, flow_context, state, player);
}

static void MakeHero(state::State & state, FlowControl::FlowContext & flow_context, state::PlayerIdentifier player)
{
	state::Cards::CardData raw_card;
	raw_card.card_id = 8;
	raw_card.card_type = state::kCardTypeHero;
	raw_card.zone = state::kCardZoneNewlyCreated;
	raw_card.enchanted_states.max_hp = 30;
	raw_card.enchanted_states.player = player;
	raw_card.enchanted_states.attack = 0;
	raw_card.enchantment_handler.SetOriginalStates(raw_card.enchanted_states);

	state::CardRef ref = state.AddCard(state::Cards::Card(raw_card));

	state.GetZoneChanger<state::kCardZoneNewlyCreated>(FlowControl::Manipulate(state, flow_context), ref)
		.ChangeTo<state::kCardZonePlay>(player);


	auto hero_power = Cards::CardDispatcher::CreateInstance(Cards::ID_CS1h_001);
	assert(hero_power.card_type == state::kCardTypeHeroPower);
	hero_power.zone = state::kCardZoneNewlyCreated;
	ref = state.AddCard(state::Cards::Card(hero_power));
	state.GetZoneChanger<state::kCardZoneNewlyCreated>(FlowControl::Manipulate(state, flow_context), ref)
		.ChangeTo<state::kCardZonePlay>(player);
}

struct MinionCheckStats
{
	int attack;
	int hp;
	int max_hp;
};

static void CheckMinion(state::State &state, state::CardRef ref, MinionCheckStats const& stats)
{
	assert(state.GetCardsManager().Get(ref).GetAttack() == stats.attack);
	assert(state.GetCardsManager().Get(ref).GetMaxHP() == stats.max_hp);
	assert(state.GetCardsManager().Get(ref).GetHP() == stats.hp);
}

static void CheckMinions(state::State & state, state::PlayerIdentifier player, std::vector<MinionCheckStats> const& checking)
{
	std::vector<state::CardRef> const& minions = state.GetBoard().Get(player).minions_.Get();

	assert(minions.size() == checking.size());
	for (size_t i = 0; i < minions.size(); ++i) {
		CheckMinion(state, minions[i], checking[i]);
	}
}

struct CrystalCheckStats
{
	int current;
	int total;
};
static void CheckCrystals(state::State & state, state::PlayerIdentifier player, CrystalCheckStats checking)
{
	assert(state.GetBoard().Get(player).GetResource().GetCurrent() == checking.current);
	assert(state.GetBoard().Get(player).GetResource().GetTotal() == checking.total);
}

static void CheckHero(state::State & state, state::PlayerIdentifier player, int hp, int armor, int attack)
{
	auto hero_ref = state.GetBoard().Get(player).GetHeroRef();
	auto const& hero = state.GetCardsManager().Get(hero_ref);

	assert(hero.GetHP() == hp);
	assert(hero.GetArmor() == armor);
	assert(hero.GetAttack() == attack);
}

void test4()
{
	Test4_ActionParameterGetter parameter_getter;
	Test4_RandomGenerator random;
	state::State state;
	FlowControl::FlowContext flow_context(random, parameter_getter);

	FlowControl::FlowController controller(state, flow_context);

	MakeHero(state, flow_context, state::PlayerIdentifier::First());
	MakeHand(state, flow_context, state::PlayerIdentifier::First());

	MakeHero(state, flow_context, state::PlayerIdentifier::Second());
	MakeHand(state, flow_context, state::PlayerIdentifier::Second());

	state.GetMutableCurrentPlayerId().SetFirst();
	state.GetBoard().GetFirst().GetResource().SetTotal(10);
	state.GetBoard().GetFirst().GetResource().Refill();
	state.GetBoard().GetSecond().GetResource().SetTotal(10);
	state.GetBoard().GetSecond().GetResource().Refill();

	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), {});
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);


	struct TestBase
	{
		state::State state;
		FlowControl::FlowContext flow_context;
		Test4_ActionParameterGetter & parameter_getter;
		Test4_RandomGenerator & random;
	} base{ state, flow_context, parameter_getter, random };

	[state, flow_context, &parameter_getter, &random]() mutable {
		FlowControl::FlowController controller(state, flow_context);

		state.GetBoard().GetFirst().GetResource().Refill();
		state.GetBoard().GetSecond().GetResource().Refill();
		AddHandCard(Cards::ID_EX1_534, flow_context, state, state::PlayerIdentifier::First());
		parameter_getter.next_minion_put_location = 0;
		if (controller.PlayCard(1) != FlowControl::kResultNotDetermined) assert(false);
		CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
		CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
		CheckCrystals(state, state::PlayerIdentifier::First(), { 4, 10 });
		CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
		CheckMinions(state, state::PlayerIdentifier::First(), { {6, 5, 5} });
		CheckMinions(state, state::PlayerIdentifier::Second(), {});
		assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
		assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

		state.GetBoard().GetFirst().GetResource().Refill();
		state.GetBoard().GetSecond().GetResource().Refill();
		AddHandCard(Cards::ID_EX1_312, flow_context, state, state::PlayerIdentifier::First());
		parameter_getter.next_minion_put_location = 0;
		if (controller.PlayCard(1) != FlowControl::kResultNotDetermined) assert(false);
		CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
		CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
		CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 10 });
		CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
		CheckMinions(state, state::PlayerIdentifier::First(), { {2,2,2},{2,2,2} });
		CheckMinions(state, state::PlayerIdentifier::Second(), {});
		assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
		assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);
	}();

	[state, flow_context, &parameter_getter, &random]() mutable {
		FlowControl::FlowController controller(state, flow_context);

		state.GetBoard().GetFirst().GetResource().Refill();
		state.GetBoard().GetSecond().GetResource().Refill();
		AddHandCard(Cards::ID_EX1_534, flow_context, state, state::PlayerIdentifier::First());
		parameter_getter.next_minion_put_location = 0;
		if (controller.PlayCard(1) != FlowControl::kResultNotDetermined) assert(false);
		CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
		CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
		CheckCrystals(state, state::PlayerIdentifier::First(), { 4, 10 });
		CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
		CheckMinions(state, state::PlayerIdentifier::First(), { { 6, 5, 5 } });
		CheckMinions(state, state::PlayerIdentifier::Second(), {});
		assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
		assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

		state.GetBoard().GetFirst().GetResource().Refill();
		state.GetBoard().GetSecond().GetResource().Refill();
		AddHandCard(Cards::ID_EX1_312, flow_context, state, state::PlayerIdentifier::First());
		parameter_getter.next_minion_put_location = 0;
		if (controller.PlayCard(1) != FlowControl::kResultNotDetermined) assert(false);
		CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
		CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
		CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 10 });
		CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
		CheckMinions(state, state::PlayerIdentifier::First(), { { 2,2,2 },{ 2,2,2 } });
		CheckMinions(state, state::PlayerIdentifier::Second(), {});
		assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
		assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);
	}();
}
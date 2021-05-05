#ifndef LOCAL
#pragma GCC optimize ("Ofast,inline,omit-frame-pointer")
#endif

#include <iostream>
#include <string.h>
#include <vector>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <cmath>

#include <bitset>

/*      whoami C stuff      */
#include <stdlib.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
/////////////////////

using namespace std;
using namespace chrono;

static const int MAX_MANA = 12;
static const int MAX_CREATURES_IN_PLAY = 6;

void trap()
{
    exit(EXIT_FAILURE);
    // *(int*)0 = 0;
}

int getPlayer()
{
    register struct passwd *pw;
    register uid_t uid;
    int c;

    uid = geteuid();
    pw = getpwuid(uid);
    if (!pw) throw std::logic_error("whoami error");

    char* user = pw->pw_name;
    int i = 0;
    while (user[i] != '\0')++i;

    char ch = user[i-1];
    if (ch != '1' && ch != '2') throw std::logic_error("whoami implimentation error");

    return (ch == '1' ? 1 : 2);
}

struct Random
{   
    uint32_t state;

    Random()
    {
        state = (uint32_t)time(0);
        // state = 12345;
    }

    uint32_t getRandom()
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    int getRandomInt(int bound)
    {
        return getRandom() % bound;  // not best way btw
    }
};

struct Timeout
{
    void start();
    bool isElapsed(double maxTimeSeconds);

    high_resolution_clock::time_point startTime;

};

void Timeout::start()
{
    startTime = high_resolution_clock::now();
}

bool Timeout::isElapsed(double maxTimeSeconds)
{
    duration<double> time_span = duration_cast<duration<double>>(high_resolution_clock::now() - startTime);
    return time_span.count() >= maxTimeSeconds;
}

static const char* g_encodingTable = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
static char g_decodingTable[256] = {0};
static bool g_decodingInit = false;
// Base64
struct BitStream
{
    static const int MAX_SIZE = 512;

    BitStream();

    void    initRead(const char* str);

    void    incBitCount();

    bool    readBit();
    void    writeBit(bool value);
    int     readInt(int bits);
    void    writeInt(int value, int bits);

    void    encode();
    void    decode(int count);
    void    print(ostream& os);

    char    buffer[MAX_SIZE] {0};
    int     iter = 0;
    int     bitCount = 0;
};


BitStream::BitStream()
{
    if (g_decodingInit) return;
    for (int i = 0; i < 64; ++i)
    {
        g_decodingTable[g_encodingTable[i]] = i;
    }
    g_decodingInit = true;
}

void BitStream::initRead(const char* str)
{
    int count = strlen(str);
    // cerr << "count: " << count << endl;
    strcpy(buffer, str);
    // cerr << "buffer: " << buffer << endl;
    decode(count);
    iter = bitCount = 0;
}

void BitStream::incBitCount()
{
    bitCount++;
    if (bitCount >= 6)
    {
        iter++;
        if (iter >= MAX_SIZE) 
        {
            cerr << "BitStream buffer is full\n";
            trap();
        }
        bitCount = 0;
    }
}

bool BitStream::readBit()
{
    auto& c = buffer[iter];
    bool value = c & (1 << (5-bitCount));
    incBitCount();
    return value;

}

void BitStream::writeBit(bool value)
{
    auto& c = buffer[iter];
    c <<= 1;
    if (value) c |= 1;
    incBitCount();
}

int BitStream::readInt(int bits)
{
    bool negative = readBit();
    int result = 0;
    for (int i = 0; i < bits; ++i)
    {
        result <<= 1;
        if (readBit()) result |= 1;
    }
    return negative? -result : result;
}

void BitStream::writeInt(int value, int bits)
{
    writeBit(value < 0);
    value = abs(value);
    int mask = 1 << (bits-1);
    for (int i = 0; i < bits; ++i)
    {
        writeBit(value & mask);
        mask >>= 1;
    }
}

void BitStream::encode()
{
    while (bitCount != 0) writeBit(false);
    
    for (int i = 0; i < iter; ++i)
    {
        char c = buffer[i];
        buffer[i] = g_encodingTable[c];
    }
}

void BitStream::decode(int count)
{
    for (int i = 0; i < count; ++i)
    {
        char c = buffer[i];
        buffer[i] = g_decodingTable[c];
    }
}

void BitStream::print(ostream& os)
{
    os << buffer;
}

struct Player
{
    int hp;
    int mana;
    int cardsRemaining;
    int rune;
    int draw;

    int cardsDrawn = 0;

    void read(BitStream& bs);
    void write(BitStream& bs);
};

void Player::read(BitStream& bs)
{
    hp = bs.readInt(7);
    mana = bs.readInt(4);
    cardsRemaining = bs.readInt(6);
    rune = bs.readInt(5);
}

void Player::write(BitStream& bs)
{
    bs.writeInt(hp, 7);
    bs.writeInt(mana, 4);
    bs.writeInt(cardsRemaining, 6);
    bs.writeInt(rune, 5);
}

enum class CardLocation
{
    Opponent = -1,
    InHand = 0,
    Mine = 1,
    OutOfPlay = 2,
};

enum class CardType
{
    Creature,
    GreenItem,
    RedItem,
    BlueItem
};

enum Ability
{
    None = 0,
    Breakthorough = 1 << 0,
    Charge = 1 << 1,
    Guard = 1 << 2,
    Drain = 1 << 3,
    Lethal = 1 << 4,
    Ward = 1 << 5
};

struct Card
{
    int id;
    int idx;
    int cardID;
    CardLocation location;
    CardType cardType;
    int cost;
    int attack;
    int defense;
    int hpChange;
    int hpChangeEnemy;
    int cardDraw;

    unsigned int abilities;


    bool canAttack;

    bool used = false;

    void read(BitStream& bs);
    void write(BitStream& bs);
};

void Card::read(BitStream& bs)
{
    id = bs.readInt(8);
    cardID = bs.readInt(8);
    location = (CardLocation)bs.readInt(2);
    cardType = (CardType)bs.readInt(2);
    cost = bs.readInt(4);
    attack = bs.readInt(4);
    defense = bs.readInt(4);
    hpChange = bs.readInt(4);
    hpChangeEnemy = bs.readInt(4);
    cardDraw = bs.readInt(3);
    abilities = bs.readInt(6);
}

void Card::write(BitStream& bs)
{
    bs.writeInt(id, 8);
    bs.writeInt(cardID, 8);
    bs.writeInt((int)location, 2);
    bs.writeInt((int)cardType, 2);
    bs.writeInt(cost, 4);
    bs.writeInt(attack, 4);
    bs.writeInt(defense, 4);
    bs.writeInt(hpChange, 4);
    bs.writeInt(hpChangeEnemy, 4);
    bs.writeInt(cardDraw, 3);
    bs.writeInt(abilities, 6);
}

enum class ActionType
{
    Pass,
    Summon,
    Attack,
    Use,
    Pick
};

struct State;

struct Action
{
    ActionType type = ActionType::Pass;
    int idx = -1;
    int idxTarget = -1;

    void pass();
    void summon(int idx);
    void attack(int idx, int targetIdx = -1);
    void use(int idx, int targetIdx = -1);
    void pick(int idx);
    void print(ostream& os, State&);
};

struct Turn
{
    vector<Action> actions;

    void clear();
    bool isCardPlayer(int id);
    Action& newAction();

    void print(ostream& os, State& state);
};

struct State
{
    Player players[2];
    int opponentHand;
    int opponentActions;
    vector<Card> cards;
    vector<int> creatureIdxs[2];

    bool isInDraft();

    void read(BitStream& bs);
    void write(BitStream& bs);

    void removeCard(Card& card);
    void dealDamage(Card& card, int amount);
    void dealLethal(Card& card);
    void doAttack(Card& card, Card& cardTarget);

    void generateActions(vector<Action>& actions, int playerIdx);

    void applyGlobalEffects(Card& card, int playerIdx);

    template <bool testValidity>
    void summon(Action& action, int playerIdx);
    template <bool testValidity>
    void use(Action& action, int playerIdx);
    template <bool testValidity>
    void attack(Action& action, int playerIdx);

    template <bool testValidity>
    void update(Action& action, int playerIdx);

    template <bool testValidity>
    void update(Turn& action, int playerIdx);

    void print();
};

void State::removeCard(Card& card)
{
    int playerIdx = card.location == CardLocation::Mine ? 0 : 1;
    auto& creatures = creatureIdxs[playerIdx];
    auto it = find(creatures.begin(), creatures.end(), card.idx);
    if (it != creatures.end())
        creatures.erase(it);
    else
    {
        cerr << "Cannot find creature that's being removed.\n";
        trap();
    }
    card.location = CardLocation::OutOfPlay;

}

void State::dealDamage(Card& card, int amount)
{
    if (amount <= 0) return;
    if (card.abilities & Ward)
    {
        card.abilities != ~Ward;
        return;
    }
    
    card.defense -= amount;
    if (card.defense <= 0)
    {
        removeCard(card);
    }
}

void State::dealLethal(Card& card)
{
    if (card.abilities & Ward)
        card.abilities &= ~Ward;
    else
        removeCard(card);  
}

void State::doAttack(Card& card, Card& cardTarget)
{
    if (card.abilities & Lethal)
        dealLethal(cardTarget);
    else
        dealDamage(cardTarget, card.attack);
}

bool State::isInDraft()
{
    return (players[0].mana == 0); 
}

void State::read(BitStream& bs)
{
    for (int i = 0; i < 2; ++i)
    {
        players[0].read(bs);
    }
    opponentHand = bs.readInt(3);
    opponentActions = bs.readInt(6);

    cards.clear();
    int cardCound = bs.readInt(8);
    for (int i = 0; i < cardCound; ++i)
    {
        cards.emplace_back();
        Card& card = cards.back();
        card.idx = i;
        card.read(bs);

        bool inBoard = card.location == CardLocation::Mine || card.location == CardLocation::Opponent;
        if (card.cardType == CardType::Creature && inBoard)
            creatureIdxs[card.location == CardLocation::Mine ? 0 : 1].push_back(card.idx);
        card.canAttack = inBoard;
    }
}

void State::write(BitStream& bs)
{
    for (int i = 0; i < 2; ++i)
    {
        players[0].write(bs);
    }
    bs.writeInt(opponentHand, 3);
    bs.writeInt(opponentActions, 6);


    bs.writeInt((int)cards.size(), 8);
    for (Card& card : cards)
    {
        card.write(bs);
    }
}

void State::generateActions(vector<Action>& actions, int playerIdx)
{
    Player& myPlayer = players[playerIdx];
    CardLocation myLocation = playerIdx == 0 ? CardLocation::Mine : CardLocation::Opponent;
    for (Card& card : cards)
    {
        if (playerIdx == 0 && card.location == CardLocation::InHand)
        {
            if (card.cost > myPlayer.mana) continue;
            if (card.cardType == CardType::Creature)
            {
                if (creatureIdxs[playerIdx].size() >= MAX_CREATURES_IN_PLAY) continue;
                
                actions.emplace_back();
                Action& action = actions.back();
                action.summon(card.idx);
            }
            else
            {
                if (card.cardType == CardType::BlueItem && card.defense == 0)
                {
                    actions.emplace_back();
                    Action& action = actions.back();
                    action.use(card.idx);
                }
                else
                {
                    auto& creatures = creatureIdxs[card.cardType == CardType::GreenItem ? playerIdx : (1-playerIdx)];
                    for (int creatureIdx : creatures)
                    {
                        actions.emplace_back();
                        Action& action = actions.back();
                        action.use(card.idx, creatureIdx);
                    }
                }
            } 
        }
        else if (card.location == myLocation && card.canAttack)
        {
            bool foundsGuard = false;
            for (int creatureIdx : creatureIdxs[1 - playerIdx])
            {
                Card& creature = cards[creatureIdx];
                if (!(creature.abilities & Guard)) continue;

                foundsGuard = true;
                actions.emplace_back();
                Action& action = actions.back();
                action.attack(card.idx, creatureIdx);
            }
            if (!foundsGuard)
            {
                for (int creatureIdx : creatureIdxs[1 - playerIdx])
                {
                    Card& creature = cards[creatureIdx];
                    actions.emplace_back();
                    Action& action = actions.back();
                    action.attack(card.idx, creatureIdx);
                }

                
                actions.emplace_back();
                Action& action = actions.back();
                action.attack(card.idx);
            }
        }
    }
}

void State::applyGlobalEffects(Card& card, int playerIdx)
{
    Player& myPlayer = players[playerIdx];
    Player& opponentPlayer = players[1-playerIdx];
    myPlayer.mana -= card.cost;
    myPlayer.hp += card.hpChange; // changed to hpChange to apply hpChange to myPlayer
    opponentPlayer.hp += card.hpChangeEnemy;
    myPlayer.cardsDrawn += card.cardDraw;
}

template <bool testValidity>
void State::summon(Action& action, int playerIdx)
{
    Player& myPlayer = players[playerIdx];

    Card& card = cards[action.idx];
    if (testValidity)
    {
        if (card.cardType != CardType::Creature)
        {
            cerr << "Attempted to summon a non-creature card" << endl;
            trap();
        }

        if (card.cost > myPlayer.mana)
        {
            cerr << "Attempted to summon a card with not enough mana" << endl;
            trap();
        }

        if (creatureIdxs[playerIdx].size() >= MAX_CREATURES_IN_PLAY)
        {
            cerr << "Attempted to summon a card on a full board" << endl;
            trap();
        }
    }

    applyGlobalEffects(card, playerIdx);

    card.location = playerIdx == 0 ? CardLocation::Mine : CardLocation::Opponent;
    card.canAttack = card.abilities & Charge;
    // myPlayer.mana -= card.cost; // danger seems like mana double cost one in applyGlobalEffects
    creatureIdxs[playerIdx].push_back(card.idx);
}

template <bool testValidity>
void State::use(Action& action, int playerIdx)
{
    Player& myPlayer = players[playerIdx];

    Card& card = cards[action.idx];
    if (testValidity)
    {
        if (card.cardType == CardType::Creature)
        {
            cerr << "Attempted to use a creature card" << endl;
            trap();
        }
        if (card.cost > myPlayer.mana)
        {
            cerr << "Attempted to use a card with not enough mana" << endl;
            trap();
        }
    }
    card.location = CardLocation::OutOfPlay;
    applyGlobalEffects(card, playerIdx);

    if (action.idxTarget != -1)
    {
        Card& cardTarget = cards[action.idxTarget];

        // Change abilities
        if (card.cardType == CardType::GreenItem)
        {
            cardTarget.abilities |= card.abilities;
        }
        else if (card.cardType == CardType::RedItem)
        {
            cardTarget.abilities &= ~card.abilities;
        }

        // Change attack/defense
        cardTarget.attack += card.attack;
        if (cardTarget.attack < 0) cardTarget.attack = 0;
        if (card.defense > 0)
            cardTarget.defense += card.defense;
        else if (card.defense < 0)
            dealDamage(cardTarget, -card.defense);
    } 
}

template <bool testValidity>
void State::attack(Action &action, int playerIdx)
{
    Player& myPlayer = players[playerIdx];
    Player& opponentPlayer = players[1-playerIdx];
    Card& card = cards[action.idx];
    if (testValidity)
    {
        if (card.location != (playerIdx == 0 ? CardLocation::Mine : CardLocation::Opponent))
        {
            cerr << "Attacking with a creature that I do not control\n";
            trap();
        }
        if (!card.canAttack)
        {
            cerr << "Attacking with a creature that cannot attack\n";
            trap();
        }

        bool foundGuard = false;
        bool attackingGuard = false;
        for (Card& card : cards)
        {
            if (card.location != (playerIdx == 0 ? CardLocation::Opponent : CardLocation::Mine)) continue;
            if (!(card.abilities & Guard)) continue;

            foundGuard = true;
            if (action.idxTarget == -1)
            {
                cerr << "Attacking the player when there is a guard on board" << endl;
                trap();
            } 

            if (card.idx == action.idxTarget)
                attackingGuard = true;
            
            // if (foundGuard && !attackingGuard)
            if (foundGuard && !(card.abilities & Guard))
            {
                cerr << "Attacking a non-guard card when there is a guard on board" << endl;
                trap();                        
            }


        }
    }

    if (action.idxTarget == -1)
    {
        opponentPlayer.hp -= card.attack;
    }
    else 
    {
        Card& cardTarget = cards[action.idxTarget];

        if ((card.abilities & Breakthorough)  && !(cardTarget.abilities & Ward))
        {
            int remainder = card.attack -= cardTarget.defense;
            if (remainder > 0)
                opponentPlayer.hp -= remainder;
        }

        if ((card.abilities & Drain) && !(cardTarget.abilities & Ward))
        {
            myPlayer.hp += card.attack;
        }

        doAttack(card, cardTarget);
        doAttack(cardTarget, card);
    }
    card.canAttack = false;
}

template <bool testValidity>
void State::update(Action& action, int PlayerIdx)
{
    if (action.type == ActionType::Summon)
    {
        summon<testValidity>(action, PlayerIdx);
    }
    else if (action.type == ActionType::Use)
    {
        use<testValidity>(action, PlayerIdx);
    }
    else if (action.type == ActionType::Attack)
    {
        attack<testValidity>(action, PlayerIdx);
    }  
}

template <bool testValidity>
void State::update(Turn& turn, int playerIdx)
{
    for (Action& action : turn.actions)
    {
        update<testValidity>(action, playerIdx);
    }
}

void State::print()
{
    const char* abilitiesStr = "BCGDLW";
    cerr << "Players hp: " << players[0].hp << " " << players[1].hp << endl;
    for (Card& card : cards)
    {
        if (card.location == CardLocation::OutOfPlay) continue;
        if (card.location == CardLocation::InHand) cerr << "InHand ";
        else if (card.location == CardLocation::Mine) cerr << "Mine ";
        else if (card.location == CardLocation::Opponent) cerr << "Opponent ";
        cerr << card.id << " " << card.attack << " " << card.defense << " ";
        for (int i = 0, mask = Breakthorough; i < 6; ++i, mask <<= 1)
        {
            if (card.abilities & mask)
                cerr << abilitiesStr[i];
            else
                cerr << "-";
        }
        cerr << endl;
    }
}


void Action::pass()
{
    type = ActionType::Pass;
}

void Action::summon(int _idx)
{
    type = ActionType::Summon;
    idx = _idx;
}

void Action::attack(int _idx, int _targetId)
{
    type = ActionType::Attack;
    idx = _idx;
    idxTarget = _targetId;
}

void Action::use(int _idx, int _targetId)
{
    type = ActionType::Use;
    idx = _idx;
    idxTarget = _targetId;
}

void Action::pick(int _idx)
{
    type = ActionType::Pick;
    idx = _idx;
}

void Action::print(ostream& os, State& state)
{
    if (type == ActionType::Pass)
    {
        os << "PASS";
    }
    else if (type == ActionType::Summon)
    {
        Card& card = state.cards[idx];
        os << "SUMMON " << card.id;
    }
    else if (type == ActionType::Attack)
    {
        Card& card = state.cards[idx];
        // Card& cardTarget = state.cards[idxTarget];
        os << "ATTACK " << card.id << " " << (idxTarget > 0 ? state.cards[idxTarget].id : idxTarget);//cardTarget.id;
    }
    else if (type == ActionType::Use)
    {
        Card& card = state.cards[idx];
        // Card& cardTarget = state.cards[idxTarget];
        os << "USE " << card.id << " " << (idxTarget > 0 ? state.cards[idxTarget].id : idxTarget);
    }
    else if (type == ActionType::Pick)
    {
        os << "PICK " << idx;
    }
    else
    {
        cerr << "Action not found: " << (int)type << endl;
        trap();
    }
}


Action& Turn::newAction()
{
    actions.emplace_back();
    return actions.back();
}

void Turn::clear()
{
    actions.clear();
}

bool Turn::isCardPlayer(int id)
{
    for (Action& action : actions)
    {
        if (!(action.type == ActionType::Summon || action.type == ActionType::Use)) continue;
        if (action.idx == id) return true;
    }
    return false;
}

void Turn::print(ostream& os, State& state)
{
    if (actions.size() == 0)
    {
        os << "PASS";
        return;
    }

    bool first = true;
    for (auto& action : actions)
    {
        if (!first) os << ";";
        first = false;
        action.print(os, state);
    }
}

vector<int> cardRate = {68, 7, 65, 49, 116, 69, 151, 48, 53, 51, 44, 67, 29, 139, 84, 18, 158, 28, 64, 80, 33, 85,
32, 147, 103, 37, 54, 52, 50, 99, 23, 87, 66, 81, 148, 88, 150, 121, 82, 95, 115, 133, 152,
19, 109, 157, 105, 3, 75, 96, 114, 9, 106, 144, 129, 17, 111, 128, 12, 11, 145, 15, 21, 8,
134, 155, 141, 70, 90, 135, 104, 41, 112, 61, 5, 97, 26, 34, 73, 6, 36, 86, 77, 83, 13, 89,
79, 93, 149, 59, 159, 74, 94, 38, 98, 126, 39, 30, 137, 100, 62, 122, 22, 72, 118, 1, 47, 71,
4, 91, 27, 56, 119, 101, 45, 16, 146, 58, 120, 142, 127, 25, 108, 132, 40, 14, 76, 125, 102,
131, 123, 2, 35, 130, 107, 43, 63, 31, 138, 124, 154, 78, 46, 24, 10, 136, 113, 60, 57, 92,
117, 42, 55, 153, 20, 156, 143, 110, 160, 140};


struct ManaCurve
{
    int curve[MAX_MANA+1];

    int evalScore();
    void compute(vector<Card>& cards);
    void print(ostream& os);
};

int ManaCurve::evalScore()
{
    int lowerCount = 0;
    int lowCount = 0;
    int medCount = 0;
    int hiCount = 0;
    int laCount = 0;

    
    for (int i = 1; i <= 3; ++i) lowCount += curve[i];
    for (int i = 4; i <= 5; ++i) medCount += curve[i];
    for (int i = 6; i <= 8; ++i) hiCount += curve[i];

    return (abs(lowCount-15) + abs(medCount-10) + abs(hiCount-5))*5;
}

void ManaCurve::compute(vector<Card>& cards)
{
    fill(curve, curve+MAX_MANA+1, 0);
    for (Card& card : cards)
        curve[card.cost]++;
}

void ManaCurve::print(ostream& os)
{
    for (int i = 0; i <= MAX_MANA; ++i)
        os << "Mana " << i << " = " << curve[i] << " card" << endl;
}

struct Agent
{
    Random rnd;
    Timeout timeout;
    State state;
    Turn bestTurn;
    vector<Card> draftedCards;

    bool getRandomAction(State& state, Action& action, int playerIdx);

    void read();
    bool findBestPair(vector<Card*>& targets, Card*& myCard, Card*& enemyCard);
    float evalScore(const State& stat);
    void think();
    void print(); 
};

bool Agent::getRandomAction(State& state, Action& action, int playerIdx)
{
    vector<Action> actions;
    state.generateActions(actions, playerIdx);
    if (actions.empty()) return false;
    int actionIdx = rnd.getRandomInt((int)actions.size());
    action = actions[actionIdx];
    return true;
}

void Agent::read()
{
    for (int i = 0; i < 2; i++) 
        {
            Player& player = state.players[i];

            int playerHealth, playerMana, playerDeck, playerRune, playerDraw;
            cin >> playerHealth >> playerMana >> playerDeck >> playerRune >> playerDraw; cin.ignore();
            player.hp = playerHealth;
            player.mana = playerMana;
            player.cardsRemaining = playerDeck;
            player.rune = playerRune;
            player.draw = playerDraw;

            state.creatureIdxs[i].clear();
        }

        int opponentHand;
        int opponentActions;
        cin >> opponentHand >> opponentActions; cin.ignore();
        state.opponentHand = opponentHand;
        state.opponentActions = opponentActions;

        for (int i = 0; i < opponentActions; i++) 
        {
            string cardNumberAndAction;
            getline(cin, cardNumberAndAction);
        }

        state.cards.clear();
        int cardCount;
        cin >> cardCount; cin.ignore();
        for (int i = 0; i < cardCount; i++) 
        {
            state.cards.emplace_back();
            Card& card = state.cards.back();
            card.idx = i;

            int cardNumber, instanceId, location, cardType, cost, attack, defense, myHealthChange, opponentHealthChange, cardDraw;
            string abilities;
            cin >> cardNumber >> instanceId >> location >> cardType >> cost >> attack >> defense >> abilities >> myHealthChange >> opponentHealthChange >> cardDraw; cin.ignore();
            card.cardID = cardNumber;
            card.id = instanceId;
            card.location = (CardLocation)location;
            card.cardType = (CardType)cardType;
            card.cost = cost;
            card.attack = attack;
            card.defense = defense;
            card.hpChange = myHealthChange;
            card.hpChangeEnemy = opponentHealthChange;
            card.cardDraw = cardDraw;
            card.canAttack = card.location == CardLocation::InHand? false : true;
            if (card.location == CardLocation::Opponent) card.canAttack = true;

            card.abilities = Ability::None;
            for (char c : abilities)
            {
                if (c == 'B') card.abilities |= Ability::Breakthorough;
                if (c == 'C') card.abilities |= Ability::Charge;
                if (c == 'G') card.abilities |= Ability::Guard;
                if (c == 'D') card.abilities |= Ability::Drain;
                if (c == 'L') card.abilities |= Ability::Lethal;
                if (c == 'W') card.abilities |= Ability::Ward;
            }

            if (card.cardType == CardType::Creature && (card.location == CardLocation::Mine || card.location == CardLocation::Opponent))
                state.creatureIdxs[card.location == CardLocation::Mine ? 0 : 1].push_back(card.idx);            
        }

        timeout.start();
}

float Agent::evalScore(const State& stat)
{
    if (stat.players[0].hp <= 0) return -__FLT_MAX__;
    if (stat.players[1].hp <= 0) return __FLT_MAX__;

    float score = 0;

    score += stat.players[0].hp;
    score -= stat.players[1].hp;
    
    int opDmgSum = 0;
    auto& opCreatures = stat.creatureIdxs[1];
    for (auto& opCreatureIdx : opCreatures)
    {
        auto& opCreature = stat.cards[opCreatureIdx];
        opDmgSum += opCreature.attack;
    }
    int myPredictHp = stat.players[0].hp - opDmgSum;
    if (myPredictHp <= 8)
    {
        int temp = 8 - (myPredictHp > 0? myPredictHp : 0);
        score -= (pow(temp+1, 1.7f)+1.5f)/1.5f;
    }

    // if (stat.players[0].hp <= 8)
    // {
    //     score -= 8.f - stat.players[0].hp*1.f;
    // }

    // if (stat.players[1].hp <= 8)
    // {
    //     score += 8.f - stat.players[1].hp*1.f;
    // }
    
    for (const Card& card : stat.cards)
    {
        if (card.cardType != CardType::Creature && card.location == CardLocation::InHand)
            score = score + 0.5f * card.cost;
        
        // else
        // {
        //     int abilityScore = 0;
        //     for (int i = 0, mask = Breakthorough; i < 6; ++i,mask <<=1)
        //     {
        //         if (card.abilities & mask)
        //         {
        //             abilityScore++;
        //         }
        //     }
        //     if (card.location == CardLocation::Mine)
        //         score += abilityScore;
        //     else
        //         score -= abilityScore;

        // }
        
    }



    

    for (int s = 0; s < 2; ++s)
    {
        auto& creatures = stat.creatureIdxs[s];
        
        float cSize = creatures.size();
        float creatureScore = 0;
        // float creatureScore = (pow(cSize+1, 1.7f)+1.5f)/1.5f;;
        for (int creatureIdx : creatures)
        {
            const Card& creature = stat.cards[creatureIdx];
            creatureScore += creature.attack;
            creatureScore += creature.defense;
            creatureScore += 0.9f;
            if (creature.abilities & Lethal) creatureScore += 2.1f;

        }
        creatureScore *= 1.7f;
        score += s == 0 ? creatureScore : -creatureScore;
    }

    return score;
}

void Agent::think()
{
    bestTurn.clear();

    ManaCurve curve;
    if (state.isInDraft())
    {
        int bestScore = __INT_MAX__;
        int bestPick = -1;
        for (int i = 0; i < 3 ; ++i)
        {
            Card& card = state.cards[i];
            curve.compute(draftedCards);
            int oldScore = curve.evalScore();
            curve.curve[card.cost]++;
            int score = curve.evalScore();
            curve.curve[card.cost]--;
            
            auto found = find(cardRate.begin(), cardRate.end(), card.cardID);
            int rate = found - cardRate.begin();
            score += rate;

            cerr << "i: " << i << " score: " << score << " cardCost: " << card.cost << " rate: " << rate << " oldScore: " << oldScore << endl;

            if (score < bestScore)
            {
                bestScore = score;
                bestPick = i;
            }
        }

        auto& action = bestTurn.newAction();
        action.pick(bestPick);
        draftedCards.push_back(state.cards[bestPick]);

        return;
    }

    
    float bestScore = -__FLT_MAX__;
    bestTurn.clear();
    int iters = 0;
    State bestState;
    // hight resolution clock seems a little bit expensive
    while ((iters&511) > 0 || !timeout.isElapsed(0.095))
    {
        State newState = state;
        Turn turn;
        {
            float score = evalScore(newState);
            if (score > bestScore)
            {
                bestScore = score;
                bestTurn = turn;
                bestState = newState;
            }
        }
        while (true)
        {
            // if (rnd.getRandomInt(5) == 0) break;
            Action action;
            if (!getRandomAction(newState, action, 0)) break;
            turn.actions.push_back(action);
            newState.update<true>(action, 0);

            float score = evalScore(newState);
            if (score > bestScore)
            {
                bestScore = score;
                bestTurn = turn;
                bestState = newState;
            }
        }
        
        ++iters;
    }
    cerr << "iters: " << iters << endl;
    // cerr << "bestScore : " << bestScore << endl;
    // cerr << "bestState: hp 0: " << bestState.players[0].hp << " 1: " << bestState.players[1].hp;
    // cerr << " mana 0: " << bestState.players[0].mana << endl;
    
    vector<Action> actions;
    state.generateActions(actions, 1);


}

void Agent::print()
{
    bestTurn.print(cout, state);
    cout << endl;
}

void test()
{
    BitStream bs;
    bs.initRead("4a3pn90yz0mC0QJ24280202940CSe00W2Ph0ACOm042uH08Ge00039j0AKm0003Pv140O0203ep08Ce00G19j8AKm0002Gpe8C800G312eAK800W2XMe64W0020GHe8Ge000");
    State s;
    s.read(bs);

    Agent agent;
    agent.timeout.start();
    agent.state = s;
    agent.think();
    agent.print();
}

int main()
{
    cerr << "Player numb: " << getPlayer() << endl;

#ifdef LOCAL
    test();
#endif

    Agent agent;
    while (1) 
    {
        agent.read();

        BitStream bs;
        agent.state.write(bs);
        bs.encode();
        // cerr << "rune: " << agent.state.players[0].rune << endl;
        cerr << "Bit stream: ";
        cerr << "opHp: " << agent.state.players[1].hp << endl;
        bs.print(cerr);
        cerr << endl;
        
        agent.think();
        agent.print();
    }
}
#pragma once

#include <cstddef>
#include <cstdint>

enum class BestwayProtocolFamily : uint8_t {
    Uart4Wire,
    SixWireType1,
    SixWireType2,
};

enum class BestwayModel : uint8_t {
    PRE2021,
    MIAMI2021,
    MALDIVES2021,
    M54149E,
    M54173,
    M54154,
    M54144,
    M54138,
    M54123,
};

struct BestwayModelInfo {
    BestwayModel model;
    const char *name;
    BestwayProtocolFamily family;

    bool has_air;
    bool has_jets;

    // Sommige legacy 4-wire modellen hebben een afwijkende
    // fysieke pinout. Dit is alleen metadata; hardware blijft
    // uitgeschakeld zolang simulation actief is.
    bool special_pinout;
};

inline constexpr BestwayModelInfo BESTWAY_MODELS[] = {
    { BestwayModel::PRE2021,     "PRE2021",      BestwayProtocolFamily::SixWireType1, true,  false, false },
    { BestwayModel::MIAMI2021,   "MIAMI2021",    BestwayProtocolFamily::SixWireType1, true,  false, false },
    { BestwayModel::MALDIVES2021,"MALDIVES2021", BestwayProtocolFamily::SixWireType1, true,  true,  false },

    { BestwayModel::M54149E,     "54149E",        BestwayProtocolFamily::SixWireType2, true,  false, false },

    { BestwayModel::M54173,      "54173",         BestwayProtocolFamily::Uart4Wire,    true,  true,  false },
    { BestwayModel::M54154,      "54154",         BestwayProtocolFamily::Uart4Wire,    true,  false, true  },
    { BestwayModel::M54144,      "54144",         BestwayProtocolFamily::Uart4Wire,    false, true,  false },
    { BestwayModel::M54138,      "54138",         BestwayProtocolFamily::Uart4Wire,    true,  true,  false },
    { BestwayModel::M54123,      "54123",         BestwayProtocolFamily::Uart4Wire,    true,  false, true  },
};

inline constexpr size_t BESTWAY_MODEL_COUNT =
    sizeof(BESTWAY_MODELS) / sizeof(BESTWAY_MODELS[0]);

inline constexpr const BestwayModelInfo &bestway_model_info(BestwayModel model)
{
    for (const auto &info : BESTWAY_MODELS) {
        if (info.model == model) {
            return info;
        }
    }

    return BESTWAY_MODELS[0];
}

inline constexpr const char *bestway_protocol_name(BestwayProtocolFamily family)
{
    switch (family) {
        case BestwayProtocolFamily::Uart4Wire:
            return "4-WIRE UART";

        case BestwayProtocolFamily::SixWireType1:
            return "6-WIRE TYPE 1";

        case BestwayProtocolFamily::SixWireType2:
            return "6-WIRE TYPE 2";
    }

    return "UNKNOWN";
}



enum class BestwayButton : uint8_t {
    NoButton  = 0,
    Lock      = 1,
    Timer     = 2,
    Bubbles   = 3,
    Unit      = 4,
    Heat      = 5,
    Pump      = 6,
    Down      = 7,
    Up        = 8,
    Power     = 9,
    HydroJets = 10,
};

/*
 * Exacte 6-wire Type1 buttoncodes uit de oude v4.
 * Indexvolgorde:
 * NOBTN, LOCK, TIMER, BUBBLES, UNIT, HEAT,
 * PUMP, DOWN, UP, POWER, HYDROJETS
 */

inline constexpr uint16_t BW_TYPE1_PRE2021_BUTTONS[11] = {
    0x1B1B, // NOBTN
    0x0200, // LOCK
    0x0100, // TIMER
    0x0300, // BUBBLES
    0x1012, // UNIT
    0x1212, // HEAT
    0x1112, // PUMP
    0x1312, // DOWN
    0x0809, // UP
    0x0000, // POWER
    0x0000  // HYDROJETS not supported
};

inline constexpr uint16_t BW_TYPE1_MIAMI2021_BUTTONS[11] = {
    0x1B1B, // NOBTN
    0x0100, // LOCK
    0x0300, // TIMER
    0x1212, // BUBBLES
    0x0809, // UNIT
    0x1012, // HEAT
    0x1112, // PUMP
    0x1312, // DOWN
    0x0200, // UP
    0x0000, // POWER
    0x0000  // HYDROJETS not supported
};

inline constexpr uint16_t BW_TYPE1_MALDIVES2021_BUTTONS[11] = {
    0x1B1B, // NOBTN
    0x0100, // LOCK
    0x0300, // TIMER
    0x1212, // BUBBLES
    0x0A09, // UNIT
    0x1012, // HEAT
    0x1312, // PUMP
    0x0809, // DOWN
    0x0200, // UP
    0x0000, // POWER
    0x1112  // HYDROJETS
};

inline constexpr uint16_t bestway_type1_button_code(
    BestwayModel model,
    BestwayButton button)
{
    const auto index = static_cast<uint8_t>(button);

    if (index > 10) {
        return 0;
    }

    switch (model) {
        case BestwayModel::PRE2021:
            return BW_TYPE1_PRE2021_BUTTONS[index];

        case BestwayModel::MIAMI2021:
            return BW_TYPE1_MIAMI2021_BUTTONS[index];

        case BestwayModel::MALDIVES2021:
            return BW_TYPE1_MALDIVES2021_BUTTONS[index];

        default:
            return 0;
    }
}

inline bool bestway_type1_decode_button(
    BestwayModel model,
    uint16_t code,
    BestwayButton &button)
{
    for (uint8_t i = 0; i <= 10; ++i) {
        const auto candidate =
            static_cast<BestwayButton>(i);

        if (bestway_type1_button_code(model, candidate) == code) {
            /*
             * 0x0000 is bij Type1 een geldige POWER-code.
             * Door van laag naar hoog door de enum te lopen wordt
             * POWER gevonden vóór een eventuele ongebruikte placeholder.
             */
            button = candidate;
            return true;
        }
    }

    return false;
}


/*
 * Exacte 6-wire Type2 buttoncodes voor MODEL54149E uit oude v4.
 * Indexvolgorde:
 * NOBTN, LOCK, TIMER, BUBBLES, UNIT, HEAT,
 * PUMP, DOWN, UP, POWER, HYDROJETS
 */
inline constexpr uint16_t BW_TYPE2_54149E_BUTTONS[11] = {
    0x0000, // NOBTN
    1U << 7, // LOCK
    1U << 6, // TIMER
    1U << 5, // BUBBLES
    1U << 4, // UNIT
    1U << 3, // HEAT
    1U << 2, // PUMP
    1U << 1, // DOWN
    1U << 0, // UP
    1U << 8, // POWER
    1U << 9  // HYDROJETS
};

inline constexpr uint16_t bestway_type2_button_code(
    BestwayModel model,
    BestwayButton button)
{
    const auto index = static_cast<uint8_t>(button);

    if (index > 10) {
        return 0;
    }

    switch (model) {
        case BestwayModel::M54149E:
            return BW_TYPE2_54149E_BUTTONS[index];

        default:
            return 0;
    }
}

inline bool bestway_type2_decode_button(
    BestwayModel model,
    uint16_t code,
    BestwayButton &button)
{
    for (uint8_t i = 0; i <= 10; ++i) {
        const auto candidate =
            static_cast<BestwayButton>(i);

        if (bestway_type2_button_code(model, candidate) == code) {
            button = candidate;
            return true;
        }
    }

    return false;
}


struct Bestway4WireMasks {
    uint8_t pump;
    uint8_t bubbles;
    uint8_t jets;
    uint8_t heat1;
    uint8_t heat2;
    uint8_t power;
};

inline constexpr Bestway4WireMasks bestway_4wire_masks(BestwayModel model)
{
    switch (model) {
        case BestwayModel::M54123:
            return {0x10, 0x20, 0x00, 0x02, 0x08, 0x01};

        case BestwayModel::M54138:
            return {0x05, 0x02, 0x08, 0x30, 0x40, 0x80};

        case BestwayModel::M54144:
            return {0x05, 0x02, 0x08, 0x30, 0x40, 0x80};

        case BestwayModel::M54154:
            return {0x10, 0x20, 0x00, 0x02, 0x08, 0x01};

        case BestwayModel::M54173:
            return {0x05, 0x02, 0x08, 0x30, 0x40, 0x80};

        default:
            return {0, 0, 0, 0, 0, 0};
    }
}


enum class Bestway4WireToggle : uint8_t {
    Bubbles = 0,
    Jets    = 1,
    Pump    = 2,
    Heat    = 3,
};

struct Bestway4WireState {
    uint8_t bubbles;
    uint8_t jets;
    uint8_t pump;
    uint8_t heat;
};

/* 54123 */
inline constexpr uint8_t BW54123_JUMP[4][4] = {
    {1,0,2,3},
    {0,1,2,3},
    {1,2,0,3},
    {1,3,0,2},
};

inline constexpr Bestway4WireState BW54123_STATE[4] = {
    {0,0,0,0},
    {1,0,0,0},
    {0,0,1,0},
    {0,0,1,2},
};

/* 54138 */
inline constexpr uint8_t BW54138_JUMP[5][4] = {
    {1,2,3,4},
    {0,2,3,4},
    {1,0,3,4},
    {1,2,0,4},
    {1,2,0,3},
};

inline constexpr Bestway4WireState BW54138_STATE[5] = {
    {0,0,0,0},
    {1,0,0,0},
    {0,1,0,0},
    {0,0,1,0},
    {0,0,1,2},
};

/* 54144 */
inline constexpr uint8_t BW54144_JUMP[5][4] = {
    {1,2,3,4},
    {0,2,3,4},
    {1,0,3,4},
    {1,2,0,4},
    {1,2,0,3},
};

inline constexpr Bestway4WireState BW54144_STATE[5] = {
    {0,0,0,0},
    {1,0,0,0},
    {0,1,0,0},
    {0,0,1,0},
    {0,0,1,2},
};

/* 54154 */
inline constexpr uint8_t BW54154_JUMP[6][4] = {
    {3,0,1,2},
    {4,1,0,2},
    {5,2,0,1},
    {0,3,4,5},
    {1,4,3,5},
    {2,5,3,4},
};

inline constexpr Bestway4WireState BW54154_STATE[6] = {
    {0,0,0,0},
    {0,0,1,0},
    {0,0,1,2},
    {1,0,0,0},
    {1,0,1,0},
    {1,0,1,1},
};

/* 54173 */
inline constexpr uint8_t BW54173_JUMP[8][4] = {
    {6,4,1,3},
    {7,4,0,3},
    {3,5,6,7},
    {2,4,0,1},
    {5,0,1,3},
    {4,6,1,3},
    {0,5,7,2},
    {1,5,6,2},
};

inline constexpr Bestway4WireState BW54173_STATE[8] = {
    {0,0,0,0},
    {0,0,1,0},
    {1,0,1,1},
    {0,0,1,2},
    {0,1,0,0},
    {1,1,0,0},
    {1,0,0,0},
    {1,0,1,0},
};

inline constexpr uint8_t bestway_4wire_state_count(BestwayModel model)
{
    switch (model) {
        case BestwayModel::M54123: return 4;
        case BestwayModel::M54138: return 5;
        case BestwayModel::M54144: return 5;
        case BestwayModel::M54154: return 6;
        case BestwayModel::M54173: return 8;
        default: return 0;
    }
}

inline constexpr uint8_t bestway_4wire_next_state(
    BestwayModel model,
    uint8_t current,
    Bestway4WireToggle toggle)
{
    const uint8_t col = static_cast<uint8_t>(toggle);

    switch (model) {
        case BestwayModel::M54123:
            return current < 4 ? BW54123_JUMP[current][col] : 0;

        case BestwayModel::M54138:
            return current < 5 ? BW54138_JUMP[current][col] : 0;

        case BestwayModel::M54144:
            return current < 5 ? BW54144_JUMP[current][col] : 0;

        case BestwayModel::M54154:
            return current < 6 ? BW54154_JUMP[current][col] : 0;

        case BestwayModel::M54173:
            return current < 8 ? BW54173_JUMP[current][col] : 0;

        default:
            return 0;
    }
}

inline constexpr Bestway4WireState bestway_4wire_state(
    BestwayModel model,
    uint8_t state)
{
    switch (model) {
        case BestwayModel::M54123:
            return state < 4 ? BW54123_STATE[state] : Bestway4WireState{0,0,0,0};

        case BestwayModel::M54138:
            return state < 5 ? BW54138_STATE[state] : Bestway4WireState{0,0,0,0};

        case BestwayModel::M54144:
            return state < 5 ? BW54144_STATE[state] : Bestway4WireState{0,0,0,0};

        case BestwayModel::M54154:
            return state < 6 ? BW54154_STATE[state] : Bestway4WireState{0,0,0,0};

        case BestwayModel::M54173:
            return state < 8 ? BW54173_STATE[state] : Bestway4WireState{0,0,0,0};

        default:
            return {0,0,0,0};
    }
}

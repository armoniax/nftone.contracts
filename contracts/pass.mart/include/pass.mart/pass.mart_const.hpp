#pragma once

#include <cstdint>
#include <eosio/name.hpp>
#include <amax.ntoken/amax.ntoken.hpp>

using namespace eosio;

enum class err: uint8_t {
   NONE                 = 0,
   RECORD_NOT_FOUND     = 1,
   RECORD_EXISTING      = 2,
   SYMBOL_MISMATCH      = 4,
   PARAM_ERROR          = 5,
   MEMO_FORMAT_ERROR    = 6,
   PAUSED               = 7,
   NO_AUTH              = 8,
   NOT_POSITIVE         = 9,
   NOT_STARTED          = 10,
   OVERSIZED            = 11,
   TIME_EXPIRED         = 12,
   NOTIFY_UNRELATED     = 13,
   ACTION_REDUNDANT     = 14,
   ACCOUNT_INVALID      = 15,
   FEE_INSUFFICIENT     = 16,
   FIRST_CREATOR        = 17,
   STATUS_ERROR         = 18,
   RATE_OVERLOAD        = 19,
   DATA_ERROR           = 20,
   MISC                 = 255
};

namespace pass_status {
    static constexpr eosio::name none           = "none"_n;
    static constexpr eosio::name open         = "open"_n;
    static constexpr eosio::name closed         = "closed"_n;
    static constexpr eosio::name processing     = "processing"_n;
};

constexpr eosio::name MBANK                     = "amax.mtoken"_n;
static constexpr symbol MUSDT                   = symbol(symbol_code("MUSDT"), 6);

static constexpr uint32_t MAX_TITLE_SIZE        = 64;

// static constexpr uint32_t seconds_per_year      = 24 * 3600 * 7 * 52;
// static constexpr uint32_t seconds_per_month     = 24 * 3600 * 30;
// static constexpr uint32_t seconds_per_week      = 24 * 3600 * 7;
// static constexpr uint32_t seconds_per_day       = 24 * 3600;
// static constexpr uint32_t seconds_per_hour      = 3600;

static constexpr eosio::name active_permission{"active"_n};

#define TRANSFER(bank, to, quantity, memo) \
{ action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }

#define TRANSFER_N(bank, to, quants, memo) \
    {	ntoken::transfer_action act{ bank, { {_self, active_perm} } };\
			act.send( _self, to, quants , memo );}

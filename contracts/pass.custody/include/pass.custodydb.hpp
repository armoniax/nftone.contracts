#pragma once

#include <amax.ntoken/amax.ntoken.hpp>
#include "commons/wasm_db.hpp"

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

using namespace eosio;
using namespace std;
using std::string;

// using namespace wasm;
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm        {"active"_n};
static constexpr symbol SYS_SYMBOL              = SYMBOL("AMAX", 8);
static constexpr name SYS_BANK                  { "amax.token"_n };

static constexpr uint64_t MAX_LOCK_DAYS         = 365 * 10;

#ifndef DAY_SECONDS_FOR_TEST
static constexpr uint64_t DAY_SECONDS           = 24 * 60 * 60;
#else
#warning "DAY_SECONDS_FOR_TEST should be used only for test!!!"
static constexpr uint64_t DAY_SECONDS           = DAY_SECONDS_FOR_TEST;
#endif//DAY_SECONDS_FOR_TEST

static constexpr uint32_t MAX_TITLE_SIZE        = 64;

namespace plan_status {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name enabled            = "enabled"_n;
    static constexpr eosio::name disabled           = "disabled"_n;
    static constexpr eosio::name feeunpaid          = "feeunpaid"_n;
};

namespace lock_status {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name locked             = "locked"_n;
    static constexpr eosio::name unlocked           = "unlocked"_n;
};

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
   DATA_MISMATCH        = 20,
   MISC                 = 255
};


namespace wasm { namespace db {

using namespace amax;

#define CUSTODY_TBL [[eosio::table, eosio::contract("pass.custody")]]
#define CUSTODY_TBL_NAME(name) [[eosio::table(name), eosio::contract("pass.custody")]]

struct CUSTODY_TBL_NAME("global") global_t {
    asset plan_fee          = asset(0, SYS_SYMBOL);
    name fee_receiver       = "amax.daodev"_n;
    uint64_t  last_plan_id  = 0;

    EOSLIB_SERIALIZE( global_t, (plan_fee)(fee_receiver)(last_plan_id) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


struct CUSTODY_TBL plan_t {
    uint64_t            id = 0;                     //PK
    name                owner;                      //plan owner
    string              title;                      //plan title: <=64 chars
    name                asset_contract;             //asset issuing contract (ARC20)
    nsymbol             asset_symbol;               //NFT Asset Symbol, E.g. [10001, 0]
    uint64_t            unlock_interval_days;       //interval between two consecutive unlock timepoints
    uint64_t            unlock_times;               //unlock times, duration=unlock_interval_days*unlock_times
    nasset              total_issued;               //stats: updated upon issue deposit
    nasset              total_locked;               //stats: updated upon lock & unlock
    nasset              total_unlocked;             //stats: updated upon unlock & endissue
    nasset              total_refunded;             //stats: updated upon and endissue
    name                status = plan_status::feeunpaid;   //status, see plan_status_t
    uint64_t            last_lock_id = 0;

    time_point_sec      created_at;                 //creation time (UTC time)
    time_point_sec      updated_at;                 //update time: last updated at

    uint64_t primary_key() const { return id; }

    uint128_t by_owner() const { return (uint128_t)owner.value << 64 | (uint128_t)id; }

    typedef eosio::multi_index<"plans"_n, plan_t,
        indexed_by<"owneridx"_n,  const_mem_fun<plan_t, uint128_t, &plan_t::by_owner> >
    > tbl_t;

    EOSLIB_SERIALIZE( plan_t, (id)(owner)(title)(asset_contract)(asset_symbol)(unlock_interval_days)(unlock_times)
                              (total_issued)(total_locked)(total_unlocked)(total_refunded)(status)(last_lock_id)(created_at)(updated_at) )

};

// scope = plan_id
struct CUSTODY_TBL lock_t {
    uint64_t        id = 0;                       //PK, unique within the contract
    name            locker;                       //locker
    name            receiver;                     //receiver of issue who can unlock
    nasset          issued;                       //originally issued amount
    nasset          locked;                       //currently locked amount
    nasset          unlocked;                     //currently unlocked amount
    uint64_t        first_unlock_days = 0;        //unlock since issued_at
    uint64_t        unlock_interval_days;         //interval between two consecutive unlock timepoints
    uint64_t        unlock_times;                 //unlock times, duration=unlock_interval_days*unlock_times
    name            status = lock_status::none;   //status of issue, see issue_status_t
    time_point      locked_at;                    //locker time (UTC time)
    time_point      updated_at;                   //update time: last unlocked at

    uint64_t primary_key() const { return id; }

    uint128_t by_receiver_issue() const { return (uint128_t)receiver.value << 64 | (uint128_t)id; }
    uint64_t by_receiver()const { return receiver.value; }

    typedef eosio::multi_index<"locks"_n, lock_t,
        indexed_by<"receiveridx"_n,     const_mem_fun<lock_t, uint128_t, &lock_t::by_receiver_issue>>,
        indexed_by<"receivers"_n,       const_mem_fun<lock_t, uint64_t,  &lock_t::by_receiver>>
    > tbl_t;

    EOSLIB_SERIALIZE( lock_t,   (id)(locker)(receiver)(issued)(locked)(unlocked)
                                (first_unlock_days)(unlock_interval_days)(unlock_times)
                                (status)(locked_at)(updated_at) )
};

// scope = contract self
// table for plan creator accounts
struct CUSTODY_TBL account {
    name    owner;
    uint64_t last_plan_id;

    uint64_t primary_key()const { return owner.value; }

    typedef multi_index_ex< "payaccounts"_n, account > tbl_t;

    EOSLIB_SERIALIZE( account,  (owner)(last_plan_id) )
};

} }
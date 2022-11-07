#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <amax.ntoken/amax.ntoken.db.hpp>

using namespace eosio;
using namespace std;
using std::string;
using namespace amax;

namespace custody{

namespace plan_status {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name enabled            = "enabled"_n;
    static constexpr eosio::name disabled           = "disabled"_n;
    static constexpr eosio::name feeunpaid          = "feeunpaid"_n;
};

struct plan_t {
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
    > idx_t;

    EOSLIB_SERIALIZE( plan_t, (id)(owner)(title)(asset_contract)(asset_symbol)(unlock_interval_days)(unlock_times)
                              (total_issued)(total_locked)(total_unlocked)(total_refunded)(status)(last_lock_id)(created_at)(updated_at) )

};

struct split_plan_t {
    uint64_t                    id;        //PK
    symbol                      token_symbol;
    bool                        split_by_rate   = false;  //rate boost by 10000

    uint64_t primary_key()const { return id; }

    typedef eosio::multi_index<"splitplans"_n, split_plan_t > idx_t;

    EOSLIB_SERIALIZE( split_plan_t, (id)(token_symbol)(split_by_rate) )
};

}

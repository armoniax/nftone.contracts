#pragma once

#include <amax.ntoken/amax.ntoken.hpp>
#include "commons/wasm_db.hpp"

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <commons/utils.hpp>

using namespace eosio;
using namespace std;
using std::string;

// using namespace wasm;
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr uint64_t DAY_SECONDS           = 24 * 60 * 60;
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
   OVERDRAWN            = 21,
   MISC                 = 255
};


namespace wasm { namespace db {

using namespace amax;

#define TBL struct [[eosio::table, eosio::contract("rndnft.mart")]]
#define TBL_NAME(name) struct [[eosio::table(name), eosio::contract("rndnft.mart")]]

static constexpr eosio::name active_permission{"active"_n};


struct aplink_farm {
    name contract           = "aplink.farm"_n;
    uint64_t lease_id       = 6;    //xch-farm-land
    map<string, asset> xin_reward_conf = {
        { "MUSDT", asset_from_string("5.0000 APL")      }
    };
};


TBL_NAME("global") global_t {
    name        admin;
    name        fund_distributor;
    uint16_t    max_booth_boxes      = 30;
    uint64_t    last_booth_id        = 0;
    
    EOSLIB_SERIALIZE( global_t, (admin)(fund_distributor)(max_booth_boxes)(last_booth_id))
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

TBL_NAME("global1") global1_t {
    aplink_farm apl_farm ;

    EOSLIB_SERIALIZE( global1_t, (apl_farm))
};
typedef eosio::singleton< "global1"_n, global1_t > global1_singleton;

namespace booth_status {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name enabled            = "enabled"_n;
    static constexpr eosio::name disabled           = "disabled"_n;
};

// namespace nft_random_type {
//     static constexpr eosio::name none               = "none"_n;
//     static constexpr eosio::name nftboxnum          = "nftboxnum"_n;
//     static constexpr eosio::name nftnum             = "nftnum"_n;
// };

TBL booth_t {
    uint64_t            id = 0;                                             //PK
    name                owner;                                              //shop owner
    string              title;                                              //shop title: <=64 chars                                
    name                nft_contract;
    name                fund_contract;
    uint64_t            split_plan_id;                                      //amax.split plan ID must be prepared in advance
    asset               price; 
    asset               fund_recd;
    uint64_t            nft_box_num                 = 0;
    uint64_t            nft_num                     = 0;                    // available num    
    uint64_t            nft_num_sum                 = 0;                    // total sum num
    name                status                      = booth_status::none;    //status, see plan_status_t
    time_point_sec      created_at;                                         //creation time (UTC time)
    time_point_sec      updated_at;                                         //update time: last updated at
    time_point_sec      opened_at;                                          //opend time: opening time of blind box
    time_point_sec      closed_at;                                          //close time: close time of blind box
    

    booth_t() {}
    booth_t( const uint64_t& i): id(i) {}

    uint64_t primary_key() const { return id; }
    uint128_t by_owner() const { return (uint128_t)owner.value << 64 | (uint128_t)id; }

    typedef eosio::multi_index<"booths"_n, booth_t,
        indexed_by<"owneridx"_n,  const_mem_fun<booth_t, uint128_t, &booth_t::by_owner> >
    > idx_t;

    EOSLIB_SERIALIZE( booth_t, (id)(owner)(title)(nft_contract)(fund_contract)(split_plan_id)(price)(fund_recd)
                               (nft_box_num)(nft_num)(nft_num_sum)(status)
                               (created_at)(updated_at)(opened_at)(closed_at) )

};

TBL booth_nftbox_t {
    uint64_t                booth_id;
    map<nsymbol, uint64_t>  nfts;
      
    booth_nftbox_t() {}
    booth_nftbox_t( const uint64_t& sid ): booth_id(sid) {}

    uint64_t primary_key() const { return booth_id; }

    typedef eosio::multi_index<"boothboxes"_n, booth_nftbox_t
    > idx_t;

    EOSLIB_SERIALIZE( booth_nftbox_t,  (booth_id)(nfts) )
};

struct deal_trace_s_s {
    uint64_t            booth_id;
    name                buyer;
    name                nft_contract;
    name                fund_contract;
    asset               paid_quant;
    nasset              sold_quant;
    time_point_sec      created_at;
};

} }
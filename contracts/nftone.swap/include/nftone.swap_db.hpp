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

#define BLINDBOX [[eosio::table, eosio::contract("nftone.swap")]]
#define BLINDBOX_NAME(name) [[eosio::table(name), eosio::contract("nftone.swap")]]

static constexpr eosio::name active_permission{"active"_n};

struct BLINDBOX_NAME("global") global_t {

    name      admin;
    uint64_t  last_pool_id          = 0;
    uint64_t  last_order_id         = 0;

    EOSLIB_SERIALIZE( global_t, (admin)(last_pool_id)(last_order_id) )
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

namespace pool_status {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name enabled            = "enabled"_n;
    static constexpr eosio::name disabled           = "disabled"_n;
};

namespace transfer_type {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name transfer            = "transfer"_n;
    static constexpr eosio::name allowance           = "allowance"_n;
};

struct price_t {
    asset               value;
    asset               received;
    name                fee_receiver;
    EOSLIB_SERIALIZE( price_t,  (value)(received)(fee_receiver) )
};

struct BLINDBOX pool_t {
    uint64_t            id = 0;                                             //PK
    name                owner;                                              //pool owner
    string              title;                                              //pool title: <=64 chars
    name                asset_contract;                                     
    price_t             price;                                        
    name                blindbox_contract;                                  
    uint64_t            total_nft_amount            = 0;
    uint64_t            exchange_nft_amount         = 0;         
    uint64_t            not_exchange_nft_amount     = 0;       
    uint64_t            refund_nft_amount           = 0;   
    uint64_t            max_table_distance          = 0;                    // table advance max  
    name                status                      = pool_status::none;    //status, see plan_status_t
    bool                allow_to_buy_again;
    uint64_t            last_nft_id                 = 0;
    time_point_sec      created_at;                                         //creation time (UTC time)
    time_point_sec      updated_at;                                         //update time: last updated at
    time_point_sec      opended_at;                                         //opend time: opening time of blind box
    time_point_sec      closed_at;                                          //close time: close time of blind box
    

    uint64_t primary_key() const { return id; }

    uint128_t by_owner() const { return (uint128_t)owner.value << 64 | (uint128_t)id; }

    typedef eosio::multi_index<"pools"_n, pool_t,
        indexed_by<"owneridx"_n,  const_mem_fun<pool_t, uint128_t, &pool_t::by_owner> >
    > tbl_t;

    EOSLIB_SERIALIZE( pool_t, (id)(owner)(title)(asset_contract)
                              (price)(blindbox_contract)(total_nft_amount)(exchange_nft_amount)
                              (not_exchange_nft_amount)(refund_nft_amount)(max_table_distance)
                              (status)(allow_to_buy_again)(last_nft_id)(created_at)(updated_at)(opended_at)(closed_at) )

};

// scope = pool_id
struct BLINDBOX buyer_t {

    name                owner;
    uint64_t            buy_times     = 0;
    time_point_sec      created_at;                                         //creation time (UTC time)
    time_point_sec      updated_at;                                         //update time: last updated at

    uint64_t primary_key() const { return owner.value; }

     typedef eosio::multi_index<"buyers"_n, buyer_t
    > tbl_t;

    EOSLIB_SERIALIZE( buyer_t,   (owner)(buy_times)(created_at)(updated_at)
                               )
};


// scope = pool_id
struct BLINDBOX nft_boxes_t {

    uint64_t        id = 0;                       //PK, unique within the contract
    nasset          quantity;  
    name            transfer_type = transfer_type::none;               
      
    uint64_t primary_key() const { return id; }

    typedef eosio::multi_index<"boxes"_n, nft_boxes_t
    > tbl_t;

    EOSLIB_SERIALIZE( nft_boxes_t,   (id)(quantity)(transfer_type)
                               )
};

struct deal_trace_t {

        uint64_t            pool_id;
        uint64_t            order_id;
        name                receiver;
        asset               pay_quantity;
        nasset              recv_quantity;
        time_point_sec      created_at;
        name                pay_contract;
        name                recv_contract;
};

} }
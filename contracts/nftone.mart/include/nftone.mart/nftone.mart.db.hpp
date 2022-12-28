#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <eosio/binary_extension.hpp> 
#include <amax.ntoken/amax.ntoken.db.hpp>
#include <utils.hpp>

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

namespace amax {

using namespace std;
using namespace eosio;

#define TBL struct [[eosio::table, eosio::contract("nftone.mart")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("nftone.mart")]]

static constexpr uint8_t MAX_BUYER_BID_COUNT = 30;
static constexpr uint8_t MAX_REFUND_COUNT = 10;

struct aplink_farm {
    name contract           = "aplink.farm"_n;
    uint64_t lease_id       = 4;    //nftone-farm-land
    asset unit_reward       = asset_from_string("0.0500 APL");  //per MUSDT
};

NTBL("global") global_t {
    name admin;
    name dev_fee_collector;
    double dev_fee_rate             = 0.0;
    double creator_fee_rate         = 0.0;
    double ipowner_fee_rate         = 0.0;
    double notary_fee_rate          = 0.0;
    uint32_t order_expiry_hours     = 72;
    eosio::symbol pay_symbol;
    name bank_contract               = "amax.mtoken"_n;
    aplink_farm apl_farm;
    uint64_t last_buy_order_idx     = 0;
    uint64_t last_deal_idx          = 0;

    EOSLIB_SERIALIZE( global_t, (admin)(dev_fee_collector)(dev_fee_rate)(creator_fee_rate)(ipowner_fee_rate)
                                (notary_fee_rate)(order_expiry_hours)(pay_symbol)(bank_contract)
                                (apl_farm)(last_buy_order_idx)(last_deal_idx) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


// price for NFT tokens
struct price_s {
    asset value;    //price value
    nsymbol symbol;

    price_s() {}
    price_s(const asset& v, const nsymbol& symb): value(v), symbol(symb) {}

    friend bool operator > ( const price_s& a, const price_s& b ) {
        return a.value > b.value;
    }

    friend bool operator <= ( const price_s& a, const price_s& b ) {
        return a.value <= b.value;
    }

    friend bool operator < ( const price_s& a, const price_s& b ) {
        return a.value < b.value;
    }

    EOSLIB_SERIALIZE( price_s, (value)(symbol) )
};


//Scope: nasset.symbol.id
TBL order_t {
    uint64_t                          id;                 //PK
    price_s                           price;
    int64_t                           frozen;             //nft amount for sellers
    name                              maker;
    time_point_sec                    created_at;
    time_point_sec                    updated_at;
    binary_extension<name>            nbank = "amax.ntoken"_n;
    binary_extension<name>            cbank = "amax.mtoken"_n;

    order_t() {}
    order_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    uint64_t by_small_price_first()const { return price.value.amount; }
    uint64_t by_large_price_first()const { return( std::numeric_limits<uint64_t>::max() - price.value.amount ); }
    uint128_t by_maker_small_price_first()const { return (uint128_t) maker.value << 64 | (uint128_t) price.value.amount ; }
    uint128_t by_maker_large_price_first()const { return (uint128_t) maker.value << 64 | (uint128_t) (std::numeric_limits<uint64_t>::max() - price.value.amount ); }
    uint128_t by_maker_created_at()const { return (uint128_t) maker.value << 64 | (uint128_t) created_at.sec_since_epoch(); }

    typedef eosio::multi_index
    < "sellorders"_n,  order_t,
        indexed_by<"makerordidx"_n,     const_mem_fun<order_t, uint128_t, &order_t::by_maker_small_price_first> >,
        indexed_by<"makercreated"_n,    const_mem_fun<order_t, uint128_t, &order_t::by_maker_created_at> >,
        indexed_by<"priceidx"_n,        const_mem_fun<order_t, uint64_t,  &order_t::by_small_price_first> >
    > idx_t;
    EOSLIB_SERIALIZE( order_t, (id)(price)(frozen)(maker)(created_at)(updated_at)(nbank)(cbank) )
    // template<typename DataStream>
    // friend DataStream& operator << ( DataStream& ds, const order_t& t ) {
    //     ds      << t.id
    //             << t.price
    //             << t.frozen
    //             << t.maker
    //             << t.created_at
    //             << t.updated_at
    //             << t.nbank
    //             << t.cbank;
    //     return ds;
    // }
    
    // //read op (read as is)
    // template<typename DataStream>
    // friend DataStream& operator >> ( DataStream& ds, order_t& t ) {  
    //      ds >> t.id;
    //      ds >> t.price;
    //      ds >> t.frozen;
    //      ds >> t.maker;
    //      ds >> t.created_at;
    //      ds >> t.updated_at;
    //      if(ds.remaining() != 0)  ds >> t.nbank;
    //      if(ds.remaining() != 0)  ds >> t.cbank;
    //     return ds;
    // }
};

TBL buyer_bid_t {
    uint64_t        id;
    uint64_t        sell_order_id;
    price_s         price;
    asset           frozen; //CNYD
    name            buyer;
    time_point_sec  created_at;
    binary_extension<name>        cbank = "amax.mtoken"_n; 
    // name            cbank  = "amax.mtoken"_n;


    buyer_bid_t() {}
    buyer_bid_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    uint64_t by_large_price_first()const { return( std::numeric_limits<uint64_t>::max() - price.value.amount ); }

    checksum256 by_buyer_created_at()const { return make256key( sell_order_id,
                                                                buyer.value,
                                                                created_at.sec_since_epoch(),
                                                                id); }
    uint64_t by_sell_order_id()const { return sell_order_id; }

    EOSLIB_SERIALIZE( buyer_bid_t, (id)(sell_order_id)(price)(frozen)(buyer)(created_at)(cbank) )

    typedef eosio::multi_index
    < "buyerbids"_n,  buyer_bid_t,
        indexed_by<"priceidx"_n,        const_mem_fun<buyer_bid_t, uint64_t, &buyer_bid_t::by_large_price_first> >,
        indexed_by<"sellorderidx"_n,    const_mem_fun<buyer_bid_t, uint64_t, &buyer_bid_t::by_sell_order_id> >,
        indexed_by<"createidx"_n,       const_mem_fun<buyer_bid_t, checksum256, &buyer_bid_t::by_buyer_created_at> >
    > idx_t;

    // template<typename DataStream>
    // friend DataStream& operator << ( DataStream& ds, const buyer_bid_t& t ) {
    //     return ds   << t.id
    //                 << t.sell_order_id
    //                 << t.price
    //                 << t.frozen
    //                 << t.buyer
    //                 << t.created_at
    //                 << t.cbank;
    // }
    
    // //read op (read as is)
    // template<typename DataStream>
    // friend DataStream& operator >> ( DataStream& ds, buyer_bid_t& t ) {  
    //     ds >> t.id;
    //     ds >> t.sell_order_id;
    //     ds >> t.price;
    //     ds >> t.frozen;
    //     ds >> t.buyer;
    //     ds >> t.created_at;
    //     if(ds.remaining() != 0) ds >> t.cbank;
    //     return ds;
    // }  

};

// scope: cbank
TBL coinconf_t {
    symbol          pay_symbol;
    coinconf_t() {}
    coinconf_t(const symbol& i): pay_symbol(i) {}
    EOSLIB_SERIALIZE( coinconf_t, (pay_symbol))

    uint64_t primary_key()const { return pay_symbol.raw(); }
    typedef eosio::multi_index < "coinconfs"_n,  coinconf_t> idx_t;
};

//scope: _self
TBL nftconf_t {
    name            nbank;
    // name            status;
    nftconf_t() {}
    nftconf_t(const name& i): nbank(i) {}

    EOSLIB_SERIALIZE( nftconf_t, (nbank) )


    uint64_t primary_key()const { return nbank.value; }
    typedef eosio::multi_index < "nftconfs"_n,  nftconf_t> idx_t;

};

} //namespace amax
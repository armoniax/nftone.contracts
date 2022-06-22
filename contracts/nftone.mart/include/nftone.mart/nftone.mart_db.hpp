#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <amax.ntoken/amax.ntoken_db.hpp>

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>



namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

#define TBL struct [[eosio::table, eosio::contract("nftone.mart")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("nftone.mart")]]

NTBL("global") global_t {
    name admin;
    name dev_fee_collector;
    float dev_fee_rate      = 0.0;
    float creator_fee_rate  = 0.0;
    float ipowner_fee_rate  = 0.0;
    float notary_fee_rate   = 0.0;
    uint32_t order_expiry_hours = 72;

    EOSLIB_SERIALIZE( global_t, (admin)(dev_fee_collector)(dev_fee_rate)(creator_fee_rate)(ipowner_fee_rate)(notary_fee_rate)(order_expiry_hours) )

/*
    // template<typename DataStream>
    // friend DataStream& operator << ( DataStream& ds, const global_t& t ) {
    //     return ds   << t.admin 
    //                 << t.dev_fee_collector
    //                 << t.dev_fee_rate
    //                 << t.creator_fee_rate
    //                 << t.ipowner_fee_rate
    //                 << t.notary_fee_rate
    //                 << t.order_expiry_hours ;
    // }

    // template<typename DataStream>
    // friend DataStream& operator >> ( DataStream& ds, global_t& t ) {
    //     return ds;
    // }
*/
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


// price for NFT tokens
struct price_s {
    float value;    //price value
    nsymbol symbol;

    price_s() {}
    price_s(const float& v, const nsymbol& symb): value(v), symbol(symb) {}

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
    uint64_t        id;                 //PK
    price_s         price;
    int64_t         frozen;             //nft amount for sellers, cnyd amount for buyers
    name            maker;
    time_point_sec  created_at;
    time_point_sec  updated_at;

    order_t() {}
    order_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    uint64_t by_small_price_first()const { return price.value; }
    uint64_t by_large_price_first()const { return( std::numeric_limits<uint64_t>::max() - price.value ); }
    uint128_t by_maker_small_price_first()const { return (uint128_t) maker.value << 64 | (uint128_t) price.value; }
    uint128_t by_maker_large_price_first()const { return (uint128_t) maker.value << 64 | (uint128_t) (std::numeric_limits<uint64_t>::max() - price.value); }

    EOSLIB_SERIALIZE( order_t, (id)(price)(frozen)(maker)(created_at)(updated_at) )
 
};

TBL buyer_bid_t {
    uint64_t        id;
    uint64_t        sell_order_id;
    price_s         price;
    int64_t         frozen; //CNYD
    name            buyer;
    time_point_sec  created_at;

    buyer_bid_t() {}
    buyer_bid_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    uint64_t by_large_price_first()const { return( std::numeric_limits<uint64_t>::max() - price.value ); }

    EOSLIB_SERIALIZE( buyer_bid_t, (id)(sell_order_id)(price)(frozen)(buyer)(created_at) )

    typedef eosio::multi_index
    < "buyerbids"_n,  buyer_bid_t,
        indexed_by<"priceidx"_n,        const_mem_fun<buyer_bid_t, uint64_t, &buyer_bid_t::by_large_price_first> >
    > idx_t;
};

/**
//Scope: owner
//if not set, a default rate of 1% will be charged to sellers
//dev fee rate is determined by contract dev only
// TBL fee_rate_t {
//     nsymbol     symbol;
//     float       creator_fee_rate;
//     float       ipowner_fee_rate;
//     float       notary_fee_rate;     

//     fee_rate_t() {}
//     fee_rate_t(const nsymbol& symb): symbol(symb) {}
//     fee_rate_t(const nsymbol& symb, const float& cfr, const float& ifr, const float& nfr): symbol(symb), 
//             creator_fee_rate(cfr), ipowner_fee_rate(ifr), notary_fee_rate(nfr) {}

//     uint64_t primary_key()const { return symbol.id; }

//     EOSLIB_SERIALIZE( fee_rate_t, (symbol)(creator_fee_rate)(ipowner_fee_rate)(notary_fee_rate) )  
// };
*/

typedef eosio::multi_index
< "sellorders"_n,  order_t,
    indexed_by<"makerordidx"_n,     const_mem_fun<order_t, uint128_t, &order_t::by_maker_small_price_first> >,
    indexed_by<"priceidx"_n,        const_mem_fun<order_t, uint64_t, &order_t::by_small_price_first> >
> sellorder_idx;

// //buyer to bid for the token ID
// typedef eosio::multi_index
// < "buyorders"_n,  order_t,
//     indexed_by<"makerordidx"_n,  const_mem_fun<order_t, uint128_t, &order_t::by_maker_large_price_first> >,
//     indexed_by<"priceidx"_n,  const_mem_fun<order_t, uint64_t, &order_t::by_large_price_first> >
// > buyorder_idx;

} //namespace amax
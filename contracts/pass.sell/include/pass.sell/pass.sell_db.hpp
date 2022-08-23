#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>
#include <eosio/name.hpp>
#include <amax.ntoken/amax.ntoken_db.hpp>
#include "pass.sell_const.hpp"
#include <string>

#define TBL struct [[eosio::table, eosio::contract("pass.sell")]]

namespace sell{

    using namespace amax;
    using std::string;

    struct [[eosio::table("global"), eosio::contract("pass.sell")]] global_t{

        name                 admin;
        asset                total_sells;
        asset                total_rewards;
        asset                total_claimed_rewards;
        name                 lock_contract ;
        name                 partner_account ;
        name                 nft_contract ;
        name                 storage_account;
        uint64_t             first_rate ;
        uint64_t             second_rate ;
        uint64_t             partner_rate ;
        uint64_t             operable_days;
        time_point_sec       started_at;
        uint64_t             last_product_id = 0;
        uint64_t             last_order_id = 0;
        
        EOSLIB_SERIALIZE( global_t, (admin)(total_sells)(total_rewards)(total_claimed_rewards)
        (lock_contract)(partner_account)(nft_contract)(storage_account)(first_rate)
        (second_rate)(partner_rate)(operable_days)(started_at)
        (last_product_id)(last_order_id) )
    };
    
    typedef eosio::singleton< "global"_n, global_t > global_singleton;

    struct rule_t{
        uint64_t single_amount = 0;
        bool buy_again_flag = true ;
    };

    TBL product_t{
        uint64_t            id;
        string              title;
        name                owner;
        nasset              balance;
        nasset              total_issue;
        asset               price;
        name                status = product_status::none;
        rule_t              rule;
        time_point_sec      created_at;
        time_point_sec      updated_at;
        time_point_sec      sell_started_at;
        time_point_sec      sell_ended_at;
        time_point_sec      operable_started_at;
        time_point_sec      operable_ended_at;

        product_t(){}
        product_t(const uint64_t& pid) : id(pid){}
        
        uint64_t scope() const { return 0; }

        uint64_t primary_key()const { return id; } 

        uint64_t by_owner()const    { return owner.value; }
        uint64_t by_nsy_id() const     { return balance.symbol.id; }
        uint64_t by_parent_id()const    { return balance.symbol.parent_id; }
        uint64_t by_status()const    { return status.value; }

        typedef eosio::multi_index<"products"_n,product_t,
            indexed_by<"owneridx"_n, const_mem_fun<product_t,uint64_t, &product_t::by_owner> >,
            indexed_by<"nsyidx"_n, const_mem_fun<product_t,uint64_t, &product_t::by_nsy_id> >,
            indexed_by<"parentidx"_n, const_mem_fun<product_t,uint64_t, &product_t::by_parent_id> >,
            indexed_by<"statusidx"_n, const_mem_fun<product_t,uint64_t, &product_t::by_status> >
        > tbl_t;

        EOSLIB_SERIALIZE(product_t,(id)(title)(owner)(balance)(total_issue)(price)(status)
                        (rule)(created_at)(updated_at)(sell_started_at)(sell_ended_at)
                        (operable_started_at)(operable_ended_at))
    };

    TBL account_t{

        uint64_t            product_id;
        asset               balance     = asset(0,MUSDT);
        asset               total_claimed_rewards;
        nasset              card;
        name                status = account_status::none;
        time_point_sec      created_at;
        time_point_sec      updated_at;
        time_point_sec      operable_started_at;
        time_point_sec      operable_ended_at;

        account_t(){}
        account_t(const uint64_t& pid) : product_id(pid){}

        uint64_t scope() const { return 0; }
        uint64_t primary_key()const { return product_id; } 
        
        uint64_t by_nsy_id() const     { return card.symbol.id; }
        uint64_t by_status() const     { return status.value; }

        typedef eosio::multi_index<"accounts"_n,account_t,
            indexed_by<"nsyidx"_n, const_mem_fun<account_t,uint64_t, &account_t::by_nsy_id> >,
            indexed_by<"stausidx"_n, const_mem_fun<account_t,uint64_t, &account_t::by_status> >
        > tbl_t;

        EOSLIB_SERIALIZE(account_t,(product_id)(balance)(total_claimed_rewards)(card)(status)(created_at)(updated_at)
                        (operable_started_at)(operable_ended_at)
                        )
    };

    TBL card_t{
        name             owner;
        time_point_sec   created_at;

        card_t(){}
        card_t(const name& o) : owner(o){}

        uint64_t primary_key() const { return owner.value; }
        uint64_t scope() const { return 0; }

        typedef eosio::multi_index<"cards"_n, card_t> tbl_t;
        
        EOSLIB_SERIALIZE(card_t,(owner)(created_at)
                        )
    };

    // struct reward_t{
    //     name owner;
    //     asset quantity;
    //     name type;
    //     reward_t() {}
    //     reward_t(const name& o,const asset& q,const name& t):owner(o),quantity(q),type(t) {}

    //     EOSLIB_SERIALIZE(reward_t,(owner)(quantity)(type)
    //                     )
    // };

    TBL order_t{

        uint64_t            id;
        uint64_t            product_id;
        name                owner;
        asset               quantity;
        nasset              nft_quantity;
        //vector<reward_t>    rewards;
        time_point_sec      created_at;
      

        order_t(){}
        order_t(const uint64_t& pid) : id(pid){}

        uint64_t primary_key() const { return id; }
        uint64_t scope() const { return 0; }

        uint64_t by_pro_id() const { return product_id; }

        typedef eosio::multi_index<"orders"_n, order_t,
             indexed_by<"byproid"_n, const_mem_fun<order_t,uint64_t, &order_t::by_pro_id> >
            > tbl_t;
        
        EOSLIB_SERIALIZE(order_t,(id)(product_id)(owner)(quantity)
                        (nft_quantity)
                        (created_at))
    };

    // struct deal_trace {

    //     uint64_t            order_id;
    //     name                buyer;
    //     asset               quantity;
    //     asset               price;
    //     nasset              nft_quantity;
    //     name                direct_name;
    //     asset               direct_quantity;
    //     name                indirect_name;
    //     asset               indirect_quantity;
    //     name                parent_name;
    //     asset               parent_quantity;
    //     time_point_sec      created_at;

    // };

    struct deal_trace {

        uint64_t            order_id;
        name                buyer;
        name                receiver;
        asset               quantity;
        name                type = reward_type::none;
        time_point_sec      created_at;
    };
}
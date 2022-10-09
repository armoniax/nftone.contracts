#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>
#include <eosio/name.hpp>
#include <amax.ntoken/amax.ntoken_db.hpp>
#include "pass.mart_const.hpp"
#include <string>

#define TBL struct [[eosio::table, eosio::contract("pass.mart")]]

namespace mart{

    using namespace amax;
    using std::string;

    struct [[eosio::table("global"), eosio::contract("pass.mart")]] global_t{

        name                 admin;
        asset                total_sells;
        asset                total_rewards;
        asset                total_claimed_rewards;
        name                 lock_contract ;
        name                 partner_account ;
        name                 nft_contract ;
        name                 storage_account;
        name                 unable_claimrewards_account;
        uint64_t             first_rate ;
        uint64_t             second_rate ;
        uint64_t             partner_rate ;
        uint64_t             claimrewrads_day;
        time_point_sec       started_at;
        uint64_t             last_product_id = 0;
        uint64_t             last_order_id = 0;

        EOSLIB_SERIALIZE( global_t, (admin)(total_sells)(total_rewards)(total_claimed_rewards)
        (lock_contract)(partner_account)(nft_contract)(storage_account)(unable_claimrewards_account)
        (first_rate)(second_rate)(partner_rate)(claimrewrads_day)(started_at)
        (last_product_id)(last_order_id) )
    };

    typedef eosio::singleton< "global"_n, global_t > global_singleton;

    struct rule_t{
        uint64_t onetime_buy_amount = 0;
        bool allow_to_buy_again = true ;
    };
    
    struct gift_t{
        name                contract_name = ""_n;
        nasset              balance       = nasset(0,nsymbol(0,0));
        nasset              total_issue   = nasset(0,nsymbol(0,0));
    };
    
    TBL product_t{
        uint64_t            id = 0;             //PK
        string              title;
        name                owner;
        nasset              balance;
        nasset              total_issue;
        asset               price;
        uint64_t            buy_lock_plan_id = 0;
        name                status = product_status::none;
        rule_t              rule;
        time_point_sec      created_at;
        time_point_sec      updated_at;
        time_point_sec      sell_started_at;
        time_point_sec      sell_ended_at;
        time_point_sec      claimrewards_started_at;
        time_point_sec      claimrewards_ended_at;

        gift_t              gift_nft;               // Purchase pass to obtain ID
        uint64_t            token_split_plan_id;
        product_t(){}
        product_t(const uint64_t& pid) : id(pid){}

        uint64_t scope() const { return 0; }

        uint64_t primary_key()const { return id; }

        uint64_t by_owner()const    { return owner.value; }
        uint64_t by_nsymb_id() const     { return balance.symbol.id; }
        uint64_t by_parent_id()const    { return balance.symbol.parent_id; }
        uint64_t by_status()const    { return status.value; }

        typedef eosio::multi_index<"products"_n,product_t
            //indexed_by<"owneridx"_n, const_mem_fun<product_t,uint64_t, &product_t::by_owner> >,
            //indexed_by<"nsymbidx"_n, const_mem_fun<product_t,uint64_t, &product_t::by_nsymb_id> >,
            //indexed_by<"parentidx"_n, const_mem_fun<product_t,uint64_t, &product_t::by_parent_id> >,
            //indexed_by<"statusidx"_n, const_mem_fun<product_t,uint64_t, &product_t::by_status> >
        > tbl_t;

        EOSLIB_SERIALIZE(product_t,(id)(title)(owner)(balance)(total_issue)(price)(buy_lock_plan_id)(status)
                        (rule)(created_at)(updated_at)(sell_started_at)(sell_ended_at)
                        (claimrewards_started_at)(claimrewards_ended_at)(gift_nft)(token_split_plan_id))
    };

    // scope: owner account
    TBL account_t{

        uint64_t            product_id;                     // PK
        asset               sum_balance = asset(0,MUSDT);   // rewards
        asset               balance     = asset(0,MUSDT);
        nasset              pass;
        name                status = account_status::none;
        time_point_sec      created_at;
        time_point_sec      updated_at;


        account_t(){}
        account_t(const uint64_t& pid) : product_id(pid){}

        uint64_t primary_key()const { return product_id; }

        typedef eosio::multi_index<"accounts"_n,account_t
        > tbl_t;

        EOSLIB_SERIALIZE(account_t,(product_id)(sum_balance)(balance)(pass)(status)(created_at)(updated_at)
                        //(claimrewards_started_at)(claimrewards_ended_at)
                        )
    };

    TBL pass_recv_t{
        name             owner;     //PK
        time_point_sec   created_at;

        pass_recv_t(){}
        pass_recv_t(const name& o) : owner(o){}

        uint64_t primary_key() const { return owner.value; }
        uint64_t scope() const { return 0; }

        typedef eosio::multi_index<"passrecv"_n, pass_recv_t> tbl_t;

        EOSLIB_SERIALIZE(pass_recv_t,(owner)(created_at)
                        )
    };

    struct order_t{

        uint64_t            id;
        uint64_t            product_id;
        name                owner;
        asset               quantity;
        nasset              nft_quantity;
        time_point_sec      created_at;
        nasset              gift_quantity;
    };

    struct deal_trace {

        uint64_t            product_id;
        uint64_t            order_id;
        name                buyer;
        name                receiver;
        asset               quantity;
        name                type = reward_type::none;
        time_point_sec      created_at;
    };
}

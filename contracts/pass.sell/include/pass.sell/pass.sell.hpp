#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/time.hpp>
#include "pass.sell_db.hpp"
#include <thirdparty/wasm_db.hpp>

using namespace eosio;
using namespace std;
using namespace wasm::db;

namespace sell{

    class [[eosio::contract("pass.sell")]] pass_sell : public contract{

        public:
            using contract::contract;
       
            
            pass_sell(eosio::name receiver, eosio::name code, datastream<const char*> ds):
            contract(receiver, code, ds),_db(get_self()),
            _global(get_self(), get_self().value)
            {
                _gstate = _global.exists() ? _global.get() : global_t{};
            }

            ~pass_sell() { _global.set( _gstate, get_self() ); }


            [[eosio::action]]
            void init();

            [[eosio::action]]
            void setclaimday(const name& admin, const uint64_t& days);
            
            [[eosio::action]]
            void setadmin(const name& admin,const name& newAdmin);

            [[eosio::action]]
            void setrule(const name& owner,const uint64_t& product_id,const rule_t& rule);
            
            [[eosio::action]]
            void setaccouts(const name& owner,
                            const name& nft_contract,
                            const name& lock_contract,
                            const name& partner_name,
                            const name& unclaimed_account);
            
            [[eosio::action]]
            void setrates(const name& owner,
                            const uint64_t& first_rate,
                            const uint64_t& second_rate,
                            const uint64_t& partner_rate);

            [[eosio::action]]
            void addproduct( const name& owner, const string& title, const nsymbol& nft_symbol,
                                const asset& price, const time_point_sec& started_at,
                                const time_point_sec& ended_at);
 
            [[eosio::on_notify("passptoken11::transfer")]]
            void nft_transfer(const name& from, const name& to, const vector< nasset >& assets, const string& memo);

            [[eosio::on_notify("amax.mtoken::transfer")]]
            void token_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

            [[eosio::action]]
            void claimrewards( const name& owner, const uint64_t& product_id);
            
            [[eosio::action]]
            void dealtrace(const deal_trace& trace);

            using deal_trace_action = eosio::action_wrapper<"dealtrace"_n, &pass_sell::dealtrace>;
        private:
            global_singleton    _global;
            global_t            _gstate;
            dbc                _db;

            void _check_conf_auth(const name& owner);
            void _reward(const product_t& product, const name& owner, const asset& quantiy, const nasset& nft_quantity);
            //void _add_card( const product_t& product, const name& owner, const nasset& nft_quantity);
            void _add_balance(const product_t& product, const name& owner, const asset& quantity, const nasset& nft_quantity);
            void _on_deal_trace(const uint64_t&orer_id, const name&buyer, const name&receiver, const asset& quantity,const name& type);

    };
}
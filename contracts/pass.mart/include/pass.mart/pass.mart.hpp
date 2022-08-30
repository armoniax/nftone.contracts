#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/time.hpp>
#include "pass.mart_db.hpp"
#include <libraries/wasm_db.hpp>

using namespace eosio;
using namespace std;
using namespace wasm::db;

namespace mart{

    class [[eosio::contract("pass.mart")]] pass_mart : public contract{

        public:
            using contract::contract;


            pass_mart(eosio::name receiver, eosio::name code, datastream<const char*> ds):
            contract(receiver, code, ds),_db(get_self()),
            _global(get_self(), get_self().value)
            {
                _gstate = _global.exists() ? _global.get() : global_t{};
            }

            ~pass_mart() { _global.set( _gstate, get_self() ); }


            [[eosio::action]]
            void init();

            [[eosio::action]]
            void setclaimday( const uint64_t& days);

            [[eosio::action]]
            void setadmin( const name& newAdmin);

            [[eosio::action]]
            void setrule( const uint64_t& product_id,const rule_t& rule);

            [[eosio::action]]
            void setaccouts(const name& nft_contract,
                            const name& lock_contract,
                            const name& partner_account,
                            const name& storage_account,
                            const name& unable_claimrewards_account);

            [[eosio::action]]
            void setrates(  const uint64_t& first_rate,
                            const uint64_t& second_rate,
                            const uint64_t& partner_rate);

            [[eosio::action]]
            void cancelplan(  const uint64_t& product_id);

            // [[eosio::action]]
            // void editclaimtime( const uint64_t& product_id, const time_point_sec& ended_time);
            [[eosio::action]]
            void addproduct( const name& owner, const string& title, const nsymbol& nft_symbol,
                                const asset& price, const time_point_sec& started_at,
                                const time_point_sec& ended_at, uint64_t buy_lock_plan_id);

            [[eosio::on_notify("pass.ntoken::transfer")]]
            void nft_transfer(const name& from, const name& to, const vector< nasset >& assets, const string& memo);

            [[eosio::on_notify("amax.mtoken::transfer")]]
            void token_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

            [[eosio::action]]
            void claimrewards( const name& owner, const uint64_t& product_id);

            [[eosio::action]]
            void dealtrace(const deal_trace& trace);

            [[eosio::action]]
            void ordertrace(const order_t& order);

            using deal_trace_action = eosio::action_wrapper<"dealtrace"_n, &pass_mart::dealtrace>;

            using order_trace_action = eosio::action_wrapper<"ordertrace"_n, &pass_mart::ordertrace>;
        private:
            global_singleton    _global;
            global_t            _gstate;
            dbc                _db;


            void _tally_rewards(const product_t& product, const name& owner, const asset& quantiy, const nasset& nft_quantity);
            void _add_quantity(const product_t& product, const name& owner, const asset& quantity, const nasset& nft_quantity);
            void _creator_reward( const product_t& product, const name& buyer,const name& creator, const asset& quantity, const name& status );
            void _on_deal_trace(const uint64_t&product_id, const name&buyer, const name&receiver, const asset& quantity,const name& type);
            void _on_order_deal_trace( const order_t& order);
    };
}
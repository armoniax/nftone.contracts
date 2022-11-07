   #pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>

#include <string>

#include <nftone.mart/nftone.mart.db.hpp>

namespace amax {

using std::string;
using std::vector;

using namespace eosio;

static constexpr name      NFT_BANK    = "amax.ntoken"_n;
static constexpr name      CNYD_BANK   = "cnyd.token"_n;
static constexpr symbol    CNYD        = symbol(symbol_code("CNYD"), 4);

struct deal_trace_s {
    uint64_t         seller_order_id;
    uint64_t         bid_id;
    name             maker;
    name             buyer;
    price_s          price;
    asset            fee;
    int64_t          count;
    time_point_sec   created_at;
    name             ipowner;
    asset            ipfee;

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
   STATUS_ERROR         = 18

};


/**
 * The `nftone.mart` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `nftone.mart` contract instead of developing their own.
 *
 * The `nftone.mart` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 *
 * The `nftone.mart` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 *
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("nftone.mart")]] nftone_mart : public contract {
   public:
      using contract::contract;

   nftone_mart(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~nftone_mart() { _global.set( _gstate, get_self() ); }

   //Sell
   [[eosio::on_notify("amax.ntoken::transfer")]]
   void on_ntoken_transfer(const name& from, const name& to, const vector<nasset>& quants, const string& memo);
   [[eosio::on_notify("verso.itoken::transfer")]]
   void on_itoken_transfer(const name& from, const name& to, const vector<nasset>& quants, const string& memo);

   //Buy
   [[eosio::on_notify("cnyd.token::transfer")]]
   void on_cnyd_transfer(const name& from, const name& to, const asset& quant, const string& memo);
   [[eosio::on_notify("amax.mtoken::transfer")]]
   void on_mtoken_transfer(const name& from, const name& to, const asset& quant, const string& memo);

   ACTION setfeecollec(const name& dev_fee_collector) {
      require_auth( _self );

      _gstate.dev_fee_collector = dev_fee_collector;
   }
   
   ACTION init(const symbol& pay_symbol, const name& pay_contract, const name& admin,
                              const double& devfeerate, const name& feecollector,
                              const double& ipfeerate);
   ACTION cancelorder(const name& maker, const uint32_t& token_id, const uint64_t& order_id);
   ACTION takebuybid( const name& issuer, const uint32_t& token_id, const uint64_t& buyer_bid_id );
   // ACTION takeselorder( const name& issuer, const uint32_t& token_id, const uint64_t& sell_order_id );
   ACTION cancelbid( const name& buyer, const uint64_t& buyer_bid_id );
   ACTION dealtrace(const deal_trace_s& trace);

   using deal_trace_s_action = eosio::action_wrapper<"dealtrace"_n, &nftone_mart::dealtrace>;

   private:
      global_singleton    _global;
      global_t            _gstate;

   private:

      void compute_memo_price( const string& memo, asset& price );
      void _emit_deal_action(const deal_trace_s& trace);
      void process_single_buy_order(const name& buyer, order_t& order, asset& quantity, nasset& bought, uint64_t& deal_count, asset& devfee, name& ipowner, asset& ipfee);
      void _settle_maker(const name& buyer, const name& maker, asset& earned, nasset& bought, asset& devfee, const name& ipowner, asset& ipfee);
      void _reward_farmer( const asset& fee, const name& farmer );
      void _sell_transfer(const name& from, const name& to, const vector<nasset>& quants, const string& memo);
      void _buy_transfer(const name& from, const name& to, const asset& quant, const string& memo);

};
} //namespace amax

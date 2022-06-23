#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>

#include <string>

#include <nftone.mart/nftone.mart_db.hpp>

namespace amax {

using std::string;
using std::vector;

using namespace eosio;

static constexpr name      NFT_BANK    = "amax.ntoken"_n;
static constexpr name      CNYD_BANK   = "cnyd.token"_n;
static constexpr name      MTOKEN_BANK = "amax.mtoken"_n;
static constexpr symbol    CNYD        = symbol(symbol_code("CNYD"), 4);

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

   [[eosio::on_notify("amax.ntoken::transfer")]]
   void onselltransfer(const name& from, const name& to, const vector<nasset>& quants, const string& memo);

   [[eosio::on_notify("cnyd.token::transfer")]]
   void onbuytransfer(const name& from, const name& to, const asset& quant, const string& memo);

   ACTION init();
   ACTION cancelorder(const name& maker, const uint32_t& token_id, const uint64_t& order_id);
   ACTION takebuybid( const name& issuer, const uint32_t& token_id, const uint64_t& buyer_bid_id );
   // ACTION takeselorder( const name& issuer, const uint32_t& token_id, const uint64_t& sell_order_id );
   ACTION cancelbid( const name& buyer, const uint64_t& buyer_bid_id );
   
   private:
      global_singleton    _global;
      global_t            _gstate;

   private:
      void process_single_buy_order( order_t& order, asset& quantity, nasset& bought );
      void compute_memo_price( const string& memo, float& price );
      void transfer_token(const name &from, const asset &quantity, const string &memo);
};
} //namespace amax

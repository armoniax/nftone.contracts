#include "pass.custodydb.hpp"
#include <amax.ntoken/amax.ntoken.hpp>

using namespace std;
using namespace wasm::db;

class [[eosio::contract("pass.custody")]] custody: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    custody(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~custody() { _global.set( _gstate, get_self() ); }

    ACTION init();
    ACTION setconfig(const asset &plan_fee, const name &fee_receiver);
    ACTION setplanowner(const name& owner, const uint64_t& plan_id, const name& new_owner);

    ACTION addplan(const name& owner, const string& title, const name& asset_contract, const nsymbol& asset_symbol, const uint64_t& unlock_interval_days, const int64_t& unlock_times);


    /**
     * ontokentrans, trigger by recipient of amax.token::transfer()
     *  @param from - plan owner
     *  @param to   - must be contract self
     *  @param quantity - quantity
     *  @param memo - memo format:
     *      1. plan:${plan_id}, pay plan fee, Eg: "plan:" or "plan:1"
     *          @description pay plan fee
     *          @param plan_id - plan id. if null, represents the last added plan of the `from` account
     *
     */
    [[eosio::on_notify("amax.token::transfer")]] 
    void ontokentrans(const name& from, const name& to, const asset& quantity, const string& memo);
    /**
     * onnfttrans, trigger by recipient of pass.ntoken::transfer()
     *  @param from - locker
     *  @param to   - must be contract self
     *  @param assets - assets, size must be 1
     *  @param memo - memo format:
     *      1. lock:${receiver}:${plan_id}:${first_unlock_days}, Eg: "lock:receiver1234:1:30"
     *          @description new lock
     *          @param receiver - owner name
     *          @param plan_id - plan id
     *          @param first_unlock_days - first unlock days after created, range: [0, MAX_LOCK_DAYS)
     */
    [[eosio::on_notify("pass.ntoken::transfer")]] 
    void onnfttrans(const name& from, const name& to, const vector<nasset>& assets, const string& memo);

    /**
     * @brief send mid token in and out in order to move pass.ntoken from mid token owner to another
     * 
     * @param from 
     * @param to 
     * @param assets 
     * @param memo - format:
     *                  move:$from_lock_id:$to_account
     *                  @parm from_lock_id - lock owned by from, its quantity must be smaller than recd
     */
    [[eosio::on_notify("verso.itoken::transfer")]] 
    void onmidtrans(const name& from, const name& to, const vector<nasset>& assets, const string& memo);

    ACTION setmovwindow( const uint64_t& plan_id, const nsymbol& symbol, const time_point_sec& started_at, const time_point_sec& finished_at );
    
    ACTION unlock(const name& locker, const uint64_t& plan_id, const uint64_t& lock_id);
    /**
     * @require run by locker only
     */
    ACTION endlock(const name& locker, const uint64_t& plan_id, const uint64_t& lock_id);

    /**
     * @brief Set up a warehouse transfer whitelist
     * 
     * @param owner 
     * @param to_add 
     * @return ACTION 
     */
    ACTION whitelist( const name& owner, const bool& to_add);

    ACTION movetrace(const move_log_s& trace);
    using move_trace_action = eosio::action_wrapper<"movetrace"_n, &custody::movetrace>;
private:
    void _unlock(const name& actor, const uint64_t& plan_id, const uint64_t& lock_id, bool to_terminate);
    void _on_move_trace( const move_log_s& trace);
}; //contract pass.custody
#include "rndnft.mart.db.hpp"
#include <amax.ntoken/amax.ntoken.hpp>

using namespace std;
using namespace wasm::db;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

class [[eosio::contract("rndnft.mart")]] rndnft_mart: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

    global1_singleton   _global1;
    global1_t           _gstate1;

    dbc                 _db;

public:
    using contract::contract;

    rndnft_mart(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
        _global(get_self(), get_self().value),_db(get_self()),_global1(_self, _self.value) {
        _gstate = _global.exists() ? _global.get() : global_t{};
        _gstate1 = _global1.exists() ? _global1.get() : global1_t{};
    }

    ~rndnft_mart() { 
        _global.set( _gstate, get_self() ); 
        _global1.set( _gstate1, get_self() ); 
    }

    // ACTION init() { _gstate.last_booth_id = 2; _gstate.admin = "nftone.admin"_n; _gstate.fund_distributor = "amax.split"_n; }

    ACTION createbooth( const name& owner,const string& title, const name& nft_contract, const name& fund_contract,
                        const uint64_t& split_plan_id, const asset& price, const time_point_sec& opened_at, const uint64_t& duration_days);
    ACTION enablebooth( const name& owner, const uint64_t& booth_id, bool enabled);
    ACTION setboothtime( const name& owner, const uint64_t& booth_id, const time_point_sec& opened_at, const time_point_sec& closed_at);
    ACTION closebooth( const name& owner, const uint64_t& booth_id);
    ACTION dealtrace( const deal_trace_s_s& trace);

    ACTION delboothbox( const uint64_t& booth_id, const nsymbol& nsymb) {
        require_auth( _self );

        booth_nftbox_t boothboxes( booth_id );
        check( _db.get( boothboxes ), "err: booth box not found" );

        boothboxes.nfts.erase( nsymb );

        _db.set( boothboxes );
    }

    ACTION setmaxbox(const uint64_t& max_num) {
        require_auth( _self );

        _gstate.max_booth_boxes = max_num;
    }
    
    ACTION fixboothbox( const uint64_t& booth_id, const nasset& nfts) {
        require_auth( _self );

        booth_nftbox_t boothboxes( booth_id );
        check( _db.get( boothboxes ), "err: booth box not found" );

        boothboxes.nfts[nfts.symbol] = nfts.amount;
        _db.set( boothboxes );
    }

    /**
     * @brief booth owner to send nft into one's own booth
     * 
     * @param from 
     * @param to 
     * @param assets 
     * @param memo 
     */
    [[eosio::on_notify("*::transfer")]] 
    void on_transfer_ntoken(const name& from, const name& to, const vector<nasset>& assets, const string& memo);

    /**
     * @brief buyer to buy booth nft
     * 
     * @param from 
     * @param to 
     * @param quantity 
     * @param memo 
     */
    [[eosio::on_notify("amax.mtoken::transfer")]] 
    void on_transfer_mtoken( const name& from, const name& to, const asset& quantity, const string& memo );
    
    using deal_trace_s_action = eosio::action_wrapper<"dealtrace"_n, &rndnft_mart::dealtrace>;

private:
    uint64_t _rand(const uint64_t& max_uint, const name& owner, const uint64_t& index);
    void _one_nft( const name& owner, const uint64_t& index, booth_t& booth, nasset& nft );
    void _on_deal_trace_s( const deal_trace_s_s& deal_trace_s);
    void _add_times( const uint64_t& booth_id, const name& owner);
    void _reward_farmer( const asset& xin_quantity, const name& farmer );
};